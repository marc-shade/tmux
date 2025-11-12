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

#ifndef MCP_CLIENT_H
#define MCP_CLIENT_H

/*
 * MCP (Model Context Protocol) client implementation for tmux.
 * Enables native communication with MCP servers for cognitive infrastructure.
 */

#define MCP_MAX_SERVERS 16
#define MCP_MAX_MESSAGE_SIZE 65536
#define MCP_SOCKET_TIMEOUT 5000  /* 5 seconds */

/* MCP transport type */
enum mcp_transport {
	MCP_TRANSPORT_SOCKET,		/* Unix socket */
	MCP_TRANSPORT_STDIO		/* stdin/stdout (spawned process) */
};

/* MCP connection state */
enum mcp_state {
	MCP_DISCONNECTED,
	MCP_CONNECTING,
	MCP_CONNECTED,
	MCP_ERROR
};

/* MCP server configuration */
struct mcp_server_config {
	char	*name;			/* Server name (e.g., "enhanced-memory") */
	enum mcp_transport transport;	/* Transport type */

	/* Socket transport */
	char	*socket_path;		/* Unix socket path */

	/* Stdio transport */
	char	*command;		/* Command to start server */
	char	**args;			/* Command arguments */

	int	auto_start;		/* Auto-start if not running */
};

/* Active MCP connection */
struct mcp_connection {
	struct mcp_server_config *config;
	enum mcp_state state;
	time_t	connected_at;
	time_t	last_activity;
	int	request_id;		/* JSON-RPC request counter */

	/* Socket transport */
	int	socket_fd;

	/* Stdio transport */
	pid_t	server_pid;		/* Spawned server process ID */
	int	stdin_fd;		/* Write to server stdin */
	int	stdout_fd;		/* Read from server stdout */
	char	*read_buffer;		/* Buffer for reading responses */
	size_t	read_buffer_len;	/* Current buffer length */
	size_t	read_buffer_size;	/* Allocated buffer size */

	/* Statistics */
	u_int	requests_sent;
	u_int	responses_received;
	u_int	errors;
};

/* MCP client (one per tmux server) */
struct mcp_client {
	struct mcp_connection *connections[MCP_MAX_SERVERS];
	int	num_connections;
	int	initialized;
};

/* MCP response */
struct mcp_response {
	int	success;
	int	request_id;
	char	*result;		/* JSON result on success */
	char	*error_message;		/* Error message on failure */
	int	error_code;
};

/* Function prototypes */

/* Client lifecycle */
struct mcp_client	*mcp_client_create(void);
void			mcp_client_destroy(struct mcp_client *);
int			mcp_client_init(struct mcp_client *);

/* Server management */
int			mcp_add_server(struct mcp_client *, struct mcp_server_config *);
int			mcp_connect_server(struct mcp_client *, const char *);
int			mcp_connect_socket(struct mcp_connection *);
int			mcp_connect_stdio(struct mcp_connection *);
void			mcp_disconnect_server(struct mcp_client *, const char *);
struct mcp_connection	*mcp_find_connection(struct mcp_client *, const char *);

/* Communication */
struct mcp_response	*mcp_call_tool(struct mcp_client *, const char *,
				const char *, const char *);
struct mcp_response	*mcp_list_tools(struct mcp_client *, const char *);
void			mcp_response_free(struct mcp_response *);

/* Status and health */
int			mcp_connection_healthy(struct mcp_connection *);
const char		*mcp_state_string(enum mcp_state);

/* JSON-RPC helpers (internal) */
char			*mcp_build_request(int, const char *, const char *);
struct mcp_response	*mcp_parse_response(const char *);

/* Low-level transport (for protocol extensions) */
ssize_t			mcp_send(struct mcp_connection *, const char *, size_t);
ssize_t			mcp_recv(struct mcp_connection *, char *, size_t);

#endif /* MCP_CLIENT_H */
