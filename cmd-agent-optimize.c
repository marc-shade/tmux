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

#include "tmux.h"
#include "agent-optimizer.h"
#include "agent-learning.h"
#include "session-agent.h"

/*
 * Phase 4.4D: Agent Optimization Command
 *
 * Display optimization suggestions based on learned patterns
 * and apply optimizations to sessions.
 */

static enum cmd_retval		cmd_agent_optimize_exec(struct cmd *,
				    struct cmdq_item *);

const struct cmd_entry cmd_agent_optimize_entry = {
	.name = "agent-optimize",
	.alias = "optim",

	.args = { "s:t:", 0, 0, NULL },
	.usage = "[-s strategy] [-t agent-type]",

	.flags = 0,
	.exec = cmd_agent_optimize_exec
};

static enum cmd_retval
cmd_agent_optimize_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct session			*s = target->s;
	struct optimization_result	*result;
	const char			*agent_type, *strategy_str;
	enum optimization_strategy	 strategy;
	struct agent_learning		*learning;

	/* Get agent type */
	agent_type = args_get(args, 't');
	if (agent_type == NULL) {
		/* Try to get from current session */
		if (s == NULL || s->agent_metadata == NULL) {
			cmdq_error(item, "no agent type specified and no current session with agent");
			return (CMD_RETURN_ERROR);
		}
		agent_type = s->agent_metadata->agent_type;
	}

	/* Get strategy */
	strategy_str = args_get(args, 's');
	if (strategy_str == NULL) {
		strategy = OPT_AUTO;
	} else {
		if (strcmp(strategy_str, "workflow") == 0)
			strategy = OPT_WORKFLOW;
		else if (strcmp(strategy_str, "performance") == 0)
			strategy = OPT_PERFORMANCE;
		else if (strcmp(strategy_str, "efficiency") == 0)
			strategy = OPT_EFFICIENCY;
		else if (strcmp(strategy_str, "quality") == 0)
			strategy = OPT_QUALITY;
		else if (strcmp(strategy_str, "auto") == 0)
			strategy = OPT_AUTO;
		else {
			cmdq_error(item, "invalid strategy: %s", strategy_str);
			return (CMD_RETURN_ERROR);
		}
	}

	/* Display learning statistics */
	learning = agent_learning_get_stats();
	cmdq_print(item, "Agent Learning Statistics:");
	cmdq_print(item, "  Sessions Analyzed: %u", learning->sessions_analyzed);
	cmdq_print(item, "  Patterns Learned: %u", learning->pattern_count);
	cmdq_print(item, "  Failures Analyzed: %u", learning->failure_count);
	cmdq_print(item, "  Success Factors: %u", learning->success_count);
	cmdq_print(item, "");

	/* Generate optimization for fake session agent */
	struct session_agent fake_agent;
	memset(&fake_agent, 0, sizeof fake_agent);
	fake_agent.agent_type = xstrdup(agent_type);
	fake_agent.session_name = xstrdup("optimization");

	result = agent_optimizer_optimize(&fake_agent, strategy);

	free(fake_agent.agent_type);
	free(fake_agent.session_name);

	if (result == NULL) {
		cmdq_error(item, "failed to generate optimization");
		return (CMD_RETURN_ERROR);
	}

	/* Display optimization results */
	cmdq_print(item, "Optimization Results for '%s':", agent_type);
	cmdq_print(item, "  Strategy: %s", result->description);
	cmdq_print(item, "  Expected Improvement: %.1f%%", result->expected_improvement);
	cmdq_print(item, "  Confidence: %.0f%%", result->confidence * 100.0);
	cmdq_print(item, "");
	cmdq_print(item, "%s", result->recommendations);

	/* Display learning recommendations */
	char *recommendations = agent_learning_recommend_improvements(agent_type);
	if (recommendations != NULL) {
		cmdq_print(item, "");
		cmdq_print(item, "%s", recommendations);
		free(recommendations);
	}

	agent_optimizer_free_result(result);

	return (CMD_RETURN_NORMAL);
}
