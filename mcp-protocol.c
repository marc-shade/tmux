/* $OpenBSD$ */

/*
 * Copyright (c) 2025 Marc Shade <marc.alan.shade@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"
#include "mcp-client.h"

/*
 * MCP Protocol Extensions
 * Implements proper MCP protocol handshake and advanced features
 */

/* MCP protocol version */
#define MCP_PROTOCOL_VERSION "2024-11-05"
#define MCP_CLIENT_NAME "tmux-mcp-client"
#define MCP_CLIENT_VERSION "1.0.0"

/* Connection retry configuration */
#define MCP_MAX_RETRIES 3
#define MCP_RETRY_DELAY 1  /* seconds */

/*
 * Send MCP initialize request
 * This establishes protocol version and capabilities with the MCP server
 */
int
mcp_protocol_initialize(struct mcp_connection *conn)
{
	char			*request, *params;
	char			 buffer[MCP_MAX_MESSAGE_SIZE];
	struct mcp_response	*resp;
	ssize_t			 n;
	size_t			 params_len;

	if (conn == NULL || conn->state != MCP_CONNECTED)
		return (-1);

	/* Build initialize params according to MCP spec */
	params_len = 512;
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"protocolVersion\":\"%s\","
	    "\"clientInfo\":{"
	    "\"name\":\"%s\","
	    "\"version\":\"%s\""
	    "},"
	    "\"capabilities\":{"
	    "\"roots\":{\"listChanged\":false},"
	    "\"sampling\":{},"
	    "\"experimental\":{}"
	    "}}",
	    MCP_PROTOCOL_VERSION,
	    MCP_CLIENT_NAME,
	    MCP_CLIENT_VERSION);

	/* Build JSON-RPC request */
	request = mcp_build_request(conn->request_id++, "initialize", params);
	free(params);

	if (request == NULL)
		return (-1);

	/* Send initialize request */
	n = mcp_send(conn, request, strlen(request));
	free(request);

	if (n < 0) {
		conn->errors++;
		return (-1);
	}

	conn->requests_sent++;
	conn->last_activity = time(NULL);

	/* Read initialize response */
	memset(buffer, 0, sizeof buffer);
	n = mcp_recv(conn, buffer, sizeof buffer);
	if (n < 0) {
		conn->errors++;
		return (-1);
	}

	conn->responses_received++;
	conn->last_activity = time(NULL);

	/* Parse response */
	resp = mcp_parse_response(buffer);
	if (resp == NULL || !resp->success) {
		if (resp != NULL)
			mcp_response_free(resp);
		return (-1);
	}

	mcp_response_free(resp);

	/* Send initialized notification (required by MCP spec) */
	request = mcp_build_request(conn->request_id++, "notifications/initialized", "{}");
	if (request != NULL) {
		mcp_send(conn, request, strlen(request));
		free(request);
	}

	log_debug("MCP: initialized connection to %s", conn->config->name);
	return (0);
}

/*
 * Connect to MCP server with retries
 * Attempts connection up to MCP_MAX_RETRIES times with exponential backoff
 */
int
mcp_connect_with_retry(struct mcp_client *client, const char *server_name)
{
	struct mcp_connection	*conn;
	int			 attempt, rc;
	unsigned int		 delay;

	conn = mcp_find_connection(client, server_name);
	if (conn == NULL)
		return (-1);

	for (attempt = 0; attempt < MCP_MAX_RETRIES; attempt++) {
		/* Try to connect */
		rc = mcp_connect_server(client, server_name);
		if (rc == 0) {
			/* Connection successful, initialize protocol */
			rc = mcp_protocol_initialize(conn);
			if (rc == 0) {
				log_debug("MCP: connected to %s on attempt %d",
				    server_name, attempt + 1);
				return (0);
			}

			/* Initialize failed, disconnect and retry */
			mcp_disconnect_server(client, server_name);
		}

		/* Connection or initialization failed */
		if (attempt < MCP_MAX_RETRIES - 1) {
			/* Exponential backoff: 1s, 2s, 4s */
			delay = MCP_RETRY_DELAY << attempt;
			log_debug("MCP: connection attempt %d failed, "
			    "retrying in %u seconds", attempt + 1, delay);
			sleep(delay);
		}
	}

	log_debug("MCP: failed to connect to %s after %d attempts",
	    server_name, MCP_MAX_RETRIES);
	conn->state = MCP_ERROR;
	return (-1);
}

/*
 * Check if connection needs refresh
 * Returns 1 if connection should be re-established
 */
int
mcp_connection_stale(struct mcp_connection *conn)
{
	time_t	now, idle_time;

	if (conn == NULL)
		return (1);

	if (conn->state != MCP_CONNECTED)
		return (1);

	/* Check if connection has been idle too long (5 minutes) */
	now = time(NULL);
	idle_time = now - conn->last_activity;
	if (idle_time > 300)
		return (1);

	/* Check error rate */
	if (conn->requests_sent > 0) {
		double error_rate = (double)conn->errors / conn->requests_sent;
		if (error_rate > 0.5)  /* More than 50% errors */
			return (1);
	}

	return (0);
}

/*
 * Enhanced tool calling with automatic retry
 */
struct mcp_response *
mcp_call_tool_safe(struct mcp_client *client, const char *server_name,
    const char *tool_name, const char *arguments)
{
	struct mcp_connection	*conn;
	struct mcp_response	*resp;
	int			 retry;

	conn = mcp_find_connection(client, server_name);
	if (conn == NULL)
		return (NULL);

	/* Try up to 2 times (initial + 1 retry) */
	for (retry = 0; retry < 2; retry++) {
		/* Ensure connection is healthy */
		if (conn->state != MCP_CONNECTED ||
		    mcp_connection_stale(conn)) {
			/* Reconnect with proper initialization */
			if (mcp_connect_with_retry(client, server_name) < 0)
				continue;
		}

		/* Try to call the tool */
		resp = mcp_call_tool(client, server_name, tool_name, arguments);
		if (resp != NULL)
			return (resp);

		/* First attempt failed, try reconnecting once */
		if (retry == 0) {
			log_debug("MCP: tool call failed, attempting reconnect");
			mcp_disconnect_server(client, server_name);
		}
	}

	return (NULL);
}

/*
 * Get connection statistics
 */
void
mcp_connection_stats(struct mcp_connection *conn, char *buf, size_t len)
{
	time_t	now, uptime, idle;
	double	success_rate;

	if (conn == NULL || buf == NULL)
		return;

	now = time(NULL);
	uptime = conn->connected_at > 0 ? now - conn->connected_at : 0;
	idle = conn->last_activity > 0 ? now - conn->last_activity : 0;

	success_rate = conn->requests_sent > 0 ?
	    (double)(conn->requests_sent - conn->errors) / conn->requests_sent * 100.0 : 0.0;

	snprintf(buf, len,
	    "State: %s, Uptime: %lds, Idle: %lds, "
	    "Requests: %u, Responses: %u, Errors: %u, Success: %.1f%%",
	    mcp_state_string(conn->state),
	    (long)uptime, (long)idle,
	    conn->requests_sent, conn->responses_received, conn->errors,
	    success_rate);
}

/*
 * List resources from MCP server
 */
struct mcp_response *
mcp_list_resources(struct mcp_client *client, const char *server_name)
{
	struct mcp_connection	*conn;
	struct mcp_response	*resp;
	char			*request;
	char			 buffer[MCP_MAX_MESSAGE_SIZE];
	ssize_t			 n;

	conn = mcp_find_connection(client, server_name);
	if (conn == NULL)
		return (NULL);

	if (conn->state != MCP_CONNECTED) {
		if (mcp_connect_with_retry(client, server_name) < 0)
			return (NULL);
	}

	/* Build request for resources/list */
	request = mcp_build_request(conn->request_id++, "resources/list", NULL);
	if (request == NULL)
		return (NULL);

	/* Send request */
	n = mcp_send(conn, request, strlen(request));
	free(request);

	if (n < 0) {
		conn->errors++;
		return (NULL);
	}

	conn->requests_sent++;
	conn->last_activity = time(NULL);

	/* Read response */
	memset(buffer, 0, sizeof buffer);
	n = mcp_recv(conn, buffer, sizeof buffer);
	if (n < 0) {
		conn->errors++;
		return (NULL);
	}

	conn->responses_received++;
	conn->last_activity = time(NULL);

	/* Parse response */
	resp = mcp_parse_response(buffer);
	return (resp);
}

/*
 * Read resource from MCP server
 */
struct mcp_response *
mcp_read_resource(struct mcp_client *client, const char *server_name,
    const char *uri)
{
	struct mcp_connection	*conn;
	struct mcp_response	*resp;
	char			*request, *params;
	char			 buffer[MCP_MAX_MESSAGE_SIZE];
	ssize_t			 n;
	size_t			 params_len;

	conn = mcp_find_connection(client, server_name);
	if (conn == NULL || uri == NULL)
		return (NULL);

	if (conn->state != MCP_CONNECTED) {
		if (mcp_connect_with_retry(client, server_name) < 0)
			return (NULL);
	}

	/* Build params */
	params_len = 256 + strlen(uri);
	params = xmalloc(params_len);
	snprintf(params, params_len, "{\"uri\":\"%s\"}", uri);

	/* Build request for resources/read */
	request = mcp_build_request(conn->request_id++, "resources/read", params);
	free(params);

	if (request == NULL)
		return (NULL);

	/* Send request */
	n = mcp_send(conn, request, strlen(request));
	free(request);

	if (n < 0) {
		conn->errors++;
		return (NULL);
	}

	conn->requests_sent++;
	conn->last_activity = time(NULL);

	/* Read response */
	memset(buffer, 0, sizeof buffer);
	n = mcp_recv(conn, buffer, sizeof buffer);
	if (n < 0) {
		conn->errors++;
		return (NULL);
	}

	conn->responses_received++;
	conn->last_activity = time(NULL);

	/* Parse response */
	resp = mcp_parse_response(buffer);
	return (resp);
}
