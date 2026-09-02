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
#include "session-agent.h"

/*
 * Display an agent dashboard showing real-time status of all agents,
 * coordination groups, and analytics summary. Designed to be used with
 * display-popup for a floating dashboard experience.
 *
 * Usage:
 *   agent-dashboard            # Full dashboard
 *   agent-dashboard -s         # Summary only
 *
 * Recommended keybinding:
 *   bind A display-popup -w 80 -h 24 "tmux agent-dashboard"
 */

static enum cmd_retval	cmd_agent_dashboard_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_agent_dashboard_entry = {
	.name = "agent-dashboard",
	.alias = "adash",

	.args = { "s", 0, 0, NULL },
	.usage = "[-s]",

	.flags = CMD_AFTERHOOK|CMD_EXTENSION,
	.exec = cmd_agent_dashboard_exec
};

static enum cmd_retval
cmd_agent_dashboard_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct session		*s;
	struct session_agent	*sa;
	int			 summary_only;
	int			 total_sessions = 0, agent_sessions = 0;
	int			 total_groups = 0;
	u_int			 total_tasks = 0, total_interactions = 0;
	char			*seen_groups[64];
	int			 nseen = 0;
	time_t			 now = time(NULL);

	summary_only = args_has(args, 's');

	/* Header */
	cmdq_print(item, "");
	cmdq_print(item,
	    "  === Agent Dashboard ===");
	cmdq_print(item, "");

	/* Count sessions and collect stats. */
	RB_FOREACH(s, sessions, &sessions) {
		total_sessions++;
		sa = s->agent_metadata;
		if (sa == NULL)
			continue;

		agent_sessions++;
		total_tasks += sa->tasks_completed;
		total_interactions += sa->interactions;

		/* Track unique groups. */
		if (sa->coordination_group != NULL) {
			int	 i, found = 0;
			for (i = 0; i < nseen; i++) {
				if (strcmp(seen_groups[i],
				    sa->coordination_group) == 0) {
					found = 1;
					break;
				}
			}
			if (!found && nseen < 64)
				seen_groups[nseen++] =
				    sa->coordination_group;
		}
	}
	total_groups = nseen;

	/* Summary line. */
	cmdq_print(item,
	    "  Sessions: %d total, %d agentic | Groups: %d | "
	    "Tasks: %u | Interactions: %u",
	    total_sessions, agent_sessions, total_groups,
	    total_tasks, total_interactions);
	cmdq_print(item, "");

	if (summary_only)
		return (CMD_RETURN_NORMAL);

	/* Agent sessions detail. */
	if (agent_sessions > 0) {
		cmdq_print(item,
		    "  %-16s %-12s %-6s %-6s %-8s %s",
		    "SESSION", "TYPE", "TASKS", "INTER", "DURATION", "GOAL");
		cmdq_print(item,
		    "  %-16s %-12s %-6s %-6s %-8s %s",
		    "-------", "----", "-----", "-----", "--------", "----");

		RB_FOREACH(s, sessions, &sessions) {
			sa = s->agent_metadata;
			if (sa == NULL)
				continue;

			{
				long	 dur = (long)(now - sa->created);
				int	 hours = (int)(dur / 3600);
				int	 mins = (int)((dur % 3600) / 60);
				char	 durbuf[32];
				char	 goalbuf[40];

				if (hours > 0)
					snprintf(durbuf, sizeof durbuf,
					    "%dh%dm", hours, mins);
				else
					snprintf(durbuf, sizeof durbuf,
					    "%dm", mins);

				/* Truncate goal for display. */
				if (sa->goal != NULL &&
				    strlen(sa->goal) > 38) {
					snprintf(goalbuf, sizeof goalbuf,
					    "%.37s...", sa->goal);
				} else if (sa->goal != NULL) {
					snprintf(goalbuf, sizeof goalbuf,
					    "%s", sa->goal);
				} else {
					snprintf(goalbuf, sizeof goalbuf,
					    "(none)");
				}

				cmdq_print(item,
				    "  %-16s %-12s %-6u %-6u %-8s %s",
				    s->name,
				    sa->agent_type ? sa->agent_type : "-",
				    sa->tasks_completed,
				    sa->interactions,
				    durbuf, goalbuf);
			}
		}
		cmdq_print(item, "");
	}

	/* Coordination groups. */
	if (total_groups > 0) {
		int	 g;

		cmdq_print(item, "  Coordination Groups:");
		cmdq_print(item, "");

		for (g = 0; g < nseen; g++) {
			int	 members = 0;
			char	*coordinator = NULL;

			RB_FOREACH(s, sessions, &sessions) {
				sa = s->agent_metadata;
				if (sa == NULL ||
				    sa->coordination_group == NULL)
					continue;
				if (strcmp(sa->coordination_group,
				    seen_groups[g]) != 0)
					continue;
				members++;
				if (sa->is_coordinator)
					coordinator = s->name;
			}

			cmdq_print(item,
			    "    [%s] %d member(s), coordinator: %s",
			    seen_groups[g], members,
			    coordinator ? coordinator : "(none)");
		}
		cmdq_print(item, "");
	}

	/* Format string hint. */
	cmdq_print(item,
	    "  Tip: Add to .tmux.conf for status bar integration:");
	cmdq_print(item,
	    "    set -g status-right "
	    "'#{?agent_type,[#{agent_type}] #{agent_goal},}'");
	cmdq_print(item,
	    "    bind A display-popup -w 80 -h 24 "
	    "\"tmux agent-dashboard\"");
	cmdq_print(item, "");

	return (CMD_RETURN_NORMAL);
}
