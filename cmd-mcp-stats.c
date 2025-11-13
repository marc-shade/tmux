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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * Display MCP performance statistics for all servers or a specific server.
 */

static enum cmd_retval	cmd_mcp_stats_exec(struct cmd *, struct cmdq_item *);
static void		cmd_mcp_stats_show_server(struct cmdq_item *,
			    struct mcp_connection *, int);

const struct cmd_entry cmd_mcp_stats_entry = {
	.name = "mcp-stats",
	.alias = NULL,

	.args = { "", 0, 1, NULL },
	.usage = "[server-name]",

	.flags = 0,
	.exec = cmd_mcp_stats_exec
};

static void
cmd_mcp_stats_show_server(struct cmdq_item *item, struct mcp_connection *conn,
    int detailed)
{
	time_t		now, uptime;
	const char	*state_str, *transport_str;
	u_int		total_requests, success_rate;
	char		uptime_str[64], activity_str[64];
	struct mcp_socket_conn *sock;

	if (conn == NULL || conn->config == NULL)
		return;

	now = time(NULL);

	/* Calculate uptime and format activity time */
	if (conn->state == MCP_CONNECTED && conn->connected_at > 0) {
		uptime = now - conn->connected_at;
		if (uptime < 60)
			snprintf(uptime_str, sizeof uptime_str, "%ld seconds",
			    (long)uptime);
		else if (uptime < 3600)
			snprintf(uptime_str, sizeof uptime_str, "%ld minutes",
			    (long)(uptime / 60));
		else
			snprintf(uptime_str, sizeof uptime_str,
			    "%ld hours, %ld minutes",
			    (long)(uptime / 3600), (long)((uptime % 3600) / 60));
	} else {
		strlcpy(uptime_str, "N/A", sizeof uptime_str);
	}

	/* Format last activity time */
	if (conn->last_activity > 0) {
		time_t	since_activity = now - conn->last_activity;
		if (since_activity < 60)
			snprintf(activity_str, sizeof activity_str,
			    "%ld seconds ago", (long)since_activity);
		else if (since_activity < 3600)
			snprintf(activity_str, sizeof activity_str,
			    "%ld minutes ago", (long)(since_activity / 60));
		else
			snprintf(activity_str, sizeof activity_str,
			    "%ld hours ago", (long)(since_activity / 3600));
	} else {
		strlcpy(activity_str, "never", sizeof activity_str);
	}

	/* Get state and transport strings */
	state_str = mcp_state_string(conn->state);
	transport_str = (conn->config->transport == MCP_TRANSPORT_SOCKET) ?
	    "socket" : "stdio";

	/* Calculate success rate */
	total_requests = conn->requests_sent;
	if (total_requests > 0) {
		success_rate = (conn->responses_received * 100) / total_requests;
	} else {
		success_rate = 0;
	}

	/* Print server header */
	cmdq_print(item, "");
	cmdq_print(item, "Server: %s", conn->config->name);
	cmdq_print(item, "  Transport: %s", transport_str);
	cmdq_print(item, "  Status: %s", state_str);

	/* Print connection-specific details */
	if (conn->config->transport == MCP_TRANSPORT_SOCKET) {
		cmdq_print(item, "  Socket Path: %s",
		    conn->config->socket_path ? conn->config->socket_path :
		    "unknown");
		if (conn->socket_fd >= 0)
			cmdq_print(item, "  Socket FD: %d", conn->socket_fd);
	} else {
		cmdq_print(item, "  Command: %s",
		    conn->config->command ? conn->config->command : "unknown");
		if (conn->server_pid > 0)
			cmdq_print(item, "  Server PID: %d", conn->server_pid);
	}

	/* Print uptime and activity */
	if (conn->state == MCP_CONNECTED) {
		cmdq_print(item, "  Uptime: %s", uptime_str);
		cmdq_print(item, "  Last Activity: %s", activity_str);
	}

	/* Print statistics */
	cmdq_print(item, "  Statistics:");
	cmdq_print(item, "    Requests Sent: %u", conn->requests_sent);
	cmdq_print(item, "    Responses Received: %u",
	    conn->responses_received);
	cmdq_print(item, "    Errors: %u", conn->errors);
	cmdq_print(item, "    Success Rate: %u%%", success_rate);

	/* Print latency statistics if available (placeholder for future) */
	if (detailed) {
		cmdq_print(item, "  Performance:");
		cmdq_print(item, "    Average Latency: N/A (not yet implemented)");
		cmdq_print(item, "    P95 Latency: N/A (not yet implemented)");
		cmdq_print(item, "    P99 Latency: N/A (not yet implemented)");
	}

	/* Print socket-specific statistics */
	if (conn->config->transport == MCP_TRANSPORT_SOCKET &&
	    conn->socket_fd >= 0) {
		/* Note: This would require access to mcp_socket_conn structure
		 * For now, we show basic info */
		cmdq_print(item, "  Socket Statistics:");
		cmdq_print(item, "    Connection Active: yes");
		cmdq_print(item, "    Bytes Sent: N/A (requires socket stats)");
		cmdq_print(item, "    Bytes Received: N/A (requires socket stats)");
	}

	/* Print buffer statistics for stdio transport */
	if (conn->config->transport == MCP_TRANSPORT_STDIO) {
		if (conn->read_buffer != NULL) {
			cmdq_print(item, "  Buffer Statistics:");
			cmdq_print(item, "    Buffer Size: %zu bytes",
			    conn->read_buffer_size);
			cmdq_print(item, "    Buffer Used: %zu bytes",
			    conn->read_buffer_len);
			cmdq_print(item, "    Buffer Free: %zu bytes",
			    conn->read_buffer_size - conn->read_buffer_len);
		}
	}

	/* Print health status */
	if (mcp_connection_healthy(conn)) {
		cmdq_print(item, "  Health: healthy");
	} else {
		cmdq_print(item, "  Health: degraded or disconnected");
	}
}

static enum cmd_retval
cmd_mcp_stats_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	const char		*server_name;
	struct mcp_connection	*conn;
	int			i, found;

	/* Ensure global MCP client exists */
	if (global_mcp_client == NULL) {
		cmdq_error(item, "MCP client not initialized");
		return (CMD_RETURN_ERROR);
	}

	/* Check if a specific server was requested */
	server_name = (args_count(args) > 0) ? args_string(args, 0) : NULL;

	if (server_name != NULL) {
		/* Show stats for a specific server */
		conn = mcp_find_connection(global_mcp_client, server_name);
		if (conn == NULL) {
			cmdq_error(item, "server '%s' not found", server_name);
			return (CMD_RETURN_ERROR);
		}

		cmdq_print(item, "MCP Performance Statistics");
		cmdq_print(item, "==========================");
		cmd_mcp_stats_show_server(item, conn, 1);
	} else {
		/* Show stats for all servers */
		if (global_mcp_client->num_connections == 0) {
			cmdq_print(item, "No MCP servers configured");
			return (CMD_RETURN_NORMAL);
		}

		cmdq_print(item, "MCP Performance Statistics");
		cmdq_print(item, "==========================");
		cmdq_print(item, "Total Servers: %d",
		    global_mcp_client->num_connections);

		found = 0;
		for (i = 0; i < global_mcp_client->num_connections; i++) {
			conn = global_mcp_client->connections[i];
			if (conn != NULL) {
				cmd_mcp_stats_show_server(item, conn, 0);
				found++;
			}
		}

		if (found == 0) {
			cmdq_print(item, "");
			cmdq_print(item, "No active connections");
		}
	}

	return (CMD_RETURN_NORMAL);
}
