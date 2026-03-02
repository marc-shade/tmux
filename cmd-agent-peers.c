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
 * List peer sessions in agent coordination group.
 */

static enum cmd_retval	cmd_agent_peers_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_agent_peers_entry = {
	.name = "agent-peers",
	.alias = "apeers",

	.args = { "t:", 0, 0, NULL },
	.usage = "[-t target-session]",

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_agent_peers_exec
};

static enum cmd_retval
cmd_agent_peers_exec(struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s;
	struct session_agent	*agent;
	const char		**peers;
	int			 num_peers, i;
	time_t			 now;
	char			 time_buf[64];

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
		cmdq_print(item, "Use 'agent-join-group' to join a group first");
		return (CMD_RETURN_ERROR);
	}

	/* Display group information */
	cmdq_print(item, "Coordination Group: %s", agent->coordination_group);
	cmdq_print(item, "Role: %s",
	    agent->is_coordinator ? "Coordinator" : "Member");

	/* Display last coordination time */
	now = time(NULL);
	if (agent->last_coordination > 0) {
		strftime(time_buf, sizeof time_buf, "%Y-%m-%d %H:%M:%S",
		    localtime(&agent->last_coordination));
		cmdq_print(item, "Last Coordination: %s (%ld seconds ago)",
		    time_buf, (long)(now - agent->last_coordination));
	}

	/* List peer sessions */
	peers = session_agent_list_peers(agent, &num_peers);
	if (peers == NULL || num_peers == 0) {
		cmdq_print(item, "Peers: None (only session in group)");
	} else {
		cmdq_print(item, "Peers: %d session%s",
		    num_peers, num_peers == 1 ? "" : "s");
		for (i = 0; i < num_peers; i++)
			cmdq_print(item, "  - %s", peers[i]);
	}

	/* Display shared context */
	if (agent->shared_context != NULL && agent->shared_context_len > 0) {
		cmdq_print(item, "Shared Context: %zu bytes",
		    agent->shared_context_len);
		cmdq_print(item, "%s", agent->shared_context);
	} else {
		cmdq_print(item, "Shared Context: Empty");
	}

	return (CMD_RETURN_NORMAL);
}
