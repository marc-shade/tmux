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
#include "session-template.h"

/*
 * Create a new session from a template.
 */

static enum cmd_retval	cmd_new_from_template_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_new_from_template_entry = {
	.name = "new-from-template",
	.alias = "newt",

	.args = { "g:s:t:", 0, 0, NULL },
	.usage = "[-g goal] [-s session-name] -t template-name",

	.flags = 0,
	.exec = cmd_new_from_template_exec
};

static enum cmd_retval
cmd_new_from_template_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct session_template	*tmpl;
	struct template_params	 params;
	const char		*template_name, *session_name, *goal, *group;
	int			 ret;

	/* Get template name (required) */
	template_name = args_get(args, 't');
	if (template_name == NULL) {
		cmdq_error(item, "template name required (-t)");
		return (CMD_RETURN_ERROR);
	}

	/* Load template */
	tmpl = session_template_load_builtin(template_name);
	if (tmpl == NULL) {
		cmdq_error(item, "template '%s' not found", template_name);
		return (CMD_RETURN_ERROR);
	}

	/* Get session name (required) */
	session_name = args_get(args, 's');
	if (session_name == NULL) {
		cmdq_error(item, "session name required (-s)");
		session_template_destroy(tmpl);
		return (CMD_RETURN_ERROR);
	}

	/* Check if session already exists */
	if (session_find(session_name) != NULL) {
		cmdq_error(item, "duplicate session: %s", session_name);
		session_template_destroy(tmpl);
		return (CMD_RETURN_ERROR);
	}

	/* Get optional goal */
	goal = args_get(args, 'g');

	/* Get optional coordination group */
	group = args_get(args, 'G');

	/* Set up template parameters */
	memset(&params, 0, sizeof(params));
	params.session_name = xstrdup(session_name);
	params.goal = goal != NULL ? xstrdup(goal) : NULL;
	params.coordination_group = group != NULL ? xstrdup(group) : NULL;
	params.var_count = 0;
	params.var_values = NULL;

	/* Create session from template */
	ret = session_template_create_session(tmpl, &params);

	/* Cleanup */
	free(params.session_name);
	if (params.goal != NULL)
		free(params.goal);
	if (params.coordination_group != NULL)
		free(params.coordination_group);
	session_template_destroy(tmpl);

	if (ret != 0) {
		cmdq_error(item, "failed to create session from template");
		return (CMD_RETURN_ERROR);
	}

	cmdq_print(item, "Session '%s' created from template '%s'",
	    session_name, template_name);

	return (CMD_RETURN_NORMAL);
}
