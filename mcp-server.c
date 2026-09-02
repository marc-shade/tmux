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

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"
#include "mcp-server.h"
#include "session-agent.h"

static char	*json_escape_server(const char *);
static char	*json_extract_string_value(const char *, const char *);
static int	 json_extract_int_value(const char *, const char *, int);
static int	 mcp_server_write(struct mcp_server *, const char *, size_t);
static int	 mcp_server_send(struct mcp_server *, const char *);
static char	*mcp_server_read_message(struct mcp_server *);
static int	 mcp_server_handle_initialize(struct mcp_server *, int);
static int	 mcp_server_handle_tools_list(struct mcp_server *, int);
static int	 mcp_server_handle_tools_call(struct mcp_server *, int,
		     const char *);

/* Tool registry */
static const struct mcp_server_tool tools[] = {
	{
		"tmux_list_sessions",
		"List all tmux sessions with agent metadata",
		"{\"type\":\"object\",\"properties\":{}}",
		mcp_tool_list_sessions
	},
	{
		"tmux_capture_pane",
		"Capture pane content from a tmux session",
		"{\"type\":\"object\",\"properties\":{"
		"\"session\":{\"type\":\"string\",\"description\":\"Session name\"},"
		"\"pane\":{\"type\":\"string\",\"description\":\"Pane ID (default: active)\"},"
		"\"lines\":{\"type\":\"integer\",\"description\":\"Lines to capture (default: 50)\"}"
		"},\"required\":[\"session\"]}",
		mcp_tool_capture_pane
	},
	{
		"tmux_send_keys",
		"Send keystrokes to a tmux pane",
		"{\"type\":\"object\",\"properties\":{"
		"\"session\":{\"type\":\"string\",\"description\":\"Session name\"},"
		"\"keys\":{\"type\":\"string\",\"description\":\"Keys to send\"}"
		"},\"required\":[\"session\",\"keys\"]}",
		mcp_tool_send_keys
	},
	{
		"tmux_get_analytics",
		"Get agent analytics data",
		"{\"type\":\"object\",\"properties\":{"
		"\"agent_type\":{\"type\":\"string\",\"description\":\"Filter by agent type\"}"
		"}}",
		mcp_tool_get_analytics
	},
	{
		"tmux_get_coordination",
		"Get coordination group and peer information",
		"{\"type\":\"object\",\"properties\":{"
		"\"group\":{\"type\":\"string\",\"description\":\"Group name to query\"}"
		"}}",
		mcp_tool_get_coordination
	},
	{
		"tmux_create_agent_session",
		"Create a new tmux session with agent metadata",
		"{\"type\":\"object\",\"properties\":{"
		"\"name\":{\"type\":\"string\",\"description\":\"Session name\"},"
		"\"agent_type\":{\"type\":\"string\",\"description\":\"Agent type\"},"
		"\"goal\":{\"type\":\"string\",\"description\":\"Session goal\"}"
		"},\"required\":[\"name\",\"agent_type\",\"goal\"]}",
		mcp_tool_create_agent_session
	},
	{
		"tmux_notify_session",
		"Send a notification message to a session",
		"{\"type\":\"object\",\"properties\":{"
		"\"session\":{\"type\":\"string\",\"description\":\"Target session\"},"
		"\"message\":{\"type\":\"string\",\"description\":\"Notification message\"}"
		"},\"required\":[\"session\",\"message\"]}",
		mcp_tool_notify_session
	}
};
#define MCP_SERVER_NUM_TOOLS (sizeof(tools) / sizeof(tools[0]))

/* Escape a string for JSON output. */
static char *
json_escape_server(const char *s)
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
		case '\r':
			*p++ = '\\';
			*p++ = 'r';
			break;
		case '\t':
			*p++ = '\\';
			*p++ = 't';
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

/* Extract a string value from a JSON object. Caller must free. */
static char *
json_extract_string_value(const char *json, const char *key)
{
	char	 search[256];
	char	*start, *end, *value;
	size_t	 len;

	snprintf(search, sizeof(search), "\"%s\"", key);
	start = strstr(json, search);
	if (start == NULL)
		return (NULL);

	start += strlen(search);
	while (*start == ' ' || *start == ':')
		start++;
	if (*start != '"')
		return (NULL);
	start++;

	end = start;
	while (*end != '\0' && *end != '"') {
		if (*end == '\\')
			end++;
		end++;
	}

	len = end - start;
	value = xmalloc(len + 1);
	memcpy(value, start, len);
	value[len] = '\0';
	return (value);
}

/* Extract an integer value from a JSON object. */
static int
json_extract_int_value(const char *json, const char *key, int def)
{
	char	 search[256];
	char	*start;

	snprintf(search, sizeof(search), "\"%s\"", key);
	start = strstr(json, search);
	if (start == NULL)
		return (def);

	start += strlen(search);
	while (*start == ' ' || *start == ':')
		start++;

	return (atoi(start));
}

/* Write all bytes to output fd. */
static int
mcp_server_write(struct mcp_server *srv, const char *data, size_t len)
{
	ssize_t	 n;
	size_t	 off = 0;

	while (off < len) {
		n = write(srv->out_fd, data + off, len - off);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			return (-1);
		}
		off += (size_t)n;
	}
	return (0);
}

/* Send a JSON-RPC message (with newline delimiter). */
static int
mcp_server_send(struct mcp_server *srv, const char *msg)
{
	if (mcp_server_write(srv, msg, strlen(msg)) != 0)
		return (-1);
	return (mcp_server_write(srv, "\n", 1));
}

/* Read a complete newline-delimited message. Returns NULL on EOF/error. */
static char *
mcp_server_read_message(struct mcp_server *srv)
{
	struct pollfd	 pfd;
	char		 buf[4096];
	char		*newline;
	ssize_t		 n;

	for (;;) {
		/* Check for complete message in buffer. */
		newline = memchr(srv->read_buffer, '\n', srv->read_buffer_len);
		if (newline != NULL) {
			size_t	 msglen = (size_t)(newline - srv->read_buffer);
			char	*msg = xmalloc(msglen + 1);

			memcpy(msg, srv->read_buffer, msglen);
			msg[msglen] = '\0';

			/* Shift remaining data. */
			srv->read_buffer_len -= msglen + 1;
			if (srv->read_buffer_len > 0) {
				memmove(srv->read_buffer, newline + 1,
				    srv->read_buffer_len);
			}
			return (msg);
		}

		/* Poll for data with timeout. */
		pfd.fd = srv->in_fd;
		pfd.events = POLLIN;
		n = poll(&pfd, 1, 30000); /* 30 second timeout */
		if (n <= 0)
			return (NULL);

		/* Read more data. */
		n = read(srv->in_fd, buf, sizeof(buf));
		if (n <= 0)
			return (NULL);

		/* Grow buffer if needed. */
		if (srv->read_buffer_len + (size_t)n > srv->read_buffer_size) {
			srv->read_buffer_size = (srv->read_buffer_len +
			    (size_t)n) * 2;
			srv->read_buffer = xrealloc(srv->read_buffer,
			    srv->read_buffer_size);
		}
		memcpy(srv->read_buffer + srv->read_buffer_len, buf, (size_t)n);
		srv->read_buffer_len += (size_t)n;
	}
}

/* Create MCP server. */
struct mcp_server *
mcp_server_create(int in_fd, int out_fd)
{
	struct mcp_server	*srv;

	srv = xcalloc(1, sizeof(*srv));
	srv->in_fd = in_fd;
	srv->out_fd = out_fd;
	srv->read_buffer_size = 4096;
	srv->read_buffer = xcalloc(1, srv->read_buffer_size);
	return (srv);
}

/* Destroy MCP server. */
void
mcp_server_destroy(struct mcp_server *srv)
{
	if (srv == NULL)
		return;
	free(srv->read_buffer);
	free(srv);
}

/* Build a JSON-RPC success response. */
char *
mcp_server_build_response(int id, const char *result)
{
	char	*msg;

	xasprintf(&msg,
	    "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result);
	return (msg);
}

/* Build a JSON-RPC error response. */
char *
mcp_server_build_error(int id, int code, const char *message)
{
	char	*escaped, *msg;

	escaped = json_escape_server(message);
	xasprintf(&msg,
	    "{\"jsonrpc\":\"2.0\",\"id\":%d,"
	    "\"error\":{\"code\":%d,\"message\":\"%s\"}}",
	    id, code, escaped);
	free(escaped);
	return (msg);
}

/* Handle initialize request. */
static int
mcp_server_handle_initialize(struct mcp_server *srv, int id)
{
	char	*response, *msg;

	xasprintf(&response,
	    "{\"protocolVersion\":\"%s\","
	    "\"capabilities\":{\"tools\":{}},"
	    "\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}}",
	    MCP_PROTOCOL_VERSION, MCP_SERVER_NAME, MCP_SERVER_VERSION);

	msg = mcp_server_build_response(id, response);
	free(response);

	if (mcp_server_send(srv, msg) != 0) {
		free(msg);
		return (-1);
	}
	free(msg);
	srv->initialized = 1;
	return (0);
}

/* Handle tools/list request. */
static int
mcp_server_handle_tools_list(struct mcp_server *srv, int id)
{
	char	*tools_json, *msg, *tmp;
	size_t	 i;

	xasprintf(&tools_json, "{\"tools\":[");
	for (i = 0; i < MCP_SERVER_NUM_TOOLS; i++) {
		xasprintf(&tmp,
		    "%s%s{\"name\":\"%s\","
		    "\"description\":\"%s\","
		    "\"inputSchema\":%s}",
		    tools_json, (i > 0 ? "," : ""),
		    tools[i].name, tools[i].description,
		    tools[i].input_schema);
		free(tools_json);
		tools_json = tmp;
	}
	xasprintf(&tmp, "%s]}", tools_json);
	free(tools_json);
	tools_json = tmp;

	msg = mcp_server_build_response(id, tools_json);
	free(tools_json);

	if (mcp_server_send(srv, msg) != 0) {
		free(msg);
		return (-1);
	}
	free(msg);
	return (0);
}

/* Handle tools/call request. */
static int
mcp_server_handle_tools_call(struct mcp_server *srv, int id,
    const char *request)
{
	char	*tool_name, *args_start, *result, *content;
	char	*escaped, *msg;
	size_t	 i;
	int	 found = 0;

	/* Extract tool name from params.name. */
	tool_name = json_extract_string_value(request, "name");
	if (tool_name == NULL) {
		msg = mcp_server_build_error(id, -32602,
		    "Missing tool name");
		mcp_server_send(srv, msg);
		free(msg);
		return (0);
	}

	/* Find arguments JSON. */
	args_start = strstr(request, "\"arguments\"");
	if (args_start != NULL) {
		args_start = strchr(args_start + 11, '{');
	}

	/* Look up and call tool handler. */
	for (i = 0; i < MCP_SERVER_NUM_TOOLS; i++) {
		if (strcmp(tools[i].name, tool_name) == 0) {
			result = tools[i].handler(
			    args_start != NULL ? args_start : "{}");
			found = 1;
			break;
		}
	}

	if (!found) {
		msg = mcp_server_build_error(id, -32601,
		    "Unknown tool");
		mcp_server_send(srv, msg);
		free(msg);
		free(tool_name);
		return (0);
	}

	/* Build tool result response. */
	escaped = json_escape_server(result);
	xasprintf(&content,
	    "{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}]}", escaped);
	msg = mcp_server_build_response(id, content);
	free(escaped);
	free(content);
	free(result);
	free(tool_name);

	if (mcp_server_send(srv, msg) != 0) {
		free(msg);
		return (-1);
	}
	free(msg);
	srv->requests_handled++;
	return (0);
}

/* Handle a single JSON-RPC request. Returns -1 on fatal error, 0 otherwise. */
int
mcp_server_handle_request(struct mcp_server *srv, const char *request)
{
	char	*method;
	int	 id;

	/* Extract method. */
	method = json_extract_string_value(request, "method");
	if (method == NULL)
		return (0); /* Ignore malformed requests. */

	/* Extract id (default 0 for notifications). */
	id = json_extract_int_value(request, "id", 0);

	/* Route by method. */
	if (strcmp(method, "initialize") == 0) {
		free(method);
		return (mcp_server_handle_initialize(srv, id));
	}
	if (strcmp(method, "notifications/initialized") == 0 ||
	    strcmp(method, "initialized") == 0) {
		free(method);
		return (0); /* Acknowledged. */
	}
	if (strcmp(method, "tools/list") == 0) {
		free(method);
		return (mcp_server_handle_tools_list(srv, id));
	}
	if (strcmp(method, "tools/call") == 0) {
		free(method);
		return (mcp_server_handle_tools_call(srv, id, request));
	}

	/* Unknown method. */
	{
		char	*msg = mcp_server_build_error(id, -32601,
			    "Method not found");
		mcp_server_send(srv, msg);
		free(msg);
	}
	free(method);
	return (0);
}

/* Run the MCP server main loop (blocking, reads from stdin). */
int
mcp_server_run(struct mcp_server *srv)
{
	char	*msg;

	srv->running = 1;
	while (srv->running) {
		msg = mcp_server_read_message(srv);
		if (msg == NULL)
			break; /* EOF or error. */

		/* Skip empty lines. */
		if (*msg == '\0') {
			free(msg);
			continue;
		}

		if (mcp_server_handle_request(srv, msg) != 0) {
			free(msg);
			break; /* Fatal error. */
		}
		free(msg);
	}
	return (0);
}

/*
 * Tool implementations.
 * These run in the tmux client process and communicate with the tmux server
 * via the normal tmux command infrastructure.
 */

/* tmux_list_sessions: List all sessions with agent metadata. */
char *
mcp_tool_list_sessions(const char *args __attribute__((unused)))
{
	struct session		*s;
	struct session_agent	*sa;
	char			*result, *tmp, *escaped;
	int			 first = 1;

	xasprintf(&result, "[");
	RB_FOREACH(s, sessions, &sessions) {
		sa = s->agent_metadata;

		if (sa != NULL) {
			char	*e_type = json_escape_server(sa->agent_type);
			char	*e_goal = json_escape_server(sa->goal);
			char	*e_group = json_escape_server(
			    sa->coordination_group);
			char	*e_rid = json_escape_server(
			    sa->runtime_goal_id);

			escaped = NULL;
			xasprintf(&escaped,
			    "{\"name\":\"%s\","
			    "\"agent_type\":\"%s\","
			    "\"goal\":\"%s\","
			    "\"tasks_completed\":%u,"
			    "\"interactions\":%u,"
			    "\"context_saved\":%s,"
			    "\"runtime_goal_id\":\"%s\","
			    "\"coordination_group\":\"%s\","
			    "\"num_peers\":%d,"
			    "\"is_coordinator\":%s,"
			    "\"created\":%lld,"
			    "\"last_activity\":%lld}",
			    s->name,
			    e_type, e_goal,
			    sa->tasks_completed,
			    sa->interactions,
			    sa->context_saved ? "true" : "false",
			    e_rid, e_group,
			    sa->num_peers,
			    sa->is_coordinator ? "true" : "false",
			    (long long)sa->created,
			    (long long)sa->last_activity);
			free(e_type);
			free(e_goal);
			free(e_group);
			free(e_rid);
		} else {
			xasprintf(&escaped,
			    "{\"name\":\"%s\","
			    "\"agent_type\":null,"
			    "\"goal\":null}",
			    s->name);
		}

		xasprintf(&tmp, "%s%s%s", result,
		    first ? "" : ",", escaped);
		free(result);
		free(escaped);
		result = tmp;
		first = 0;
	}
	xasprintf(&tmp, "%s]", result);
	free(result);
	return (tmp);
}

/* tmux_capture_pane: Capture content from a pane. */
char *
mcp_tool_capture_pane(const char *args)
{
	char			*session_name, *pane_id;
	int			 lines;
	struct session		*s;
	struct window_pane	*wp;
	struct grid		*gd;
	u_int			 i, sy, top;
	char			*result, *tmp, *line, *escaped;
	size_t			 linelen;
	int			 first = 1;

	session_name = json_extract_string_value(args, "session");
	if (session_name == NULL)
		return (xstrdup("{\"error\":\"missing session parameter\"}"));

	pane_id = json_extract_string_value(args, "pane");
	lines = json_extract_int_value(args, "lines", 50);
	if (lines > 1000)
		lines = 1000;
	if (lines < 1)
		lines = 50;

	/* Find session. */
	s = session_find(session_name);
	free(session_name);
	if (s == NULL) {
		free(pane_id);
		return (xstrdup("{\"error\":\"session not found\"}"));
	}

	/* Find pane. */
	if (pane_id != NULL) {
		/* Try to parse pane id as number. */
		wp = window_pane_find_by_id(atoi(pane_id));
		free(pane_id);
	} else {
		wp = s->curw->window->active;
	}
	if (wp == NULL)
		return (xstrdup("{\"error\":\"pane not found\"}"));

	/* Capture pane content. */
	gd = wp->base.grid;
	sy = gd->sy;
	top = (u_int)lines > sy ? 0 : sy - (u_int)lines;

	xasprintf(&result, "{\"lines\":[");
	for (i = top; i < sy; i++) {
		line = grid_string_cells(gd, 0, i,
		    screen_size_x(&wp->base), NULL, 0, &wp->base);
		if (line == NULL)
			continue;

		/* Trim trailing whitespace. */
		linelen = strlen(line);
		while (linelen > 0 && line[linelen - 1] == ' ')
			line[--linelen] = '\0';

		escaped = json_escape_server(line);
		xasprintf(&tmp, "%s%s\"%s\"", result,
		    first ? "" : ",", escaped);
		free(result);
		free(escaped);
		free(line);
		result = tmp;
		first = 0;
	}
	xasprintf(&tmp, "%s],\"pane_id\":%u,\"captured\":%u}",
	    result, wp->id, sy - top);
	free(result);
	return (tmp);
}

/* tmux_send_keys: Send keys to a session pane. */
char *
mcp_tool_send_keys(const char *args)
{
	char			*session_name, *keys;
	struct session		*s;
	struct window_pane	*wp;

	session_name = json_extract_string_value(args, "session");
	keys = json_extract_string_value(args, "keys");

	if (session_name == NULL || keys == NULL) {
		free(session_name);
		free(keys);
		return (xstrdup("{\"error\":\"missing session or keys\"}"));
	}

	s = session_find(session_name);
	free(session_name);
	if (s == NULL) {
		free(keys);
		return (xstrdup("{\"error\":\"session not found\"}"));
	}

	wp = s->curw->window->active;
	if (wp == NULL) {
		free(keys);
		return (xstrdup("{\"error\":\"no active pane\"}"));
	}

	/* Send the keys by buffering them into the pane input. */
	bufferevent_write(wp->event, keys, strlen(keys));
	free(keys);

	return (xstrdup("{\"status\":\"ok\"}"));
}

/* tmux_get_analytics: Get agent analytics summary. */
char *
mcp_tool_get_analytics(const char *args)
{
	struct session		*s;
	struct session_agent	*sa;
	char			*result, *tmp;
	char			*type_filter;
	int			 total = 0, active = 0;
	u_int			 total_tasks = 0, total_interactions = 0;
	int			 first = 1;

	type_filter = json_extract_string_value(args, "agent_type");

	xasprintf(&result, "{\"sessions\":[");
	RB_FOREACH(s, sessions, &sessions) {
		sa = s->agent_metadata;
		if (sa == NULL)
			continue;
		if (type_filter != NULL &&
		    strcmp(sa->agent_type, type_filter) != 0)
			continue;

		total++;
		active++;
		total_tasks += sa->tasks_completed;
		total_interactions += sa->interactions;

		{
			char	*e_type = json_escape_server(sa->agent_type);
			char	*e_goal = json_escape_server(sa->goal);

			xasprintf(&tmp,
			    "%s%s{\"name\":\"%s\","
			    "\"type\":\"%s\","
			    "\"goal\":\"%s\","
			    "\"tasks\":%u,"
			    "\"interactions\":%u,"
			    "\"duration\":%lld}",
			    result, first ? "" : ",",
			    s->name, e_type, e_goal,
			    sa->tasks_completed,
			    sa->interactions,
			    (long long)(time(NULL) - sa->created));
			free(e_type);
			free(e_goal);
		}
		free(result);
		result = tmp;
		first = 0;
	}
	xasprintf(&tmp,
	    "%s],\"summary\":{\"total_sessions\":%d,"
	    "\"active_sessions\":%d,"
	    "\"total_tasks\":%u,"
	    "\"total_interactions\":%u}}",
	    result, total, active, total_tasks, total_interactions);
	free(result);
	free(type_filter);
	return (tmp);
}

/* tmux_get_coordination: Get coordination group info. */
char *
mcp_tool_get_coordination(const char *args)
{
	struct session		*s;
	struct session_agent	*sa;
	char			*result, *tmp, *group_filter;
	int			 first_group = 1, first_member = 1;

	group_filter = json_extract_string_value(args, "group");

	xasprintf(&result, "{\"groups\":[");
	/* We need to discover groups by scanning sessions. */
	{
		/* Track seen groups to avoid duplicates. */
		char	*seen[64];
		int	 nseen = 0;
		int	 i, already;

		RB_FOREACH(s, sessions, &sessions) {
			sa = s->agent_metadata;
			if (sa == NULL || sa->coordination_group == NULL)
				continue;
			if (group_filter != NULL &&
			    strcmp(sa->coordination_group,
			    group_filter) != 0)
				continue;

			/* Check if we already emitted this group. */
			already = 0;
			for (i = 0; i < nseen; i++) {
				if (strcmp(seen[i],
				    sa->coordination_group) == 0) {
					already = 1;
					break;
				}
			}
			if (already)
				continue;
			if (nseen < 64)
				seen[nseen++] = sa->coordination_group;

			/* Emit group with members. */
			{
				struct session		*s2;
				struct session_agent	*sa2;
				char			*e_grp;

				e_grp = json_escape_server(
				    sa->coordination_group);
				xasprintf(&tmp,
				    "%s%s{\"name\":\"%s\",\"members\":[",
				    result, first_group ? "" : ",", e_grp);
				free(e_grp);
				free(result);
				result = tmp;
				first_group = 0;
				first_member = 1;

				RB_FOREACH(s2, sessions, &sessions) {
					sa2 = s2->agent_metadata;
					if (sa2 == NULL ||
					    sa2->coordination_group == NULL)
						continue;
					if (strcmp(sa2->coordination_group,
					    sa->coordination_group) != 0)
						continue;

					{
						char *e_t, *e_g;
						e_t = json_escape_server(
						    sa2->agent_type);
						e_g = json_escape_server(
						    sa2->goal);
						xasprintf(&tmp,
						    "%s%s{\"session\":\"%s\","
						    "\"type\":\"%s\","
						    "\"goal\":\"%s\","
						    "\"is_coordinator\":%s}",
						    result,
						    first_member ? "" : ",",
						    s2->name, e_t, e_g,
						    sa2->is_coordinator ?
						    "true" : "false");
						free(e_t);
						free(e_g);
					}
					free(result);
					result = tmp;
					first_member = 0;
				}
				xasprintf(&tmp, "%s]}", result);
				free(result);
				result = tmp;
			}
		}
	}
	xasprintf(&tmp, "%s]}", result);
	free(result);
	free(group_filter);
	return (tmp);
}

/* tmux_create_agent_session: Create session with agent metadata. */
char *
mcp_tool_create_agent_session(const char *args)
{
	char	*name, *agent_type, *goal;
	char	*escaped_name;

	name = json_extract_string_value(args, "name");
	agent_type = json_extract_string_value(args, "agent_type");
	goal = json_extract_string_value(args, "goal");

	if (name == NULL || agent_type == NULL || goal == NULL) {
		free(name);
		free(agent_type);
		free(goal);
		return (xstrdup(
		    "{\"error\":\"missing name, agent_type, or goal\"}"));
	}

	/* Check if session already exists. */
	if (session_find(name) != NULL) {
		char	*r;

		escaped_name = json_escape_server(name);
		xasprintf(&r,
		    "{\"error\":\"session '%s' already exists\"}", escaped_name);
		free(escaped_name);
		free(name);
		free(agent_type);
		free(goal);
		return (r);
	}

	/*
	 * We can't create a full session from here without a client context.
	 * Return instructions for the caller to execute via tmux CLI.
	 */
	{
		char	*e_name, *e_type, *e_goal, *r;

		e_name = json_escape_server(name);
		e_type = json_escape_server(agent_type);
		e_goal = json_escape_server(goal);
		xasprintf(&r,
		    "{\"status\":\"pending\","
		    "\"command\":\"tmux new-session -d -G %s -o '%s' -s %s\","
		    "\"name\":\"%s\","
		    "\"agent_type\":\"%s\","
		    "\"goal\":\"%s\"}",
		    agent_type, goal, name,
		    e_name, e_type, e_goal);
		free(e_name);
		free(e_type);
		free(e_goal);
		free(name);
		free(agent_type);
		free(goal);
		return (r);
	}
}

/* tmux_notify_session: Display a message in a session. */
char *
mcp_tool_notify_session(const char *args)
{
	char		*session_name, *message;
	struct session	*s;
	struct client	*c;

	session_name = json_extract_string_value(args, "session");
	message = json_extract_string_value(args, "message");

	if (session_name == NULL || message == NULL) {
		free(session_name);
		free(message);
		return (xstrdup("{\"error\":\"missing session or message\"}"));
	}

	s = session_find(session_name);
	free(session_name);
	if (s == NULL) {
		free(message);
		return (xstrdup("{\"error\":\"session not found\"}"));
	}

	/* Find a client attached to this session and show message. */
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == s) {
			status_message_set(c, -1, 0, 0, 0, "%s", message);
			free(message);
			return (xstrdup("{\"status\":\"delivered\"}"));
		}
	}

	free(message);
	return (xstrdup("{\"status\":\"queued\","
	    "\"note\":\"session has no attached client\"}"));
}
