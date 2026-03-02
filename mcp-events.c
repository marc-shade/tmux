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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"
#include "mcp-client.h"
#include "mcp-protocol.h"
#include "mcp-events.h"

/*
 * Phase 5: Event-driven MCP hooks.
 * Automatically emit structured events to connected MCP servers when
 * agentic lifecycle events occur. Events are fire-and-forget.
 */

static const char	*mcp_event_type_string(enum mcp_event_type);
static char		*mcp_event_json_escape(const char *);

static const char *
mcp_event_type_string(enum mcp_event_type type)
{
	switch (type) {
	case MCP_EVENT_AGENT_CREATED:
		return ("agent_created");
	case MCP_EVENT_AGENT_COMPLETED:
		return ("agent_completed");
	case MCP_EVENT_AGENT_GROUP_CHANGED:
		return ("agent_group_changed");
	case MCP_EVENT_AGENT_CONTEXT_SAVED:
		return ("agent_context_saved");
	case MCP_EVENT_AGENT_TASK_COMPLETED:
		return ("agent_task_completed");
	case MCP_EVENT_SESSION_DETACHED:
		return ("session_detached");
	case MCP_EVENT_SESSION_ATTACHED:
		return ("session_attached");
	}
	return ("unknown");
}

static char *
mcp_event_json_escape(const char *s)
{
	char	*buf, *p;
	size_t	 len;

	if (s == NULL)
		return (xstrdup(""));

	len = strlen(s) * 2 + 1;
	buf = xcalloc(1, len);
	p = buf;

	while (*s != '\0') {
		switch (*s) {
		case '"':
			*p++ = '\\';
			*p++ = '"';
			break;
		case '\\':
			*p++ = '\\';
			*p++ = '\\';
			break;
		case '\n':
			*p++ = '\\';
			*p++ = 'n';
			break;
		default:
			*p++ = *s;
			break;
		}
		s++;
	}
	*p = '\0';
	return (buf);
}

/* Initialize event system. */
void
mcp_events_init(void)
{
	log_debug("MCP event system initialized");
}

/*
 * Core event emission. Sends a structured event to enhanced-memory
 * as an entity observation. Fire-and-forget (non-blocking).
 */
void
mcp_event_emit(enum mcp_event_type type, const char *session_name,
    const char *agent_type, const char *detail)
{
	struct mcp_response	*resp;
	char			*params, *e_session, *e_type, *e_detail;
	char			*event_name;
	char			 timestamp[64];
	time_t			 now;
	struct tm		*tm_info;
	size_t			 params_len;

	if (global_mcp_client == NULL)
		return;

	/* Format timestamp. */
	now = time(NULL);
	tm_info = localtime(&now);
	strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%S", tm_info);

	e_session = mcp_event_json_escape(session_name);
	e_type = mcp_event_json_escape(agent_type);
	e_detail = mcp_event_json_escape(detail);
	event_name = mcp_event_json_escape(mcp_event_type_string(type));

	params_len = 1024 + strlen(e_session) + strlen(e_type) +
	    strlen(e_detail);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"entities\":[{"
	    "\"name\":\"event-%s-%ld\","
	    "\"entityType\":\"tmux_event\","
	    "\"observations\":["
	    "\"Event: %s\","
	    "\"Session: %s\","
	    "\"Agent type: %s\","
	    "\"Detail: %s\","
	    "\"Timestamp: %s\""
	    "]"
	    "}]}",
	    e_session, (long)now,
	    event_name, e_session, e_type, e_detail, timestamp);

	free(e_session);
	free(e_type);
	free(e_detail);
	free(event_name);

	/* Fire-and-forget: try to send, ignore errors. */
	resp = mcp_call_tool_safe(global_mcp_client, "enhanced-memory",
	    "create_entities", params);
	free(params);

	if (resp != NULL)
		mcp_response_free(resp);

	log_debug("MCP event emitted: %s for session %s",
	    mcp_event_type_string(type),
	    session_name ? session_name : "(none)");
}

/* Convenience: agent created event. */
void
mcp_event_agent_created(const char *session_name, const char *agent_type,
    const char *goal)
{
	char	 detail[512];

	snprintf(detail, sizeof detail, "Goal: %s", goal ? goal : "none");
	mcp_event_emit(MCP_EVENT_AGENT_CREATED, session_name, agent_type,
	    detail);
}

/* Convenience: agent completed event. */
void
mcp_event_agent_completed(const char *session_name, const char *agent_type,
    u_int tasks, u_int interactions)
{
	char	 detail[512];

	snprintf(detail, sizeof detail,
	    "Tasks: %u, Interactions: %u", tasks, interactions);
	mcp_event_emit(MCP_EVENT_AGENT_COMPLETED, session_name, agent_type,
	    detail);
}

/* Convenience: group changed event. */
void
mcp_event_agent_group_changed(const char *session_name,
    const char *agent_type, const char *group)
{
	char	 detail[512];

	snprintf(detail, sizeof detail, "Group: %s", group ? group : "none");
	mcp_event_emit(MCP_EVENT_AGENT_GROUP_CHANGED, session_name,
	    agent_type, detail);
}

/* Convenience: context saved event. */
void
mcp_event_agent_context_saved(const char *session_name,
    const char *agent_type)
{
	mcp_event_emit(MCP_EVENT_AGENT_CONTEXT_SAVED, session_name,
	    agent_type, "Context saved to enhanced-memory");
}

/* Convenience: session detached event. */
void
mcp_event_session_detached(const char *session_name)
{
	mcp_event_emit(MCP_EVENT_SESSION_DETACHED, session_name, NULL,
	    "Client detached");
}

/* Convenience: session attached event. */
void
mcp_event_session_attached(const char *session_name)
{
	mcp_event_emit(MCP_EVENT_SESSION_ATTACHED, session_name, NULL,
	    "Client attached");
}
