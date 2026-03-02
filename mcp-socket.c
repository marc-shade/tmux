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
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"
#include "mcp-socket.h"

#define MCP_SOCKET_RECV_SIZE 65536

/*
 * Connect to a Unix domain socket.
 * Returns NULL on failure, connection on success.
 */
struct mcp_socket_conn *
mcp_socket_connect_unix(const char *path)
{
	struct mcp_socket_conn  *conn;
	struct sockaddr_un       addr;
	int                      fd;

	if (path == NULL || *path == '\0')
		return (NULL);

	/* Check path length */
	if (strlen(path) >= sizeof(addr.sun_path)) {
		log_debug("socket path too long: %s", path);
		return (NULL);
	}

	/* Create socket */
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		log_debug("socket() failed: %s", strerror(errno));
		return (NULL);
	}

	/* Setup address */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, path, sizeof(addr.sun_path));

	/* Connect */
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_debug("connect(%s) failed: %s", path, strerror(errno));
		close(fd);
		return (NULL);
	}

	/* Set non-blocking */
	if (mcp_socket_set_nonblocking(fd) < 0) {
		close(fd);
		return (NULL);
	}

	/* Allocate connection structure */
	conn = xcalloc(1, sizeof(*conn));
	conn->fd = fd;
	conn->type = MCP_SOCKET_UNIX;
	conn->path = xstrdup(path);
	conn->recv_buf = xmalloc(MCP_SOCKET_RECV_SIZE);
	conn->recv_size = MCP_SOCKET_RECV_SIZE;
	conn->recv_len = 0;
	conn->connected_at = time(NULL);
	conn->bytes_sent = 0;
	conn->bytes_recv = 0;
	conn->msgs_sent = 0;
	conn->msgs_recv = 0;

	log_debug("connected to Unix socket: %s (fd=%d)", path, fd);
	return (conn);
}

/*
 * Send data to socket.
 * Returns number of bytes sent, or -1 on error.
 */
int
mcp_socket_send(struct mcp_socket_conn *conn, const char *data, size_t len)
{
	ssize_t  n;
	size_t   sent;

	if (conn == NULL || data == NULL || len == 0)
		return (-1);

	sent = 0;
	while (sent < len) {
		n = write(conn->fd, data + sent, len - sent);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				/* Wait for socket to be writable */
				usleep(1000);
				continue;
			}
			log_debug("socket write failed: %s", strerror(errno));
			return (-1);
		}
		sent += n;
	}

	conn->bytes_sent += sent;
	conn->msgs_sent++;

	log_debug("socket sent %zu bytes (total: %u)", sent, conn->bytes_sent);
	return (sent);
}

/*
 * Receive data from socket (non-blocking read).
 * Returns number of bytes received, 0 on EOF, -1 on error.
 */
int
mcp_socket_recv(struct mcp_socket_conn *conn, char *buf, size_t size)
{
	ssize_t  n;

	if (conn == NULL || buf == NULL || size == 0)
		return (-1);

	n = read(conn->fd, buf, size);
	if (n < 0) {
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			return (0);  /* No data available */
		log_debug("socket read failed: %s", strerror(errno));
		return (-1);
	}

	if (n == 0) {
		log_debug("socket closed by peer");
		return (0);
	}

	conn->bytes_recv += n;
	log_debug("socket received %zd bytes (total: %u)", n, conn->bytes_recv);
	return (n);
}

/*
 * Receive a complete JSON-RPC message from socket.
 * Handles buffering and message framing.
 * Returns length of message, 0 if incomplete, -1 on error.
 */
int
mcp_socket_recv_message(struct mcp_socket_conn *conn, char *msg, size_t size)
{
	char    *newline;
	size_t   msg_len;
	int      n;

	if (conn == NULL || msg == NULL || size == 0)
		return (-1);

	/* Read more data if buffer not full */
	if (conn->recv_len < conn->recv_size - 1) {
		n = mcp_socket_recv(conn,
		    conn->recv_buf + conn->recv_len,
		    conn->recv_size - conn->recv_len - 1);
		if (n < 0)
			return (-1);
		conn->recv_len += n;
		conn->recv_buf[conn->recv_len] = '\0';
	}

	/* Look for complete message (newline-delimited) */
	newline = strchr(conn->recv_buf, '\n');
	if (newline == NULL) {
		/* No complete message yet */
		if (conn->recv_len >= conn->recv_size - 1) {
			log_debug("message too large, buffer full");
			return (-1);
		}
		return (0);
	}

	/* Extract message */
	msg_len = newline - conn->recv_buf;
	if (msg_len >= size) {
		log_debug("message larger than output buffer");
		return (-1);
	}

	memcpy(msg, conn->recv_buf, msg_len);
	msg[msg_len] = '\0';

	/* Shift remaining data in buffer */
	memmove(conn->recv_buf, newline + 1, conn->recv_len - msg_len - 1);
	conn->recv_len -= (msg_len + 1);
	conn->recv_buf[conn->recv_len] = '\0';

	conn->msgs_recv++;
	log_debug("socket received message: %zu bytes (msgs: %u)",
	    msg_len, conn->msgs_recv);

	return (msg_len);
}

/*
 * Disconnect socket and free resources.
 */
void
mcp_socket_disconnect(struct mcp_socket_conn *conn)
{
	if (conn == NULL)
		return;

	log_debug("disconnecting socket (fd=%d, sent=%u, recv=%u)",
	    conn->fd, conn->bytes_sent, conn->bytes_recv);

	if (conn->fd >= 0)
		close(conn->fd);

	free(conn->path);
	free(conn->host);
	free(conn->recv_buf);
	free(conn);
}

/*
 * Check if socket is connected.
 */
int
mcp_socket_is_connected(struct mcp_socket_conn *conn)
{
	int        error;
	socklen_t  len;

	if (conn == NULL || conn->fd < 0)
		return (0);

	/* Check socket error status */
	error = 0;
	len = sizeof(error);
	if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
		return (0);

	return (error == 0);
}

#if 0  /* Disabled - requires json-c library not currently used by tmux */
/*
 * Parse socket configuration from JSON.
 * Returns 0 on success, -1 on error.
 */
int
mcp_socket_parse_config(struct json_object *config,
    enum mcp_socket_type *type, char **path, char **host, int *port)
{
	struct json_object  *obj;
	const char          *str;

	if (config == NULL)
		return (-1);

	/* Get transport type (default: unix) */
	*type = MCP_SOCKET_UNIX;
	if (json_object_object_get_ex(config, "transport", &obj)) {
		str = json_object_get_string(obj);
		if (str != NULL) {
			if (strcmp(str, "socket") == 0 ||
			    strcmp(str, "unix") == 0)
				*type = MCP_SOCKET_UNIX;
			else if (strcmp(str, "tcp") == 0)
				*type = MCP_SOCKET_TCP;
			else if (strcmp(str, "tls") == 0)
				*type = MCP_SOCKET_TLS;
		}
	}

	/* Get socket path (for Unix sockets) */
	*path = NULL;
	if (json_object_object_get_ex(config, "socketPath", &obj)) {
		str = json_object_get_string(obj);
		if (str != NULL)
			*path = xstrdup(str);
	}

	/* Get host and port (for TCP/TLS sockets - future) */
	*host = NULL;
	*port = 0;
	if (json_object_object_get_ex(config, "host", &obj)) {
		str = json_object_get_string(obj);
		if (str != NULL)
			*host = xstrdup(str);
	}
	if (json_object_object_get_ex(config, "port", &obj))
		*port = json_object_get_int(obj);

	return (0);
}
#endif  /* json-c */

/*
 * Set socket to non-blocking mode.
 */
int
mcp_socket_set_nonblocking(int fd)
{
	int  flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		log_debug("fcntl(F_GETFL) failed: %s", strerror(errno));
		return (-1);
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		log_debug("fcntl(F_SETFL) failed: %s", strerror(errno));
		return (-1);
	}

	return (0);
}

/*
 * Enable TCP keepalive on socket.
 */
int
mcp_socket_set_keepalive(int fd)
{
	int  opt;

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
		log_debug("setsockopt(SO_KEEPALIVE) failed: %s",
		    strerror(errno));
		return (-1);
	}

	return (0);
}
