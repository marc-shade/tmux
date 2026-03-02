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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"
#include "agent-analytics.h"

/* Global analytics data */
static struct agent_analytics	 global_analytics;
static int			 analytics_initialized = 0;

/* Per-type analytics storage */
#define MAX_AGENT_TYPES 32
static struct agent_type_analytics *type_analytics[MAX_AGENT_TYPES];
static int			 type_count = 0;

/* Time-series data */
#define MAX_DATAPOINTS 1440  /* 24 hours at 1-minute intervals */
static struct analytics_datapoint datapoints[MAX_DATAPOINTS];
static int			 datapoint_count = 0;
static int			 datapoint_index = 0;

/* Active session tracking */
struct active_session_record {
	char		*session_name;
	char		*agent_type;
	time_t		 start_time;
	int		 tasks_at_start;
};

#define MAX_ACTIVE_SESSIONS 64
static struct active_session_record active_sessions[MAX_ACTIVE_SESSIONS];
static int			 active_session_count = 0;

/*
 * Initialize analytics system
 */
int
agent_analytics_init(void)
{
	if (analytics_initialized)
		return (0);

	memset(&global_analytics, 0, sizeof(global_analytics));
	memset(type_analytics, 0, sizeof(type_analytics));
	memset(datapoints, 0, sizeof(datapoints));
	memset(active_sessions, 0, sizeof(active_sessions));

	/* Initialize min duration to max value */
	global_analytics.min_session_duration = LONG_MAX;

	analytics_initialized = 1;
	return (0);
}

/*
 * Free analytics resources
 */
void
agent_analytics_free(void)
{
	int	 i;

	if (!analytics_initialized)
		return;

	/* Free per-type analytics */
	for (i = 0; i < type_count; i++) {
		if (type_analytics[i] != NULL) {
			free(type_analytics[i]->type_name);
			free(type_analytics[i]);
		}
	}

	/* Free active session records */
	for (i = 0; i < active_session_count; i++) {
		free(active_sessions[i].session_name);
		free(active_sessions[i].agent_type);
	}

	analytics_initialized = 0;
}

/*
 * Find or create per-type analytics
 */
static struct agent_type_analytics *
get_type_analytics(const char *type_name)
{
	struct agent_type_analytics	*ta;
	int				 i;

	/* Find existing */
	for (i = 0; i < type_count; i++) {
		if (type_analytics[i] != NULL &&
		    strcmp(type_analytics[i]->type_name, type_name) == 0)
			return (type_analytics[i]);
	}

	/* Create new if space available */
	if (type_count >= MAX_AGENT_TYPES)
		return (NULL);

	ta = xcalloc(1, sizeof(*ta));
	ta->type_name = xstrdup(type_name);
	ta->session_count = 0;
	ta->tasks_completed = 0;
	ta->total_runtime = 0;
	ta->success_rate = 0.0;
	ta->goal_completions = 0;

	type_analytics[type_count++] = ta;
	return (ta);
}

/*
 * Record session start
 */
void
agent_analytics_record_session_start(const char *agent_type)
{
	struct agent_type_analytics	*ta;
	struct active_session_record	*as;

	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.total_sessions++;
	global_analytics.active_sessions++;

	/* Update per-type analytics */
	if (agent_type != NULL) {
		ta = get_type_analytics(agent_type);
		if (ta != NULL)
			ta->session_count++;
	}

	/* Record active session (if space) */
	if (active_session_count < MAX_ACTIVE_SESSIONS) {
		as = &active_sessions[active_session_count++];
		as->session_name = NULL;  /* Set by caller if needed */
		as->agent_type = agent_type ? xstrdup(agent_type) : NULL;
		as->start_time = time(NULL);
		as->tasks_at_start = global_analytics.total_tasks_completed;
	}
}

/*
 * Record session end
 */
void
agent_analytics_record_session_end(const char *agent_type, int success)
{
	struct agent_type_analytics	*ta;
	time_t				 duration;
	int				 i;

	if (!analytics_initialized)
		return;

	global_analytics.active_sessions--;
	if (success)
		global_analytics.completed_sessions++;
	else
		global_analytics.failed_sessions++;

	/* Calculate duration from active session record */
	for (i = 0; i < active_session_count; i++) {
		if (active_sessions[i].agent_type != NULL &&
		    agent_type != NULL &&
		    strcmp(active_sessions[i].agent_type, agent_type) == 0) {
			duration = time(NULL) - active_sessions[i].start_time;
			global_analytics.total_runtime += duration;

			if (duration > global_analytics.max_session_duration)
				global_analytics.max_session_duration = duration;
			if (duration < global_analytics.min_session_duration)
				global_analytics.min_session_duration = duration;

			/* Update per-type analytics */
			ta = get_type_analytics(agent_type);
			if (ta != NULL) {
				ta->total_runtime += duration;
				if (success)
					ta->success_rate =
					    (ta->success_rate * (ta->session_count - 1) + 100.0) /
					    ta->session_count;
			}

			/* Remove from active sessions */
			free(active_sessions[i].session_name);
			free(active_sessions[i].agent_type);
			memmove(&active_sessions[i], &active_sessions[i + 1],
			    (active_session_count - i - 1) * sizeof(struct active_session_record));
			active_session_count--;
			break;
		}
	}

	/* Calculate average duration */
	if (global_analytics.completed_sessions > 0)
		global_analytics.avg_session_duration =
		    global_analytics.total_runtime / global_analytics.completed_sessions;
}

/*
 * Record task completed
 */
void
agent_analytics_record_task_completed(void)
{
	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.total_tasks_completed++;

	if (global_analytics.completed_sessions > 0)
		global_analytics.avg_tasks_per_session =
		    (float)global_analytics.total_tasks_completed /
		    global_analytics.completed_sessions;
}

/*
 * Record interaction
 */
void
agent_analytics_record_interaction(void)
{
	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.total_interactions++;

	if (global_analytics.completed_sessions > 0)
		global_analytics.avg_interactions_per_session =
		    (float)global_analytics.total_interactions /
		    global_analytics.completed_sessions;
}

/*
 * Record goal operation
 */
void
agent_analytics_record_goal(int operation)
{
	if (!analytics_initialized)
		agent_analytics_init();

	switch (operation) {
	case 0:  /* registered */
		global_analytics.goals_registered++;
		break;
	case 1:  /* completed */
		global_analytics.goals_completed++;
		break;
	case 2:  /* abandoned */
		global_analytics.goals_abandoned++;
		break;
	}

	if (global_analytics.goals_registered > 0)
		global_analytics.goal_completion_rate =
		    (float)global_analytics.goals_completed * 100.0 /
		    global_analytics.goals_registered;
}

/*
 * Record context save
 */
void
agent_analytics_record_context_save(int success)
{
	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.context_saves++;
	if (!success)
		global_analytics.context_save_failures++;
}

/*
 * Record context restore
 */
void
agent_analytics_record_context_restore(int success)
{
	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.context_restores++;
	(void)success;  /* Currently unused */
}

/*
 * Record coordination event
 */
void
agent_analytics_record_coordination(void)
{
	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.context_shares++;
}

/*
 * Record MCP call
 */
void
agent_analytics_record_mcp_call(int success)
{
	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.mcp_calls_total++;
	if (success)
		global_analytics.mcp_calls_success++;
	else
		global_analytics.mcp_calls_failed++;

	if (global_analytics.mcp_calls_total > 0)
		global_analytics.mcp_success_rate =
		    (float)global_analytics.mcp_calls_success * 100.0 /
		    global_analytics.mcp_calls_total;
}

/*
 * Record async operation
 */
void
agent_analytics_record_async_op(int status)
{
	if (!analytics_initialized)
		agent_analytics_init();

	global_analytics.async_operations++;
	switch (status) {
	case 0:  /* completed */
		global_analytics.async_completed++;
		break;
	case 1:  /* failed */
		global_analytics.async_failed++;
		break;
	case 2:  /* cancelled */
		global_analytics.async_cancelled++;
		break;
	}
}

/*
 * Get global analytics summary
 */
struct agent_analytics *
agent_analytics_get_summary(void)
{
	if (!analytics_initialized)
		agent_analytics_init();

	return (&global_analytics);
}

/*
 * Get analytics by type
 */
struct agent_type_analytics *
agent_analytics_get_by_type(const char *type_name)
{
	int	 i;

	if (!analytics_initialized)
		return (NULL);

	for (i = 0; i < type_count; i++) {
		if (type_analytics[i] != NULL &&
		    strcmp(type_analytics[i]->type_name, type_name) == 0)
			return (type_analytics[i]);
	}

	return (NULL);
}

/*
 * Get all type analytics
 */
struct agent_type_analytics **
agent_analytics_get_all_types(int *count)
{
	if (!analytics_initialized) {
		*count = 0;
		return (NULL);
	}

	*count = type_count;
	return (type_analytics);
}

/*
 * Generate text report
 */
char *
agent_analytics_generate_report(void)
{
	struct agent_analytics	*a;
	char			*report;
	size_t			 size;
	int			 i;

	if (!analytics_initialized)
		agent_analytics_init();

	a = &global_analytics;
	size = 2048;
	report = xmalloc(size);

	snprintf(report, size,
	    "Agent Analytics Report\n"
	    "======================\n\n"
	    "Session Statistics:\n"
	    "  Total Sessions: %d\n"
	    "  Active Sessions: %d\n"
	    "  Completed: %d\n"
	    "  Failed: %d\n\n"
	    "Time Tracking:\n"
	    "  Total Runtime: %ld seconds (%.1f hours)\n"
	    "  Average Duration: %ld seconds (%.1f minutes)\n"
	    "  Max Duration: %ld seconds\n"
	    "  Min Duration: %ld seconds\n\n"
	    "Task Metrics:\n"
	    "  Total Tasks Completed: %d\n"
	    "  Avg Tasks/Session: %.2f\n"
	    "  Total Interactions: %d\n"
	    "  Avg Interactions/Session: %.2f\n\n"
	    "Goal Tracking:\n"
	    "  Goals Registered: %d\n"
	    "  Goals Completed: %d\n"
	    "  Goals Abandoned: %d\n"
	    "  Completion Rate: %.1f%%\n\n"
	    "Context Operations:\n"
	    "  Saves: %d\n"
	    "  Restores: %d\n"
	    "  Save Failures: %d\n\n"
	    "Coordination (Phase 4.3):\n"
	    "  Context Shares: %d\n\n"
	    "MCP Performance:\n"
	    "  Total Calls: %d\n"
	    "  Success: %d\n"
	    "  Failed: %d\n"
	    "  Success Rate: %.1f%%\n\n"
	    "Async Operations (Phase 4.2):\n"
	    "  Total: %d\n"
	    "  Completed: %d\n"
	    "  Failed: %d\n"
	    "  Cancelled: %d\n\n",
	    a->total_sessions, a->active_sessions,
	    a->completed_sessions, a->failed_sessions,
	    a->total_runtime, a->total_runtime / 3600.0,
	    a->avg_session_duration, a->avg_session_duration / 60.0,
	    a->max_session_duration,
	    a->min_session_duration == LONG_MAX ? 0 : a->min_session_duration,
	    a->total_tasks_completed, a->avg_tasks_per_session,
	    a->total_interactions, a->avg_interactions_per_session,
	    a->goals_registered, a->goals_completed, a->goals_abandoned,
	    a->goal_completion_rate,
	    a->context_saves, a->context_restores, a->context_save_failures,
	    a->context_shares,
	    a->mcp_calls_total, a->mcp_calls_success, a->mcp_calls_failed,
	    a->mcp_success_rate,
	    a->async_operations, a->async_completed, a->async_failed,
	    a->async_cancelled);

	/* Add per-type analytics */
	if (type_count > 0) {
		strlcat(report, "Per-Type Analytics:\n", size);
		for (i = 0; i < type_count; i++) {
			if (type_analytics[i] != NULL) {
				char type_section[256];
				snprintf(type_section, sizeof(type_section),
				    "  %s: %d sessions, %d tasks, %ld sec runtime, %.1f%% success\n",
				    type_analytics[i]->type_name,
				    type_analytics[i]->session_count,
				    type_analytics[i]->tasks_completed,
				    type_analytics[i]->total_runtime,
				    type_analytics[i]->success_rate);
				strlcat(report, type_section, size);
			}
		}
	}

	return (report);
}

/*
 * Generate summary text
 */
char *
agent_analytics_generate_summary(void)
{
	struct agent_analytics	*a;
	char			*summary;
	size_t			 size;

	if (!analytics_initialized)
		agent_analytics_init();

	a = &global_analytics;
	size = 512;
	summary = xmalloc(size);

	snprintf(summary, size,
	    "Sessions: %d total (%d active), Tasks: %d, Goals: %d/%d (%.1f%%), "
	    "MCP: %d calls (%.1f%% success), Avg Duration: %.1f min",
	    a->total_sessions, a->active_sessions,
	    a->total_tasks_completed,
	    a->goals_completed, a->goals_registered, a->goal_completion_rate,
	    a->mcp_calls_total, a->mcp_success_rate,
	    a->avg_session_duration / 60.0);

	return (summary);
}

/*
 * Print analytics to cmdq
 */
void
agent_analytics_print(struct cmdq_item *item)
{
	char	*report;

	report = agent_analytics_generate_report();
	cmdq_print(item, "%s", report);
	free(report);
}
