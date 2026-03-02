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

#ifndef MCP_SERVER_H
#define MCP_SERVER_H

/*
 * MCP server implementation for tmux.
 * Exposes tmux state (sessions, panes, agents) via JSON-RPC 2.0 over stdio.
 */

#define MCP_SERVER_NAME		"tmux"
#define MCP_SERVER_VERSION	"1.0.0"
#define MCP_PROTOCOL_VERSION	"2024-11-05"
#define MCP_SERVER_MAX_MSG	65536

/* MCP server state */
struct mcp_server {
	int	 running;
	int	 initialized;
	int	 in_fd;		/* Read from client (stdin) */
	int	 out_fd;	/* Write to client (stdout) */
	char	*read_buffer;
	size_t	 read_buffer_len;
	size_t	 read_buffer_size;
	u_int	 requests_handled;
	u_int	 errors;
};

/* Tool handler function type */
typedef char *(*mcp_tool_handler)(const char *);

/* Tool definition */
struct mcp_server_tool {
	const char		*name;
	const char		*description;
	const char		*input_schema;	/* JSON schema for parameters */
	mcp_tool_handler	 handler;
};

/* Server lifecycle */
struct mcp_server	*mcp_server_create(int, int);
void			 mcp_server_destroy(struct mcp_server *);
int			 mcp_server_run(struct mcp_server *);

/* Request handling */
int	mcp_server_handle_request(struct mcp_server *, const char *);
char	*mcp_server_build_response(int, const char *);
char	*mcp_server_build_error(int, int, const char *);

/* Tool handlers */
char	*mcp_tool_list_sessions(const char *);
char	*mcp_tool_capture_pane(const char *);
char	*mcp_tool_send_keys(const char *);
char	*mcp_tool_get_analytics(const char *);
char	*mcp_tool_get_coordination(const char *);
char	*mcp_tool_create_agent_session(const char *);
char	*mcp_tool_notify_session(const char *);

#endif /* MCP_SERVER_H */
