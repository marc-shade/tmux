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
 * Leave an agent coordination group.
 */

static enum cmd_retval	cmd_agent_leave_group_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_agent_leave_group_entry = {
	.name = "agent-leave-group",
	.alias = "aleave",

	.args = { "t:", 0, 0, NULL },
	.usage = "[-t target-session]",

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_agent_leave_group_exec
};

static enum cmd_retval
cmd_agent_leave_group_exec(struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s;
	struct session_agent	*agent;
	struct session		*peer;
	const char		*group_name;
	int			 removed_peers = 0;

	(void)self;

	if (target->s == NULL) {
		cmdq_error(item, "no target session");
		return (CMD_RETURN_ERROR);
	}
	s = target->s;

	/* Check if session has agent metadata */
	agent = s->agent_metadata;
	if (agent == NULL) {
		cmdq_error(item, "session '%s' has no agent metadata", s->name);
		return (CMD_RETURN_ERROR);
	}

	/* Check if in a group */
	if (agent->coordination_group == NULL) {
		cmdq_error(item, "session '%s' is not in a coordination group",
		    s->name);
		return (CMD_RETURN_ERROR);
	}

	/* Save group name before leaving (it will be freed) */
	group_name = xstrdup(agent->coordination_group);
	cmdq_print(item, "Leaving group: %s", group_name);

	/* Remove this session from all peers' peer lists */
	RB_FOREACH(peer, sessions, &sessions) {
		if (peer == s || peer->agent_metadata == NULL)
			continue;

		if (peer->agent_metadata->coordination_group != NULL &&
		    strcmp(peer->agent_metadata->coordination_group, group_name) == 0) {
			if (session_agent_remove_peer(peer->agent_metadata,
			    s->name) == 0)
				removed_peers++;
		}
	}

	/* Leave the group */
	if (session_agent_leave_group(agent) != 0) {
		cmdq_error(item, "failed to leave group");
		return (CMD_RETURN_ERROR);
	}

	cmdq_print(item, "Session '%s' left group '%s'", s->name, group_name);
	if (removed_peers > 0)
		cmdq_print(item, "  Removed from %d peer session%s",
		    removed_peers, removed_peers == 1 ? "" : "s");

	free((char *)group_name);
	return (CMD_RETURN_NORMAL);
}
