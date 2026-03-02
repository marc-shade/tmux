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

#ifndef SESSION_MCP_INTEGRATION_H
#define SESSION_MCP_INTEGRATION_H

/*
 * Session-MCP Integration
 * Automatic integration between sessions and MCP servers
 * (enhanced-memory and agent-runtime-mcp)
 */

/* Enhanced-memory integration */
int	session_mcp_save_to_memory(struct session_agent *, struct session *);
struct mcp_response	*session_mcp_find_similar(struct session_agent *);

/* Agent-runtime-mcp integration */
int	session_mcp_register_goal(struct session_agent *);
int	session_mcp_update_goal_status(struct session_agent *, const char *);
int	session_mcp_complete_goal(struct session_agent *);
struct mcp_response	*session_mcp_get_tasks(struct session_agent *);

#endif /* SESSION_MCP_INTEGRATION_H */
