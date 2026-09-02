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
 * Send a notification message to coordination group peers or a specific
 * session. Messages appear as display-message in target sessions.
 *
 * Usage:
 *   agent-notify -g <group> -m "message"     # Notify all peers in group
 *   agent-notify -t <session> -m "message"   # Notify specific session
 */

static enum cmd_retval	cmd_agent_notify_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_agent_notify_entry = {
	.name = "agent-notify",
	.alias = "anotify",

	.args = { "g:m:t:", 0, 0, NULL },
	.usage = "[-g group] [-m message] [-t target-session]",

	.target = { 't', CMD_FIND_SESSION, CMD_FIND_QUIET },

	.flags = CMD_AFTERHOOK|CMD_EXTENSION,
	.exec = cmd_agent_notify_exec
};

static enum cmd_retval
cmd_agent_notify_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s, *ts;
	struct session_agent	*sa;
	struct client		*c;
	const char		*group, *message;
	char			*display_msg;
	int			 delivered = 0;

	message = args_get(args, 'm');
	if (message == NULL) {
		cmdq_error(item, "missing -m message");
		return (CMD_RETURN_ERROR);
	}

	group = args_get(args, 'g');

	if (group != NULL) {
		/*
		 * Send to all sessions in the coordination group,
		 * except the sender.
		 */
		s = target->s;

		RB_FOREACH(ts, sessions, &sessions) {
			sa = ts->agent_metadata;
			if (sa == NULL || sa->coordination_group == NULL)
				continue;
			if (strcmp(sa->coordination_group, group) != 0)
				continue;
			/* Don't notify ourselves. */
			if (s != NULL && ts == s)
				continue;

			/* Find attached client and show message. */
			TAILQ_FOREACH(c, &clients, entry) {
				if (c->session == ts) {
					xasprintf(&display_msg,
					    "[%s] %s",
					    s != NULL ? s->name : "agent",
					    message);
					status_message_set(c, -1, 0, 0, 0,
					    "%s", display_msg);
					free(display_msg);
					delivered++;
					break;
				}
			}
		}

		if (delivered == 0) {
			cmdq_error(item,
			    "no peers in group '%s' have attached clients",
			    group);
			return (CMD_RETURN_ERROR);
		}

		cmdq_print(item, "Notified %d peer(s) in group '%s'",
		    delivered, group);
	} else {
		/* Send to specific target session. */
		ts = target->s;
		if (ts == NULL) {
			cmdq_error(item, "target session not found");
			return (CMD_RETURN_ERROR);
		}

		TAILQ_FOREACH(c, &clients, entry) {
			if (c->session == ts) {
				xasprintf(&display_msg, "[notify] %s",
				    message);
				status_message_set(c, -1, 0, 0, 0,
				    "%s", display_msg);
				free(display_msg);
				delivered = 1;
				break;
			}
		}

		if (!delivered) {
			cmdq_error(item,
			    "session '%s' has no attached client",
			    ts->name);
			return (CMD_RETURN_ERROR);
		}

		cmdq_print(item, "Notified session '%s'", ts->name);
	}

	return (CMD_RETURN_NORMAL);
}
