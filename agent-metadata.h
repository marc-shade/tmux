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

#ifndef AGENT_METADATA_H
#define AGENT_METADATA_H

/* Forward declaration */
struct window_pane;

/* Agent metadata for agentic system integration */
struct agent_metadata {
	char	*agent_type;		/* "Frontend Specialist", "Research Agent" */
	char	*task_id;		/* Link to agent-runtime-mcp task */
	char	*parent_agent;		/* Spawned from which agent? */
	time_t	spawn_time;		/* When was this agent created */
	double	token_count;		/* Total tokens used */
	double	cost_usd;		/* Total cost in USD */
	char	*model_name;		/* "sonnet-4", "opus-4" */
	int	mcp_connections;	/* Count of active MCP servers */
	char	*status;		/* "idle", "thinking", "executing", "error" */

	/* Budget tracking */
	double	budget_limit;		/* Max cost allowed */
	int	budget_alert_sent;	/* Have we alerted about budget? */
	time_t	last_activity;		/* For idle detection */
};

/* Function prototypes */
void		agent_metadata_init(struct window_pane *);
void		agent_metadata_free(struct window_pane *);
void		agent_metadata_set(struct window_pane *, const char *, const char *);
const char	*agent_metadata_get(struct window_pane *, const char *);
void		agent_metadata_update_cost(struct window_pane *, double, double);

#endif /* AGENT_METADATA_H */
