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

#ifndef MCP_SOCKET_H
#define MCP_SOCKET_H

/* Forward declarations */
struct json_object;

/* Socket transport types */
enum mcp_socket_type {
	MCP_SOCKET_UNIX,     /* Unix domain socket */
	MCP_SOCKET_TCP,      /* TCP socket (future) */
	MCP_SOCKET_TLS       /* TLS socket (future) */
};

/* Socket connection state */
struct mcp_socket_conn {
	int                      fd;           /* Socket file descriptor */
	enum mcp_socket_type     type;         /* Socket type */
	char                    *path;         /* Unix socket path */
	char                    *host;         /* TCP host (future) */
	int                      port;         /* TCP port (future) */

	/* Buffer management */
	char                    *recv_buf;     /* Receive buffer */
	size_t                   recv_size;    /* Buffer size */
	size_t                   recv_len;     /* Current data length */

	/* Connection stats */
	time_t                   connected_at; /* Connection timestamp */
	unsigned int             bytes_sent;   /* Total bytes sent */
	unsigned int             bytes_recv;   /* Total bytes received */
	unsigned int             msgs_sent;    /* Messages sent */
	unsigned int             msgs_recv;    /* Messages received */
};

/* Socket transport functions */
struct mcp_socket_conn *mcp_socket_connect_unix(const char *);
int                     mcp_socket_send(struct mcp_socket_conn *, const char *,
                            size_t);
int                     mcp_socket_recv(struct mcp_socket_conn *, char *,
                            size_t);
int                     mcp_socket_recv_message(struct mcp_socket_conn *,
                            char *, size_t);
void                    mcp_socket_disconnect(struct mcp_socket_conn *);
int                     mcp_socket_is_connected(struct mcp_socket_conn *);

/* Socket configuration helpers */
/* Note: mcp_socket_parse_config() requires json-c library, currently unused */
int                     mcp_socket_set_nonblocking(int);
int                     mcp_socket_set_keepalive(int);

#endif /* MCP_SOCKET_H */
