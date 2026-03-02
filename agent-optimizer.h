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

#ifndef AGENT_OPTIMIZER_H
#define AGENT_OPTIMIZER_H

#include "session-agent.h"
#include "agent-learning.h"

/*
 * Phase 4.4D: Agent Optimization
 *
 * This module provides optimization strategies based on learned patterns
 * to improve agent performance and efficiency.
 */

/* Optimization strategies */
enum optimization_strategy {
	OPT_WORKFLOW,		/* Optimize workflow */
	OPT_PERFORMANCE,	/* Optimize performance */
	OPT_EFFICIENCY,		/* Optimize efficiency */
	OPT_QUALITY,		/* Optimize quality */
	OPT_AUTO		/* Automatic strategy selection */
};

/* Optimization result */
struct optimization_result {
	enum optimization_strategy	 strategy;
	char				*description;
	float				 expected_improvement;	/* % */
	float				 confidence;		/* 0.0-1.0 */
	char				*recommendations;
	time_t				 generated_at;
};

/* Initialize optimizer */
void		 agent_optimizer_init(void);

/* Optimize agent configuration based on learning */
struct optimization_result	*agent_optimizer_optimize(struct session_agent *,
				     enum optimization_strategy);

/* Suggest workflow improvements */
char		*agent_optimizer_suggest_workflow(const char *);

/* Suggest performance improvements */
char		*agent_optimizer_suggest_performance(const char *);

/* Suggest efficiency improvements */
char		*agent_optimizer_suggest_efficiency(const char *);

/* Suggest quality improvements */
char		*agent_optimizer_suggest_quality(const char *);

/* Auto-select best optimization strategy */
enum optimization_strategy	 agent_optimizer_auto_strategy(const char *);

/* Calculate expected improvement */
float		 agent_optimizer_calculate_improvement(const char *,
		     enum optimization_strategy);

/* Apply optimization to session */
int		 agent_optimizer_apply(struct session_agent *,
		     struct optimization_result *);

/* Get optimization history */
struct optimization_result	*agent_optimizer_get_history(const char *);

/* Free optimization result */
void		 agent_optimizer_free_result(struct optimization_result *);

#endif /* !AGENT_OPTIMIZER_H */
