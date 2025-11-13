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
#include "agent-optimizer.h"
#include "agent-learning.h"
#include "session-agent.h"

/*
 * Phase 4.4D: Agent Optimizer
 *
 * Provides optimization strategies based on learned patterns:
 * - Workflow optimization
 * - Performance tuning
 * - Efficiency improvements
 * - Quality enhancement
 */

/* Initialize optimizer */
void
agent_optimizer_init(void)
{
	log_debug("Agent optimizer initialized");
}

/* Optimize agent configuration based on learning */
struct optimization_result *
agent_optimizer_optimize(struct session_agent *agent,
    enum optimization_strategy strategy)
{
	struct optimization_result	*result;
	char				*suggestions;
	float				 improvement;

	if (agent == NULL)
		return (NULL);

	/* Auto-select strategy if needed */
	if (strategy == OPT_AUTO)
		strategy = agent_optimizer_auto_strategy(agent->agent_type);

	/* Generate optimization result */
	result = xcalloc(1, sizeof *result);
	result->strategy = strategy;
	result->generated_at = time(NULL);

	/* Get suggestions based on strategy */
	switch (strategy) {
	case OPT_WORKFLOW:
		suggestions = agent_optimizer_suggest_workflow(agent->agent_type);
		result->description = xstrdup("Workflow optimization");
		break;
	case OPT_PERFORMANCE:
		suggestions = agent_optimizer_suggest_performance(agent->agent_type);
		result->description = xstrdup("Performance optimization");
		break;
	case OPT_EFFICIENCY:
		suggestions = agent_optimizer_suggest_efficiency(agent->agent_type);
		result->description = xstrdup("Efficiency optimization");
		break;
	case OPT_QUALITY:
		suggestions = agent_optimizer_suggest_quality(agent->agent_type);
		result->description = xstrdup("Quality optimization");
		break;
	default:
		suggestions = xstrdup("No optimizations available");
		result->description = xstrdup("Unknown strategy");
		break;
	}

	result->recommendations = suggestions;

	/* Calculate expected improvement */
	improvement = agent_optimizer_calculate_improvement(agent->agent_type,
	    strategy);
	result->expected_improvement = improvement;

	/* Set confidence based on learning data */
	result->confidence = improvement > 0.0 ? 0.7 : 0.3;

	log_debug("Generated optimization: strategy=%d improvement=%.1f%%",
	    strategy, improvement);

	return (result);
}

/* Suggest workflow improvements */
char *
agent_optimizer_suggest_workflow(const char *agent_type)
{
	struct learned_pattern	*patterns;
	char			*suggestions;
	size_t			 sug_len, offset;

	if (agent_type == NULL)
		return (xstrdup("No workflow suggestions"));

	sug_len = 1024;
	suggestions = xmalloc(sug_len);
	offset = 0;

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "Workflow Optimizations:\n\n");

	/* Get workflow patterns */
	patterns = agent_learning_get_patterns(agent_type, PATTERN_WORKFLOW);
	if (patterns != NULL) {
		offset += snprintf(suggestions + offset, sug_len - offset,
		    "Common Workflows:\n");
		for (; patterns != NULL && offset < sug_len - 256;
		    patterns = patterns->next) {
			offset += snprintf(suggestions + offset,
			    sug_len - offset,
			    "  - %s (%u times)\n",
			    patterns->description,
			    patterns->occurrences);
		}
	} else {
		offset += snprintf(suggestions + offset, sug_len - offset,
		    "  No workflow patterns learned yet\n");
	}

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "\nRecommendation: Follow established workflow patterns\n");

	return (suggestions);
}

/* Suggest performance improvements */
char *
agent_optimizer_suggest_performance(const char *agent_type)
{
	struct learned_pattern	*patterns;
	char			*suggestions;
	size_t			 sug_len, offset;

	if (agent_type == NULL)
		return (xstrdup("No performance suggestions"));

	sug_len = 1024;
	suggestions = xmalloc(sug_len);
	offset = 0;

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "Performance Optimizations:\n\n");

	/* Get success patterns for performance insights */
	patterns = agent_learning_get_patterns(agent_type, PATTERN_SUCCESS);
	if (patterns != NULL) {
		offset += snprintf(suggestions + offset, sug_len - offset,
		    "High-Performance Patterns:\n");
		for (; patterns != NULL && offset < sug_len - 256;
		    patterns = patterns->next) {
			if (patterns->success_rate > 0.7) {
				offset += snprintf(suggestions + offset,
				    sug_len - offset,
				    "  - %s (%.1f%% success)\n",
				    patterns->description,
				    patterns->success_rate * 100.0);
			}
		}
	}

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "\nRecommendation: Optimize based on high-success patterns\n");

	return (suggestions);
}

/* Suggest efficiency improvements */
char *
agent_optimizer_suggest_efficiency(const char *agent_type)
{
	struct failure_reason	*failures;
	char			*suggestions;
	size_t			 sug_len, offset;

	if (agent_type == NULL)
		return (xstrdup("No efficiency suggestions"));

	sug_len = 1024;
	suggestions = xmalloc(sug_len);
	offset = 0;

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "Efficiency Optimizations:\n\n");

	/* Get failure patterns to avoid inefficiencies */
	failures = agent_learning_get_failures(agent_type);
	if (failures != NULL) {
		offset += snprintf(suggestions + offset, sug_len - offset,
		    "Inefficiencies to Avoid:\n");
		for (; failures != NULL && offset < sug_len - 256;
		    failures = failures->next) {
			offset += snprintf(suggestions + offset,
			    sug_len - offset,
			    "  - %s (impact %.1f)\n",
			    failures->reason,
			    failures->impact);
		}
	}

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "\nRecommendation: Avoid known failure patterns\n");

	return (suggestions);
}

/* Suggest quality improvements */
char *
agent_optimizer_suggest_quality(const char *agent_type)
{
	struct success_factor	*factors;
	char			*suggestions;
	size_t			 sug_len, offset;

	if (agent_type == NULL)
		return (xstrdup("No quality suggestions"));

	sug_len = 1024;
	suggestions = xmalloc(sug_len);
	offset = 0;

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "Quality Optimizations:\n\n");

	/* Get success factors for quality insights */
	factors = agent_learning_get_success_factors(agent_type);
	if (factors != NULL) {
		offset += snprintf(suggestions + offset, sug_len - offset,
		    "Quality Factors:\n");
		for (; factors != NULL && offset < sug_len - 256;
		    factors = factors->next) {
			offset += snprintf(suggestions + offset,
			    sug_len - offset,
			    "  - %s (correlation %.2f)\n",
			    factors->factor,
			    factors->correlation);
		}
	}

	offset += snprintf(suggestions + offset, sug_len - offset,
	    "\nRecommendation: Focus on high-correlation success factors\n");

	return (suggestions);
}

/* Auto-select best optimization strategy */
enum optimization_strategy
agent_optimizer_auto_strategy(const char *agent_type)
{
	struct learned_pattern	*patterns;
	struct failure_reason	*failures;
	int			 success_count, failure_count;

	if (agent_type == NULL)
		return (OPT_WORKFLOW);

	/* Count patterns and failures */
	success_count = 0;
	failure_count = 0;

	for (patterns = agent_learning_get_patterns(agent_type, PATTERN_SUCCESS);
	    patterns != NULL; patterns = patterns->next)
		success_count++;

	for (failures = agent_learning_get_failures(agent_type);
	    failures != NULL; failures = failures->next)
		failure_count++;

	/* Select strategy based on data */
	if (failure_count > success_count)
		return (OPT_EFFICIENCY);	/* Focus on avoiding failures */
	else if (success_count > 5)
		return (OPT_PERFORMANCE);	/* Optimize for success */
	else
		return (OPT_WORKFLOW);		/* Learn workflow patterns */
}

/* Calculate expected improvement */
float
agent_optimizer_calculate_improvement(const char *agent_type,
    enum optimization_strategy strategy)
{
	struct learned_pattern	*patterns;
	struct agent_learning	*learning;
	float			 improvement;

	if (agent_type == NULL)
		return (0.0);

	learning = agent_learning_get_stats();
	if (learning->sessions_analyzed < 5)
		return (0.0);	/* Not enough data */

	/* Calculate improvement based on strategy and learning */
	improvement = 0.0;

	switch (strategy) {
	case OPT_WORKFLOW:
		patterns = agent_learning_get_patterns(agent_type,
		    PATTERN_WORKFLOW);
		if (patterns != NULL)
			improvement = 10.0;	/* 10% improvement expected */
		break;

	case OPT_PERFORMANCE:
		patterns = agent_learning_get_patterns(agent_type,
		    PATTERN_SUCCESS);
		if (patterns != NULL && patterns->success_rate > 0.7)
			improvement = 15.0;	/* 15% improvement expected */
		break;

	case OPT_EFFICIENCY:
		improvement = 12.0;	/* 12% efficiency gain expected */
		break;

	case OPT_QUALITY:
		improvement = 8.0;	/* 8% quality improvement expected */
		break;

	default:
		improvement = 5.0;	/* 5% generic improvement */
		break;
	}

	return (improvement);
}

/* Apply optimization to session */
int
agent_optimizer_apply(struct session_agent *agent,
    struct optimization_result *result)
{
	/* For MVP, just log the optimization */
	if (agent == NULL || result == NULL)
		return (-1);

	log_debug("Applying optimization to %s: %s (expected %.1f%% improvement)",
	    agent->session_name,
	    result->description,
	    result->expected_improvement);

	return (0);
}

/* Get optimization history */
struct optimization_result *
agent_optimizer_get_history(const char *agent_type)
{
	/* For MVP, return NULL as we don't persist optimization history */
	(void)agent_type;	/* Suppress unused warning */

	return (NULL);
}

/* Free optimization result */
void
agent_optimizer_free_result(struct optimization_result *result)
{
	if (result == NULL)
		return;

	free(result->description);
	free(result->recommendations);
	free(result);
}
