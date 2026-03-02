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

#ifndef AGENT_ANALYTICS_H
#define AGENT_ANALYTICS_H

#include <time.h>

/* Agent analytics data structure */
struct agent_analytics {
	/* Session statistics */
	int		 total_sessions;
	int		 active_sessions;
	int		 completed_sessions;
	int		 failed_sessions;

	/* Time tracking */
	time_t		 total_runtime;		/* Total session time */
	time_t		 avg_session_duration;
	time_t		 max_session_duration;
	time_t		 min_session_duration;

	/* Task and interaction metrics */
	int		 total_tasks_completed;
	int		 total_interactions;
	float		 avg_tasks_per_session;
	float		 avg_interactions_per_session;

	/* Goal tracking */
	int		 goals_registered;
	int		 goals_completed;
	int		 goals_abandoned;
	float		 goal_completion_rate;

	/* Context operations */
	int		 context_saves;
	int		 context_restores;
	int		 context_save_failures;

	/* Coordination metrics (Phase 4.3) */
	int		 coordination_groups;
	int		 peer_connections;
	int		 context_shares;

	/* Performance metrics */
	float		 mcp_success_rate;
	int		 mcp_calls_total;
	int		 mcp_calls_success;
	int		 mcp_calls_failed;

	/* Async metrics (Phase 4.2) */
	int		 async_operations;
	int		 async_completed;
	int		 async_failed;
	int		 async_cancelled;
};

/* Per-agent-type analytics */
struct agent_type_analytics {
	char		*type_name;		/* Agent type */
	int		 session_count;
	int		 tasks_completed;
	time_t		 total_runtime;
	float		 success_rate;
	int		 goal_completions;
};

/* Time-series data point */
struct analytics_datapoint {
	time_t		 timestamp;
	int		 sessions_active;
	int		 tasks_completed;
	int		 mcp_calls;
	int		 errors;
};

/* Analytics functions */
int			 agent_analytics_init(void);
void			 agent_analytics_free(void);

/* Data collection */
void			 agent_analytics_record_session_start(const char *);
void			 agent_analytics_record_session_end(const char *, int);
void			 agent_analytics_record_task_completed(void);
void			 agent_analytics_record_interaction(void);
void			 agent_analytics_record_goal(int);
void			 agent_analytics_record_context_save(int);
void			 agent_analytics_record_context_restore(int);
void			 agent_analytics_record_coordination(void);
void			 agent_analytics_record_mcp_call(int);
void			 agent_analytics_record_async_op(int);

/* Analytics retrieval */
struct agent_analytics	*agent_analytics_get_summary(void);
struct agent_type_analytics *agent_analytics_get_by_type(const char *);
struct agent_type_analytics **agent_analytics_get_all_types(int *);

/* Reporting */
char			*agent_analytics_generate_report(void);
char			*agent_analytics_generate_summary(void);
void			 agent_analytics_print(struct cmdq_item *);

/* Time-series data */
void			 agent_analytics_record_datapoint(void);
struct analytics_datapoint *agent_analytics_get_timeseries(int *, int);

/* Trends and patterns */
float			 agent_analytics_get_trend(const char *);
char			*agent_analytics_detect_patterns(void);

#endif /* AGENT_ANALYTICS_H */
