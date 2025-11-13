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
#include "session-agent.h"

/*
 * List all agent coordination groups.
 */

static enum cmd_retval	cmd_list_agent_groups_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_list_agent_groups_entry = {
	.name = "list-agent-groups",
	.alias = "lsag",

	.args = { "", 0, 0, NULL },
	.usage = "",

	.target = { 0, 0, 0 },

	.flags = 0,
	.exec = cmd_list_agent_groups_exec
};

/* Helper structure to track unique groups */
struct group_info {
	char		*name;
	int		 member_count;
	int		 coordinator_count;
	struct session	**members;
	int		 max_members;
};

static enum cmd_retval
cmd_list_agent_groups_exec(struct cmd *self, struct cmdq_item *item)
{
	struct session		*s;
	struct session_agent	*agent;
	struct group_info	*groups = NULL;
	int			 num_groups = 0, max_groups = 16;
	int			 i, j, found;

	(void)self;

	/* Allocate initial group tracking array */
	groups = xcalloc(max_groups, sizeof(struct group_info));

	/* Iterate all sessions to find groups */
	RB_FOREACH(s, sessions, &sessions) {
		agent = s->agent_metadata;
		if (agent == NULL || agent->coordination_group == NULL)
			continue;

		/* Check if group already tracked */
		found = -1;
		for (i = 0; i < num_groups; i++) {
			if (strcmp(groups[i].name, agent->coordination_group) == 0) {
				found = i;
				break;
			}
		}

		if (found < 0) {
			/* New group - add to list */
			if (num_groups >= max_groups) {
				/* Expand array */
				max_groups *= 2;
				groups = xreallocarray(groups, max_groups,
				    sizeof(struct group_info));
			}

			groups[num_groups].name = xstrdup(agent->coordination_group);
			groups[num_groups].member_count = 0;
			groups[num_groups].coordinator_count = 0;
			groups[num_groups].max_members = 32;
			groups[num_groups].members = xcalloc(
			    groups[num_groups].max_members, sizeof(struct session *));

			found = num_groups;
			num_groups++;
		}

		/* Add session to group */
		if (groups[found].member_count < groups[found].max_members) {
			groups[found].members[groups[found].member_count++] = s;
			if (agent->is_coordinator)
				groups[found].coordinator_count++;
		}
	}

	/* Display results */
	if (num_groups == 0) {
		cmdq_print(item, "No coordination groups found");
		free(groups);
		return (CMD_RETURN_NORMAL);
	}

	cmdq_print(item, "Agent Coordination Groups: %d", num_groups);
	cmdq_print(item, "");

	for (i = 0; i < num_groups; i++) {
		cmdq_print(item, "Group: %s", groups[i].name);
		cmdq_print(item, "  Members: %d", groups[i].member_count);
		cmdq_print(item, "  Coordinators: %d",
		    groups[i].coordinator_count);

		for (j = 0; j < groups[i].member_count; j++) {
			s = groups[i].members[j];
			agent = s->agent_metadata;
			cmdq_print(item, "    - %s [%s]%s",
			    s->name,
			    agent->agent_type,
			    agent->is_coordinator ? " (coordinator)" : "");
		}

		/* Display shared context size if any member has context */
		for (j = 0; j < groups[i].member_count; j++) {
			agent = groups[i].members[j]->agent_metadata;
			if (agent->shared_context != NULL &&
			    agent->shared_context_len > 0) {
				cmdq_print(item, "  Shared Context: %zu bytes",
				    agent->shared_context_len);
				break;
			}
		}

		if (i < num_groups - 1)
			cmdq_print(item, "");
	}

	/* Cleanup */
	for (i = 0; i < num_groups; i++) {
		free(groups[i].name);
		free(groups[i].members);
	}
	free(groups);

	return (CMD_RETURN_NORMAL);
}
