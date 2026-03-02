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
#include "agent-learning.h"
#include "agent-analytics.h"
#include "session-agent.h"

/*
 * Phase 4.4D: Agent Learning Engine
 *
 * Intelligent learning from agent performance:
 * - Pattern recognition in successful sessions
 * - Failure analysis and avoidance
 * - Workflow optimization suggestions
 * - Automatic parameter tuning
 */

/* Global learning state */
static struct agent_learning	global_learning;

/* Initialize learning system */
void
agent_learning_init(void)
{
	memset(&global_learning, 0, sizeof global_learning);
	log_debug("Agent learning system initialized");
}

/* Analyze completed session for learning */
int
agent_learning_analyze_session(struct session_agent *agent,
    struct agent_type_analytics *analytics)
{
	int	success;

	if (agent == NULL || analytics == NULL)
		return (-1);

	/* Determine if session was successful */
	success = (agent->runtime_goal_id != NULL &&
	    agent->tasks_completed > 0);

	/* Update learning stats */
	global_learning.sessions_analyzed++;
	global_learning.last_learning = time(NULL);

	if (success) {
		/* Analyze for success patterns */
		agent_learning_identify_success_patterns(agent->agent_type);
		agent_learning_identify_success_factors(agent->agent_type);
		agent_learning_extract_workflows(agent->agent_type);
	} else {
		/* Analyze for failure patterns */
		agent_learning_identify_failure_patterns(agent->agent_type);
		agent_learning_analyze_failures(agent->agent_type);
	}

	log_debug("Session analyzed for learning: type=%s success=%d",
	    agent->agent_type, success);

	return (0);
}

/* Identify patterns in successful sessions */
int
agent_learning_identify_success_patterns(const char *agent_type)
{
	struct learned_pattern	*pattern, *existing;
	char			 pattern_name[MAX_PATTERN_NAME];

	if (agent_type == NULL)
		return (-1);

	/* Create or update success pattern */
	snprintf(pattern_name, sizeof pattern_name,
	    "success_%s", agent_type);

	/* Check if pattern exists */
	for (existing = global_learning.patterns; existing != NULL;
	    existing = existing->next) {
		if (strcmp(existing->name, pattern_name) == 0) {
			/* Update existing pattern */
			existing->occurrences++;
			existing->last_seen = time(NULL);
			existing->success_rate = (existing->success_rate *
			    (existing->occurrences - 1) + 1.0) /
			    existing->occurrences;
			existing->confidence = existing->success_rate;
			return (0);
		}
	}

	/* Create new pattern */
	pattern = xcalloc(1, sizeof *pattern);
	pattern->type = PATTERN_SUCCESS;
	strlcpy(pattern->name, pattern_name, sizeof pattern->name);
	snprintf(pattern->description, sizeof pattern->description,
	    "Successful %s session pattern", agent_type);
	pattern->agent_type = xstrdup(agent_type);
	pattern->occurrences = 1;
	pattern->success_rate = 1.0;
	pattern->confidence = 0.5;	/* Low confidence initially */
	pattern->first_seen = time(NULL);
	pattern->last_seen = time(NULL);

	/* Add to list */
	pattern->next = global_learning.patterns;
	global_learning.patterns = pattern;
	global_learning.pattern_count++;

	return (0);
}

/* Identify patterns in failed sessions */
int
agent_learning_identify_failure_patterns(const char *agent_type)
{
	struct learned_pattern	*pattern, *existing;
	char			 pattern_name[MAX_PATTERN_NAME];

	if (agent_type == NULL)
		return (-1);

	/* Create or update failure pattern */
	snprintf(pattern_name, sizeof pattern_name,
	    "failure_%s", agent_type);

	/* Check if pattern exists */
	for (existing = global_learning.patterns; existing != NULL;
	    existing = existing->next) {
		if (strcmp(existing->name, pattern_name) == 0) {
			/* Update existing pattern */
			existing->occurrences++;
			existing->last_seen = time(NULL);
			existing->success_rate = (existing->success_rate *
			    (existing->occurrences - 1) + 0.0) /
			    existing->occurrences;
			existing->confidence = 1.0 - existing->success_rate;
			return (0);
		}
	}

	/* Create new pattern */
	pattern = xcalloc(1, sizeof *pattern);
	pattern->type = PATTERN_FAILURE;
	strlcpy(pattern->name, pattern_name, sizeof pattern->name);
	snprintf(pattern->description, sizeof pattern->description,
	    "Failed %s session pattern", agent_type);
	pattern->agent_type = xstrdup(agent_type);
	pattern->occurrences = 1;
	pattern->success_rate = 0.0;
	pattern->confidence = 0.5;
	pattern->first_seen = time(NULL);
	pattern->last_seen = time(NULL);

	/* Add to list */
	pattern->next = global_learning.patterns;
	global_learning.patterns = pattern;
	global_learning.pattern_count++;

	return (0);
}

/* Extract workflow patterns */
int
agent_learning_extract_workflows(const char *agent_type)
{
	struct learned_pattern	*pattern, *existing;
	char			 pattern_name[MAX_PATTERN_NAME];

	if (agent_type == NULL)
		return (-1);

	/* Create or update workflow pattern */
	snprintf(pattern_name, sizeof pattern_name,
	    "workflow_%s", agent_type);

	/* Check if pattern exists */
	for (existing = global_learning.patterns; existing != NULL;
	    existing = existing->next) {
		if (strcmp(existing->name, pattern_name) == 0) {
			existing->occurrences++;
			existing->last_seen = time(NULL);
			return (0);
		}
	}

	/* Create new pattern */
	pattern = xcalloc(1, sizeof *pattern);
	pattern->type = PATTERN_WORKFLOW;
	strlcpy(pattern->name, pattern_name, sizeof pattern->name);
	snprintf(pattern->description, sizeof pattern->description,
	    "Common workflow for %s sessions", agent_type);
	pattern->agent_type = xstrdup(agent_type);
	pattern->occurrences = 1;
	pattern->success_rate = 0.5;
	pattern->confidence = 0.5;
	pattern->first_seen = time(NULL);
	pattern->last_seen = time(NULL);

	/* Add to list */
	pattern->next = global_learning.patterns;
	global_learning.patterns = pattern;
	global_learning.pattern_count++;

	return (0);
}

/* Analyze failure reasons */
int
agent_learning_analyze_failures(const char *agent_type)
{
	struct failure_reason	*reason, *existing;
	char			 reason_text[256];

	if (agent_type == NULL)
		return (-1);

	/* Generic failure reason for MVP */
	snprintf(reason_text, sizeof reason_text,
	    "Goal not completed for %s session", agent_type);

	/* Check if reason exists */
	for (existing = global_learning.failures; existing != NULL;
	    existing = existing->next) {
		if (strcmp(existing->reason, reason_text) == 0) {
			existing->frequency++;
			existing->last_occurrence = time(NULL);
			/* Increase impact based on frequency */
			existing->impact = (existing->impact *
			    (existing->frequency - 1) + 0.5) /
			    existing->frequency;
			return (0);
		}
	}

	/* Create new failure reason */
	reason = xcalloc(1, sizeof *reason);
	reason->reason = xstrdup(reason_text);
	reason->agent_type = xstrdup(agent_type);
	reason->frequency = 1;
	reason->impact = 0.5;
	reason->last_occurrence = time(NULL);

	/* Add to list */
	reason->next = global_learning.failures;
	global_learning.failures = reason;
	global_learning.failure_count++;

	return (0);
}

/* Identify success factors */
int
agent_learning_identify_success_factors(const char *agent_type)
{
	struct success_factor	*factor, *existing;
	char			 factor_text[256];

	if (agent_type == NULL)
		return (-1);

	/* Generic success factor for MVP */
	snprintf(factor_text, sizeof factor_text,
	    "Tasks completed for %s session", agent_type);

	/* Check if factor exists */
	for (existing = global_learning.successes; existing != NULL;
	    existing = existing->next) {
		if (strcmp(existing->factor, factor_text) == 0) {
			existing->occurrences++;
			existing->last_seen = time(NULL);
			/* Increase correlation based on occurrences */
			existing->correlation = (existing->correlation *
			    (existing->occurrences - 1) + 0.8) /
			    existing->occurrences;
			return (0);
		}
	}

	/* Create new success factor */
	factor = xcalloc(1, sizeof *factor);
	factor->factor = xstrdup(factor_text);
	factor->agent_type = xstrdup(agent_type);
	factor->correlation = 0.8;
	factor->occurrences = 1;
	factor->last_seen = time(NULL);

	/* Add to list */
	factor->next = global_learning.successes;
	global_learning.successes = factor;
	global_learning.success_count++;

	return (0);
}

/* Get learned patterns for agent type */
struct learned_pattern *
agent_learning_get_patterns(const char *agent_type, enum pattern_type type)
{
	struct learned_pattern	*pattern, *result, *last;

	if (agent_type == NULL)
		return (NULL);

	result = NULL;
	last = NULL;

	/* Filter patterns by agent type and type */
	for (pattern = global_learning.patterns; pattern != NULL;
	    pattern = pattern->next) {
		if (pattern->agent_type != NULL &&
		    strcmp(pattern->agent_type, agent_type) == 0 &&
		    pattern->type == type) {
			/* Add to result list (just references) */
			if (result == NULL) {
				result = pattern;
				last = pattern;
			} else {
				last->next = pattern;
				last = pattern;
			}
		}
	}

	if (last != NULL)
		last->next = NULL;

	return (result);
}

/* Get failure reasons for agent type */
struct failure_reason *
agent_learning_get_failures(const char *agent_type)
{
	struct failure_reason	*reason, *result, *last;

	if (agent_type == NULL)
		return (NULL);

	result = NULL;
	last = NULL;

	/* Filter failures by agent type */
	for (reason = global_learning.failures; reason != NULL;
	    reason = reason->next) {
		if (reason->agent_type != NULL &&
		    strcmp(reason->agent_type, agent_type) == 0) {
			if (result == NULL) {
				result = reason;
				last = reason;
			} else {
				last->next = reason;
				last = reason;
			}
		}
	}

	if (last != NULL)
		last->next = NULL;

	return (result);
}

/* Get success factors for agent type */
struct success_factor *
agent_learning_get_success_factors(const char *agent_type)
{
	struct success_factor	*factor, *result, *last;

	if (agent_type == NULL)
		return (NULL);

	result = NULL;
	last = NULL;

	/* Filter success factors by agent type */
	for (factor = global_learning.successes; factor != NULL;
	    factor = factor->next) {
		if (factor->agent_type != NULL &&
		    strcmp(factor->agent_type, agent_type) == 0) {
			if (result == NULL) {
				result = factor;
				last = factor;
			} else {
				last->next = factor;
				last = factor;
			}
		}
	}

	if (last != NULL)
		last->next = NULL;

	return (result);
}

/* Apply learned patterns to new session */
int
agent_learning_apply_patterns(struct session_agent *agent,
    const char *recommendations)
{
	/* For MVP, just log the recommendations */
	if (agent == NULL || recommendations == NULL)
		return (-1);

	log_debug("Applying learning to %s: %s",
	    agent->session_name, recommendations);

	return (0);
}

/* Recommend improvements based on learning */
char *
agent_learning_recommend_improvements(const char *agent_type)
{
	struct learned_pattern	*pattern;
	struct failure_reason	*reason;
	struct success_factor	*factor;
	char			*recommendations;
	size_t			 rec_len, offset;

	if (agent_type == NULL)
		return (xstrdup("No recommendations available"));

	/* Allocate recommendations buffer */
	rec_len = 2048;
	recommendations = xmalloc(rec_len);
	offset = 0;

	offset += snprintf(recommendations + offset, rec_len - offset,
	    "Learned Recommendations for %s:\n\n", agent_type);

	/* Success patterns */
	pattern = agent_learning_get_patterns(agent_type, PATTERN_SUCCESS);
	if (pattern != NULL) {
		offset += snprintf(recommendations + offset, rec_len - offset,
		    "Success Patterns:\n");
		for (; pattern != NULL && offset < rec_len - 256;
		    pattern = pattern->next) {
			offset += snprintf(recommendations + offset,
			    rec_len - offset,
			    "  - %s (%.1f%% success, %u occurrences)\n",
			    pattern->description,
			    pattern->success_rate * 100.0,
			    pattern->occurrences);
		}
		offset += snprintf(recommendations + offset, rec_len - offset,
		    "\n");
	}

	/* Common failures */
	reason = agent_learning_get_failures(agent_type);
	if (reason != NULL) {
		offset += snprintf(recommendations + offset, rec_len - offset,
		    "Common Failures to Avoid:\n");
		for (; reason != NULL && offset < rec_len - 256;
		    reason = reason->next) {
			offset += snprintf(recommendations + offset,
			    rec_len - offset,
			    "  - %s (occurred %u times, impact %.1f)\n",
			    reason->reason,
			    reason->frequency,
			    reason->impact);
		}
		offset += snprintf(recommendations + offset, rec_len - offset,
		    "\n");
	}

	/* Success factors */
	factor = agent_learning_get_success_factors(agent_type);
	if (factor != NULL) {
		offset += snprintf(recommendations + offset, rec_len - offset,
		    "Key Success Factors:\n");
		for (; factor != NULL && offset < rec_len - 256;
		    factor = factor->next) {
			offset += snprintf(recommendations + offset,
			    rec_len - offset,
			    "  - %s (correlation %.2f, seen %u times)\n",
			    factor->factor,
			    factor->correlation,
			    factor->occurrences);
		}
	}

	return (recommendations);
}

/* Get learning statistics */
struct agent_learning *
agent_learning_get_stats(void)
{
	return (&global_learning);
}

/* Free learning structures */
void
agent_learning_free_pattern(struct learned_pattern *pattern)
{
	if (pattern == NULL)
		return;

	free(pattern->agent_type);
	free(pattern);
}

void
agent_learning_free_failure(struct failure_reason *reason)
{
	if (reason == NULL)
		return;

	free(reason->reason);
	free(reason->agent_type);
	free(reason);
}

void
agent_learning_free_success(struct success_factor *factor)
{
	if (factor == NULL)
		return;

	free(factor->factor);
	free(factor->agent_type);
	free(factor);
}

/* Export learning data to JSON */
char *
agent_learning_export_json(void)
{
	/* TODO: Implement JSON export */
	return (xstrdup("{}"));
}

/* Import learning data from JSON */
int
agent_learning_import_json(const char *json)
{
	/* TODO: Implement JSON import */
	(void)json;	/* Suppress unused warning */

	return (-1);
}
