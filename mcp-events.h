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

#ifndef MCP_EVENTS_H
#define MCP_EVENTS_H

/*
 * Phase 5: Event-driven MCP hooks.
 * Automatically emit structured events to MCP servers when agentic
 * lifecycle events occur. No set-hook configuration needed.
 */

/* Event types */
enum mcp_event_type {
	MCP_EVENT_AGENT_CREATED,
	MCP_EVENT_AGENT_COMPLETED,
	MCP_EVENT_AGENT_GROUP_CHANGED,
	MCP_EVENT_AGENT_CONTEXT_SAVED,
	MCP_EVENT_AGENT_TASK_COMPLETED,
	MCP_EVENT_SESSION_DETACHED,
	MCP_EVENT_SESSION_ATTACHED
};

/* Event structure */
struct mcp_event {
	enum mcp_event_type	 type;
	const char		*session_name;
	const char		*agent_type;
	const char		*detail;	/* JSON detail string */
	time_t			 timestamp;
};

/* Event emission */
void	mcp_event_emit(enum mcp_event_type, const char *, const char *,
	    const char *);
void	mcp_event_agent_created(const char *, const char *, const char *);
void	mcp_event_agent_completed(const char *, const char *, u_int, u_int);
void	mcp_event_agent_group_changed(const char *, const char *, const char *);
void	mcp_event_agent_context_saved(const char *, const char *);
void	mcp_event_session_detached(const char *);
void	mcp_event_session_attached(const char *);

/* Event system lifecycle */
void	mcp_events_init(void);

#endif /* MCP_EVENTS_H */
