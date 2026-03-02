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
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUDAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"
#include "mcp-client.h"

static char		*mcp_build_request(int, const char *, const char *);
static struct mcp_response *mcp_parse_response(const char *);
static int		 mcp_do_handshake(struct mcp_connection *);
static int		 mcp_grow_read_buffer(struct mcp_connection *);
static ssize_t		 mcp_recv(struct mcp_connection *, char *, size_t);
static ssize_t		 mcp_send(struct mcp_connection *, const char *, size_t);
static int		 mcp_write_all(int, const char *, size_t);
static void		 mcp_server_config_free(struct mcp_server_config *);

/* JSON-RPC helper: Build request */
static char *
mcp_build_request(int request_id, const char *method, const char *params)
{
	char	*request;
	size_t	 len;

	/* Calculate required buffer size */
	len = 256 + (params ? strlen(params) : 0);
	request = xmalloc(len);

	/* Build JSON-RPC 2.0 request */
	if (params != NULL && params[0] != '\0')
		snprintf(request, len,
		    "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":%s}",
		    request_id, method, params);
	else
		snprintf(request, len,
		    "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\"}",
		    request_id, method);

	return (request);
}

/* JSON-RPC helper: Parse response (basic implementation) */
static struct mcp_response *
mcp_parse_response(const char *json)
{
	struct mcp_response	*resp;
	const char		*p, *result_start, *error_start;
	size_t			 len;

	resp = xcalloc(1, sizeof *resp);

	/* Look for "result" or "error" in JSON response */
	result_start = strstr(json, "\"result\":");
	error_start = strstr(json, "\"error\":");

	if (result_start != NULL) {
		/* Success case - extract result */
		resp->success = 1;
		result_start += 9; /* Skip "result": */

		/* Skip whitespace */
		while (*result_start == ' ' || *result_start == '\n')
			result_start++;

		/* Find end of result (simple approach: find closing }) */
		p = result_start;
		if (*p == '{') {
			int depth = 1;
			p++;
			while (*p && depth > 0) {
				if (*p == '{')
					depth++;
				else if (*p == '}')
					depth--;
				p++;
			}
		} else if (*p == '[') {
			int depth = 1;
			p++;
			while (*p && depth > 0) {
				if (*p == '[')
					depth++;
				else if (*p == ']')
					depth--;
				p++;
			}
		} else {
			/* Simple value - find comma or closing brace */
			while (*p && *p != ',' && *p != '}')
				p++;
		}

		len = p - result_start;
		resp->result = xmalloc(len + 1);
		memcpy(resp->result, result_start, len);
		resp->result[len] = '\0';

	} else if (error_start != NULL) {
		/* Error case */
		resp->success = 0;
		resp->error_code = -1;

		/* Extract error message (simple approach) */
		error_start = strstr(error_start, "\"message\":");
		if (error_start != NULL) {
			error_start += 10; /* Skip "message": */
			/* Skip whitespace and opening quote */
			while (*error_start == ' ' || *error_start == '"')
				error_start++;

			/* Find closing quote */
			p = strchr(error_start, '"');
			if (p != NULL) {
				len = p - error_start;
				resp->error_message = xmalloc(len + 1);
				memcpy(resp->error_message, error_start, len);
				resp->error_message[len] = '\0';
			}
		}

		if (resp->error_message == NULL)
			resp->error_message = xstrdup("Unknown error");
	} else {
		/* Malformed response */
		resp->success = 0;
		resp->error_code = -1;
		resp->error_message = xstrdup("Malformed JSON response");
	}

	return (resp);
}

/* Free response structure */
void
mcp_response_free(struct mcp_response *resp)
{
	if (resp == NULL)
		return;

	free(resp->result);
	free(resp->error_message);
	free(resp);
}

/* Create MCP client */
struct mcp_client *
mcp_client_create(void)
{
	struct mcp_client	*client;

	client = xcalloc(1, sizeof *client);
	client->num_connections = 0;
	client->initialized = 0;

	return (client);
}

/* Initialize MCP client */
int
mcp_client_init(struct mcp_client *client)
{
	if (client == NULL)
		return (-1);

	/* Ignore SIGPIPE so writes to dead MCP children don't kill tmux. */
	signal(SIGPIPE, SIG_IGN);

	client->initialized = 1;
	return (0);
}

/* Free an MCP server config including its args array. */
static void
mcp_server_config_free(struct mcp_server_config *config)
{
	int	i;

	if (config == NULL)
		return;

	free(config->name);
	free(config->socket_path);
	free(config->command);

	if (config->args != NULL) {
		for (i = 0; config->args[i] != NULL; i++)
			free(config->args[i]);
		free(config->args);
	}

	free(config);
}

/* Destroy MCP client */
void
mcp_client_destroy(struct mcp_client *client)
{
	int			 i;
	struct mcp_connection	*conn;

	if (client == NULL)
		return;

	/* Disconnect and free all connections */
	for (i = 0; i < client->num_connections; i++) {
		conn = client->connections[i];
		if (conn == NULL)
			continue;

		/* Close fds and reap children */
		mcp_disconnect_server(client, conn->config->name);

		mcp_server_config_free(conn->config);
		free(conn);
	}

	free(client);
}

/* Add MCP server configuration */
int
mcp_add_server(struct mcp_client *client, struct mcp_server_config *config)
{
	struct mcp_connection	*conn;

	if (client == NULL || config == NULL)
		return (-1);

	if (client->num_connections >= MCP_MAX_SERVERS)
		return (-1);

	/* Create connection structure */
	conn = xcalloc(1, sizeof *conn);
	conn->config = config;
	conn->state = MCP_DISCONNECTED;
	conn->request_id = 1;

	/* Initialize transport-specific fields */
	conn->socket_fd = -1;
	conn->server_pid = -1;
	conn->stdin_fd = -1;
	conn->stdout_fd = -1;
	conn->read_buffer = NULL;
	conn->read_buffer_len = 0;
	conn->read_buffer_size = 0;

	client->connections[client->num_connections++] = conn;

	return (0);
}

/* Connect via stdio transport (spawn process) */
int
mcp_connect_stdio(struct mcp_connection *conn)
{
	int	stdin_pipe[2], stdout_pipe[2];
	pid_t	pid;
	int	flags;

	if (conn == NULL || conn->config == NULL)
		return (-1);

	if (conn->config->command == NULL)
		return (-1);

	/* Already connected */
	if (conn->state == MCP_CONNECTED && conn->server_pid > 0)
		return (0);

	/* Create pipes for stdin/stdout */
	if (pipe(stdin_pipe) < 0)
		return (-1);

	if (pipe(stdout_pipe) < 0) {
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		return (-1);
	}

	/* Fork to spawn server process */
	conn->state = MCP_CONNECTING;
	pid = fork();

	if (pid < 0) {
		/* Fork failed */
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		conn->state = MCP_ERROR;
		conn->errors++;
		return (-1);
	}

	if (pid == 0) {
		/* Child process: exec server */

		/* Redirect stdin from pipe */
		dup2(stdin_pipe[0], STDIN_FILENO);
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);

		/* Redirect stdout to pipe */
		dup2(stdout_pipe[1], STDOUT_FILENO);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);

		/* Redirect stderr to /dev/null */
		{
			int devnull = open("/dev/null", O_WRONLY);
			if (devnull >= 0) {
				dup2(devnull, STDERR_FILENO);
				close(devnull);
			}
		}

		/* Execute server command */
		if (conn->config->args != NULL)
			execv(conn->config->command, conn->config->args);
		else
			execl(conn->config->command, conn->config->command,
			    (char *)NULL);

		/* If exec fails, exit */
		_exit(1);
	}

	/* Parent process: setup connection */
	close(stdin_pipe[0]);   /* Close read end of stdin pipe */
	close(stdout_pipe[1]);  /* Close write end of stdout pipe */

	conn->stdin_fd = stdin_pipe[1];   /* Write to server's stdin */
	conn->stdout_fd = stdout_pipe[0]; /* Read from server's stdout */
	conn->server_pid = pid;

	/* Set stdout to non-blocking */
	flags = fcntl(conn->stdout_fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(conn->stdout_fd, F_SETFL, flags | O_NONBLOCK);

	/* Initialize read buffer */
	conn->read_buffer_size = 4096;
	conn->read_buffer = xmalloc(conn->read_buffer_size);
	conn->read_buffer_len = 0;

	conn->state = MCP_CONNECTED;
	conn->connected_at = time(NULL);
	conn->last_activity = time(NULL);

	/* Perform MCP initialize handshake */
	if (mcp_do_handshake(conn) < 0) {
		conn->state = MCP_ERROR;
		return (-1);
	}

	return (0);
}

/*
 * Perform MCP protocol handshake: send initialize, receive response,
 * send initialized notification.
 */
static int
mcp_do_handshake(struct mcp_connection *conn)
{
	char		*request;
	char		 buffer[MCP_MAX_MESSAGE_SIZE];
	ssize_t		 n;
	const char	*init_params =
	    "{\"protocolVersion\":\"2024-11-05\","
	    "\"capabilities\":{},"
	    "\"clientInfo\":{\"name\":\"tmux\",\"version\":\"next-3.6\"}}";

	/* Send initialize request */
	request = mcp_build_request(conn->request_id++, "initialize",
	    init_params);
	n = mcp_send(conn, request, strlen(request));
	free(request);

	if (n < 0)
		return (-1);

	/* Read initialize response */
	memset(buffer, 0, sizeof buffer);
	n = mcp_recv(conn, buffer, sizeof buffer);
	if (n < 0)
		return (-1);

	/* Verify we got a result (not an error) */
	if (strstr(buffer, "\"error\"") != NULL)
		return (-1);

	/* Send initialized notification (no id — it's a notification) */
	request = xstrdup("{\"jsonrpc\":\"2.0\","
	    "\"method\":\"notifications/initialized\"}");
	n = mcp_send(conn, request, strlen(request));
	free(request);

	if (n < 0)
		return (-1);

	return (0);
}

/* Connect via socket transport */
int
mcp_connect_socket(struct mcp_connection *conn)
{
	struct sockaddr_un	 addr;
	int			 sock_fd;

	if (conn == NULL || conn->config == NULL)
		return (-1);

	if (conn->config->socket_path == NULL)
		return (-1);

	/* Already connected */
	if (conn->state == MCP_CONNECTED && conn->socket_fd >= 0)
		return (0);

	/* Create Unix socket */
	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		conn->state = MCP_ERROR;
		conn->errors++;
		return (-1);
	}

	/* Set up socket address */
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, conn->config->socket_path,
	    sizeof addr.sun_path);

	/* Connect to server */
	conn->state = MCP_CONNECTING;
	if (connect(sock_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(sock_fd);
		conn->state = MCP_ERROR;
		conn->errors++;
		return (-1);
	}

	/* Connected successfully */
	conn->socket_fd = sock_fd;
	conn->state = MCP_CONNECTED;
	conn->connected_at = time(NULL);
	conn->last_activity = time(NULL);

	return (0);
}

/* Connect to MCP server (dispatches to socket or stdio) */
int
mcp_connect_server(struct mcp_client *client, const char *server_name)
{
	struct mcp_connection	*conn;

	/* Find connection by name */
	conn = mcp_find_connection(client, server_name);
	if (conn == NULL)
		return (-1);

	/* Already connected */
	if (conn->state == MCP_CONNECTED)
		return (0);

	/* Dispatch to appropriate transport */
	if (conn->config->transport == MCP_TRANSPORT_STDIO)
		return mcp_connect_stdio(conn);
	else
		return mcp_connect_socket(conn);
}

/* Disconnect from MCP server */
void
mcp_disconnect_server(struct mcp_client *client, const char *server_name)
{
	struct mcp_connection	*conn;

	conn = mcp_find_connection(client, server_name);
	if (conn == NULL)
		return;

	/* Clean up socket transport */
	if (conn->socket_fd >= 0) {
		close(conn->socket_fd);
		conn->socket_fd = -1;
	}

	/* Clean up stdio transport — close pipes first, then reap child */
	if (conn->stdin_fd >= 0) {
		close(conn->stdin_fd);
		conn->stdin_fd = -1;
	}

	if (conn->stdout_fd >= 0) {
		close(conn->stdout_fd);
		conn->stdout_fd = -1;
	}

	if (conn->server_pid > 0) {
		kill(conn->server_pid, SIGTERM);
		/* Brief wait, then force-kill to avoid zombies */
		if (waitpid(conn->server_pid, NULL, WNOHANG) == 0) {
			usleep(100000); /* 100ms grace */
			if (waitpid(conn->server_pid, NULL, WNOHANG) == 0) {
				kill(conn->server_pid, SIGKILL);
				waitpid(conn->server_pid, NULL, 0);
			}
		}
		conn->server_pid = -1;
	}

	if (conn->read_buffer != NULL) {
		free(conn->read_buffer);
		conn->read_buffer = NULL;
		conn->read_buffer_len = 0;
		conn->read_buffer_size = 0;
	}

	conn->state = MCP_DISCONNECTED;
}

/* Find connection by server name */
struct mcp_connection *
mcp_find_connection(struct mcp_client *client, const char *server_name)
{
	int	i;

	if (client == NULL || server_name == NULL)
		return (NULL);

	for (i = 0; i < client->num_connections; i++) {
		if (client->connections[i] != NULL &&
		    strcmp(client->connections[i]->config->name, server_name) == 0)
			return (client->connections[i]);
	}

	return (NULL);
}

/* Write all bytes, handling partial writes. Returns 0 on success, -1 on error. */
static int
mcp_write_all(int fd, const char *data, size_t len)
{
	ssize_t	 n;
	size_t	 off = 0;

	while (off < len) {
		n = write(fd, data + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			return (-1);
		}
		off += n;
	}
	return (0);
}

/* Send data via appropriate transport */
static ssize_t
mcp_send(struct mcp_connection *conn, const char *data, size_t len)
{
	int	fd;

	if (conn->config->transport == MCP_TRANSPORT_STDIO)
		fd = conn->stdin_fd;
	else
		fd = conn->socket_fd;

	if (fd < 0)
		return (-1);

	if (mcp_write_all(fd, data, len) < 0)
		return (-1);

	/* Stdio JSON-RPC: terminate with newline */
	if (conn->config->transport == MCP_TRANSPORT_STDIO) {
		if (mcp_write_all(fd, "\n", 1) < 0)
			return (-1);
	}

	return ((ssize_t)len);
}

/* Grow read buffer if needed. Returns 0 on success, -1 on failure. */
static int
mcp_grow_read_buffer(struct mcp_connection *conn)
{
	size_t	 new_size;
	char	*new_buf;

	/* Cap at MCP_MAX_MESSAGE_SIZE */
	if (conn->read_buffer_size >= MCP_MAX_MESSAGE_SIZE)
		return (-1);

	new_size = conn->read_buffer_size * 2;
	if (new_size > MCP_MAX_MESSAGE_SIZE)
		new_size = MCP_MAX_MESSAGE_SIZE;

	new_buf = realloc(conn->read_buffer, new_size);
	if (new_buf == NULL)
		return (-1);

	conn->read_buffer = new_buf;
	conn->read_buffer_size = new_size;
	return (0);
}

/* Receive data via appropriate transport */
static ssize_t
mcp_recv(struct mcp_connection *conn, char *buffer, size_t size)
{
	ssize_t		n;
	char		*newline;
	fd_set		readfds;
	struct timeval	timeout;
	int		ret;

	if (conn->config->transport == MCP_TRANSPORT_STDIO) {
		/* Stdio: read until we have a complete JSON-RPC response */
		while (1) {
			/* Grow buffer if full */
			if (conn->read_buffer_len >= conn->read_buffer_size - 1) {
				if (mcp_grow_read_buffer(conn) < 0)
					return (-1); /* Hit size limit */
			}

			/* Use select() to wait for data with timeout */
			FD_ZERO(&readfds);
			FD_SET(conn->stdout_fd, &readfds);
			timeout.tv_sec = MCP_SOCKET_TIMEOUT / 1000;
			timeout.tv_usec = (MCP_SOCKET_TIMEOUT % 1000) * 1000;

			ret = select(conn->stdout_fd + 1, &readfds, NULL,
			    NULL, &timeout);
			if (ret < 0) {
				if (errno == EINTR)
					continue;
				return (-1);
			} else if (ret == 0) {
				/* Timeout */
				if (conn->read_buffer_len == 0)
					return (-1);
				break;
			}

			/* Data is available, read it */
			n = read(conn->stdout_fd,
			    conn->read_buffer + conn->read_buffer_len,
			    conn->read_buffer_size - conn->read_buffer_len - 1);

			if (n > 0) {
				conn->read_buffer_len += n;
				conn->read_buffer[conn->read_buffer_len] = '\0';
			} else if (n == 0) {
				/* EOF */
				break;
			} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
				return (-1);
			}

			/* Look for complete JSON-RPC response (newline) */
			newline = strchr(conn->read_buffer, '\n');
			if (newline != NULL) {
				*newline = '\0';
				n = newline - conn->read_buffer;

				/* Copy to output buffer */
				if ((size_t)n >= size)
					n = size - 1;
				memcpy(buffer, conn->read_buffer, n);
				buffer[n] = '\0';

				/* Remove from read buffer */
				conn->read_buffer_len -=
				    (newline - conn->read_buffer + 1);
				if (conn->read_buffer_len > 0) {
					memmove(conn->read_buffer, newline + 1,
					    conn->read_buffer_len);
				}
				conn->read_buffer[conn->read_buffer_len] = '\0';

				return (n);
			}
		}

		return (-1);
	} else {
		/* Socket: direct read */
		return read(conn->socket_fd, buffer, size - 1);
	}
}

/* Call MCP tool */
struct mcp_response *
mcp_call_tool(struct mcp_client *client, const char *server_name,
    const char *tool_name, const char *arguments)
{
	struct mcp_connection	*conn;
	struct mcp_response	*resp;
	char			*request, *method;
	char			 buffer[MCP_MAX_MESSAGE_SIZE];
	ssize_t			 n;

	/* Find and ensure connected */
	conn = mcp_find_connection(client, server_name);
	if (conn == NULL)
		return (NULL);

	if (conn->state != MCP_CONNECTED) {
		if (mcp_connect_server(client, server_name) < 0)
			return (NULL);
	}

	/* Build method name: "tools/call" */
	method = xstrdup("tools/call");

	/* Build full params JSON */
	if (arguments != NULL && arguments[0] != '\0') {
		char *params;
		size_t params_len = 256 + strlen(tool_name) + strlen(arguments);
		params = xmalloc(params_len);
		snprintf(params, params_len,
		    "{\"name\":\"%s\",\"arguments\":%s}",
		    tool_name, arguments);
		request = mcp_build_request(conn->request_id++, method, params);
		free(params);
	} else {
		char *params;
		size_t params_len = 256 + strlen(tool_name);
		params = xmalloc(params_len);
		snprintf(params, params_len, "{\"name\":\"%s\"}", tool_name);
		request = mcp_build_request(conn->request_id++, method, params);
		free(params);
	}
	free(method);

	/* Send request */
	n = mcp_send(conn, request, strlen(request));
	free(request);

	if (n < 0) {
		conn->errors++;
		conn->state = MCP_ERROR;
		return (NULL);
	}

	conn->requests_sent++;
	conn->last_activity = time(NULL);

	/* Read response */
	memset(buffer, 0, sizeof buffer);
	n = mcp_recv(conn, buffer, sizeof buffer);
	if (n < 0) {
		conn->errors++;
		conn->state = MCP_ERROR;
		return (NULL);
	}

	conn->responses_received++;
	conn->last_activity = time(NULL);

	/* Parse response */
	resp = mcp_parse_response(buffer);

	return (resp);
}

/* Get state string */
const char *
mcp_state_string(enum mcp_state state)
{
	switch (state) {
	case MCP_DISCONNECTED:
		return ("disconnected");
	case MCP_CONNECTING:
		return ("connecting");
	case MCP_CONNECTED:
		return ("connected");
	case MCP_ERROR:
		return ("error");
	default:
		return ("unknown");
	}
}
