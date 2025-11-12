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

/*
 * Show agent information for a session.
 */

static enum cmd_retval	cmd_show_agent_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_show_agent_entry = {
	.name = "show-agent",
	.alias = NULL,

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_show_agent_exec
};

static enum cmd_retval
cmd_show_agent_exec(__unused struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s;
	struct session_agent	*agent;
	time_t			 now;
	char			 created[26], activity[26];

	if (s->agent_metadata == NULL) {
		cmdq_print(item, "session %s has no agent metadata", s->name);
		return (CMD_RETURN_NORMAL);
	}

	agent = s->agent_metadata;
	now = time(NULL);

	/* Format timestamps */
	ctime_r(&agent->created, created);
	created[strcspn(created, "\n")] = '\0';

	ctime_r(&agent->last_activity, activity);
	activity[strcspn(activity, "\n")] = '\0';

	/* Print agent information */
	cmdq_print(item, "Session: %s", s->name);
	cmdq_print(item, "Agent Type: %s", agent->agent_type);
	cmdq_print(item, "Goal: %s", agent->goal);
	cmdq_print(item, "Created: %s (%ld seconds ago)", created,
	    (long)(now - agent->created));
	cmdq_print(item, "Last Activity: %s (%ld seconds ago)", activity,
	    (long)(now - agent->last_activity));
	cmdq_print(item, "Tasks Completed: %u", agent->tasks_completed);
	cmdq_print(item, "Interactions: %u", agent->interactions);

	if (agent->runtime_goal_id != NULL)
		cmdq_print(item, "Runtime Goal ID: %s", agent->runtime_goal_id);
	else
		cmdq_print(item, "Runtime Goal ID: (not registered)");

	if (agent->context_key != NULL)
		cmdq_print(item, "Context Key: %s", agent->context_key);

	if (agent->context_saved)
		cmdq_print(item, "Context: saved");
	else
		cmdq_print(item, "Context: not saved");

	return (CMD_RETURN_NORMAL);
}
