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
 * Share context with agent coordination group.
 */

static enum cmd_retval	cmd_agent_share_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_agent_share_entry = {
	.name = "agent-share",
	.alias = "ashare",

	.args = { "t:", 1, 1, NULL },
	.usage = "[-t target-session] key=value",

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_agent_share_exec
};

static enum cmd_retval
cmd_agent_share_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s;
	struct session_agent	*agent;
	const char		*kv, *eq;
	char			*key, *value;
	size_t			 keylen;

	if (target->s == NULL) {
		cmdq_error(item, "no target session");
		return (CMD_RETURN_ERROR);
	}
	s = target->s;

	kv = args_string(args, 0);
	if (kv == NULL || strlen(kv) == 0) {
		cmdq_error(item, "key=value required");
		return (CMD_RETURN_ERROR);
	}

	/* Parse key=value */
	eq = strchr(kv, '=');
	if (eq == NULL) {
		cmdq_error(item, "invalid format, use: key=value");
		return (CMD_RETURN_ERROR);
	}

	keylen = eq - kv;
	if (keylen == 0) {
		cmdq_error(item, "key cannot be empty");
		return (CMD_RETURN_ERROR);
	}

	key = xmalloc(keylen + 1);
	memcpy(key, kv, keylen);
	key[keylen] = '\0';

	value = xstrdup(eq + 1);

	/* Check if session has agent metadata */
	agent = s->agent_metadata;
	if (agent == NULL) {
		cmdq_error(item, "session '%s' has no agent metadata", s->name);
		free(key);
		free(value);
		return (CMD_RETURN_ERROR);
	}

	/* Check if in a group */
	if (agent->coordination_group == NULL) {
		cmdq_error(item, "session '%s' is not in a coordination group",
		    s->name);
		cmdq_print(item, "Use 'agent-join-group' to join a group first");
		free(key);
		free(value);
		return (CMD_RETURN_ERROR);
	}

	/* Share the context */
	if (session_agent_share_context(agent, key, value) != 0) {
		cmdq_error(item, "failed to share context");
		free(key);
		free(value);
		return (CMD_RETURN_ERROR);
	}

	cmdq_print(item, "Shared with group '%s': %s=%s",
	    agent->coordination_group, key, value);
	cmdq_print(item, "  Context size: %zu bytes",
	    agent->shared_context_len);

	free(key);
	free(value);

	return (CMD_RETURN_NORMAL);
}
