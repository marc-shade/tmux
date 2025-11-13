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

#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

/*
 * MCP Protocol Extensions
 * Enhanced protocol support, error handling, and advanced features
 */

#include "mcp-client.h"  /* for enum mcp_transport */

/* Protocol handshake */
int	mcp_protocol_initialize(struct mcp_connection *);
int	mcp_protocol_initialize_socket(struct mcp_connection *);

/* Connection management with retry */
int	mcp_connect_with_retry(struct mcp_client *, const char *);
int	mcp_connection_stale(struct mcp_connection *);
void	mcp_connection_stats(struct mcp_connection *, char *, size_t);

/* Enhanced tool calling */
struct mcp_response	*mcp_call_tool_safe(struct mcp_client *, const char *,
				const char *, const char *);

/* Resource management */
struct mcp_response	*mcp_list_resources(struct mcp_client *, const char *);
struct mcp_response	*mcp_read_resource(struct mcp_client *, const char *,
				const char *);

#endif /* MCP_PROTOCOL_H */
