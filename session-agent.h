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

#ifndef SESSION_AGENT_H
#define SESSION_AGENT_H

/*
 * Session-Agent integration for tmux agentic system.
 * Links tmux sessions to agent-runtime-mcp goals and tracks agent lifecycle.
 */

/* Agent types */
#define AGENT_TYPE_NONE		"none"
#define AGENT_TYPE_RESEARCH	"research"
#define AGENT_TYPE_DEVELOPMENT	"development"
#define AGENT_TYPE_DEBUGGING	"debugging"
#define AGENT_TYPE_WRITING	"writing"
#define AGENT_TYPE_TESTING	"testing"
#define AGENT_TYPE_ANALYSIS	"analysis"
#define AGENT_TYPE_CUSTOM	"custom"

/* Session-agent metadata */
struct session_agent {
	char		*agent_type;		/* Agent type (research, dev, etc) */
	char		*goal;			/* Goal description */
	char		*session_name;		/* Associated session name */

	/* Runtime integration */
	char		*runtime_goal_id;	/* agent-runtime-mcp goal ID */
	char		*runtime_session_id;	/* agent-runtime-mcp session ID */

	/* Context persistence */
	char		*context_key;		/* enhanced-memory context key */
	int		 context_saved;		/* Context saved flag */

	/* Timing */
	time_t		 created;		/* Creation time */
	time_t		 last_activity;		/* Last activity time */

	/* Statistics */
	u_int		 tasks_completed;	/* Tasks completed */
	u_int		 interactions;		/* User interactions */
};

/* Function prototypes */

/* Agent lifecycle */
struct session_agent	*session_agent_create(const char *, const char *, const char *);
void			 session_agent_destroy(struct session_agent *);

/* Runtime integration */
int	session_agent_register(struct session_agent *);
int	session_agent_update_status(struct session_agent *, const char *);
int	session_agent_complete(struct session_agent *);

/* Context management */
int	session_agent_save_context(struct session_agent *, const char *);
int	session_agent_restore_context(struct session_agent *);

/* Agent info */
const char	*session_agent_get_type(struct session_agent *);
const char	*session_agent_get_goal(struct session_agent *);
const char	*session_agent_get_runtime_id(struct session_agent *);

#endif /* SESSION_AGENT_H */
