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
 * Join an agent coordination group.
 */

static enum cmd_retval	cmd_agent_join_group_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_agent_join_group_entry = {
	.name = "agent-join-group",
	.alias = "ajoin",

	.args = { "t:", 1, 1, NULL },
	.usage = "[-t target-session] group-name",

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_agent_join_group_exec
};

static enum cmd_retval
cmd_agent_join_group_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s;
	struct session_agent	*agent;
	const char		*group_name;
	struct session		*peer;
	int			 found_peers = 0;

	if (target->s == NULL) {
		cmdq_error(item, "no target session");
		return (CMD_RETURN_ERROR);
	}
	s = target->s;

	group_name = args_string(args, 0);
	if (group_name == NULL || strlen(group_name) == 0) {
		cmdq_error(item, "group name required");
		return (CMD_RETURN_ERROR);
	}

	/* Check if session has agent metadata */
	agent = s->agent_metadata;
	if (agent == NULL) {
		cmdq_error(item, "session '%s' has no agent metadata", s->name);
		cmdq_print(item, "Use 'new-session -G <type> -o <goal>' to create agent-aware session");
		return (CMD_RETURN_ERROR);
	}

	/* Join the group */
	if (session_agent_join_group(agent, group_name) != 0) {
		cmdq_error(item, "failed to join group '%s'", group_name);
		return (CMD_RETURN_ERROR);
	}

	/* Discover and add existing group members as peers */
	RB_FOREACH(peer, sessions, &sessions) {
		if (peer == s || peer->agent_metadata == NULL)
			continue;

		/* Check if peer is in same group */
		if (peer->agent_metadata->coordination_group != NULL &&
		    strcmp(peer->agent_metadata->coordination_group, group_name) == 0) {
			/* Add to this agent's peer list */
			session_agent_add_peer(agent, peer->name);

			/* Add this session to peer's list */
			session_agent_add_peer(peer->agent_metadata, s->name);

			found_peers++;
		}
	}

	/* If we found peers, we're not the coordinator */
	if (found_peers > 0)
		agent->is_coordinator = 0;

	cmdq_print(item, "Session '%s' joined group '%s'", s->name, group_name);
	if (found_peers > 0)
		cmdq_print(item, "  Discovered %d peer session%s",
		    found_peers, found_peers == 1 ? "" : "s");
	else
		cmdq_print(item, "  First session in group (coordinator)");

	return (CMD_RETURN_NORMAL);
}
