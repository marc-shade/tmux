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
#include "agent-analytics.h"

/*
 * Display agent analytics and performance metrics.
 */

static enum cmd_retval	cmd_agent_analytics_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_agent_analytics_entry = {
	.name = "agent-analytics",
	.alias = "aanalytics",

	.args = { "st:", 0, 0, NULL },
	.usage = "[-s] [-t agent-type]",

	.flags = 0,
	.exec = cmd_agent_analytics_exec
};

static enum cmd_retval
cmd_agent_analytics_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct agent_type_analytics	*ta;
	const char			*type_name;
	char				*summary;
	int				 summary_only;

	summary_only = args_has(args, 's');
	type_name = args_get(args, 't');

	/* Summary mode */
	if (summary_only) {
		summary = agent_analytics_generate_summary();
		cmdq_print(item, "%s", summary);
		free(summary);
		return (CMD_RETURN_NORMAL);
	}

	/* Type-specific analytics */
	if (type_name != NULL) {
		ta = agent_analytics_get_by_type(type_name);
		if (ta == NULL) {
			cmdq_error(item, "no analytics for agent type '%s'",
			    type_name);
			return (CMD_RETURN_ERROR);
		}

		cmdq_print(item, "Agent Type: %s", ta->type_name);
		cmdq_print(item, "  Sessions: %d", ta->session_count);
		cmdq_print(item, "  Tasks Completed: %d", ta->tasks_completed);
		cmdq_print(item, "  Total Runtime: %ld seconds (%.1f hours)",
		    ta->total_runtime, ta->total_runtime / 3600.0);
		cmdq_print(item, "  Success Rate: %.1f%%", ta->success_rate);
		cmdq_print(item, "  Goal Completions: %d", ta->goal_completions);

		if (ta->session_count > 0) {
			cmdq_print(item, "  Avg Runtime: %.1f minutes",
			    (ta->total_runtime / ta->session_count) / 60.0);
			cmdq_print(item, "  Avg Tasks/Session: %.2f",
			    (float)ta->tasks_completed / ta->session_count);
		}

		return (CMD_RETURN_NORMAL);
	}

	/* Full analytics report */
	agent_analytics_print(item);
	return (CMD_RETURN_NORMAL);
}
