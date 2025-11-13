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

#ifndef AGENT_LEARNING_H
#define AGENT_LEARNING_H

#include "session-agent.h"
#include "agent-analytics.h"

/*
 * Phase 4.4D: Agent Learning and Optimization
 *
 * This module provides intelligent learning from agent performance
 * to improve future sessions through pattern recognition and failure analysis.
 */

#define MAX_PATTERNS		50
#define MAX_PATTERN_NAME	128
#define MAX_PATTERN_DESC	512
#define MAX_FAILURE_REASONS	100
#define MAX_SUCCESS_FACTORS	100

/* Pattern types identified through learning */
enum pattern_type {
	PATTERN_SUCCESS,	/* Successful pattern */
	PATTERN_FAILURE,	/* Failure pattern */
	PATTERN_WORKFLOW,	/* Workflow pattern */
	PATTERN_EFFICIENCY	/* Efficiency pattern */
};

/* Learned pattern */
struct learned_pattern {
	enum pattern_type	 type;
	char			 name[MAX_PATTERN_NAME];
	char			 description[MAX_PATTERN_DESC];
	char			*agent_type;		/* Applicable agent type */

	/* Statistics */
	u_int			 occurrences;		/* How often seen */
	float			 success_rate;		/* Success rate */
	float			 confidence;		/* Pattern confidence */
	time_t			 first_seen;		/* First occurrence */
	time_t			 last_seen;		/* Last occurrence */

	/* Conditions */
	u_int			 min_tasks;		/* Min tasks for pattern */
	u_int			 max_duration;		/* Max duration (seconds) */
	float			 quality_threshold;	/* Min quality needed */

	struct learned_pattern	*next;
};

/* Failure reason */
struct failure_reason {
	char			*reason;		/* Reason description */
	char			*agent_type;		/* Affected agent type */
	u_int			 frequency;		/* How often occurred */
	float			 impact;		/* Impact score 0.0-1.0 */
	time_t			 last_occurrence;	/* Most recent */

	struct failure_reason	*next;
};

/* Success factor */
struct success_factor {
	char			*factor;		/* Factor description */
	char			*agent_type;		/* Applicable agent type */
	float			 correlation;		/* Success correlation */
	u_int			 occurrences;		/* How often present */
	time_t			 last_seen;		/* Most recent */

	struct success_factor	*next;
};

/* Learning system state */
struct agent_learning {
	/* Learned patterns */
	struct learned_pattern	*patterns;
	u_int			 pattern_count;

	/* Failure analysis */
	struct failure_reason	*failures;
	u_int			 failure_count;

	/* Success analysis */
	struct success_factor	*successes;
	u_int			 success_count;

	/* Learning statistics */
	u_int			 sessions_analyzed;
	time_t			 last_learning;
	float			 overall_improvement;	/* % improvement */
};

/* Initialize learning system */
void		 agent_learning_init(void);

/* Analyze completed session for learning */
int		 agent_learning_analyze_session(struct session_agent *,
		     struct agent_type_analytics *);

/* Identify patterns in successful sessions */
int		 agent_learning_identify_success_patterns(const char *);

/* Identify patterns in failed sessions */
int		 agent_learning_identify_failure_patterns(const char *);

/* Extract workflow patterns */
int		 agent_learning_extract_workflows(const char *);

/* Analyze failure reasons */
int		 agent_learning_analyze_failures(const char *);

/* Identify success factors */
int		 agent_learning_identify_success_factors(const char *);

/* Get learned patterns for agent type */
struct learned_pattern	*agent_learning_get_patterns(const char *, enum pattern_type);

/* Get failure reasons for agent type */
struct failure_reason	*agent_learning_get_failures(const char *);

/* Get success factors for agent type */
struct success_factor	*agent_learning_get_success_factors(const char *);

/* Apply learned patterns to new session */
int		 agent_learning_apply_patterns(struct session_agent *,
		     const char *);

/* Recommend improvements based on learning */
char		*agent_learning_recommend_improvements(const char *);

/* Get learning statistics */
struct agent_learning	*agent_learning_get_stats(void);

/* Free learning structures */
void		 agent_learning_free_pattern(struct learned_pattern *);
void		 agent_learning_free_failure(struct failure_reason *);
void		 agent_learning_free_success(struct success_factor *);

/* Export learning data to JSON */
char		*agent_learning_export_json(void);

/* Import learning data from JSON */
int		 agent_learning_import_json(const char *);

#endif /* !AGENT_LEARNING_H */
