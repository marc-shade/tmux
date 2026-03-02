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
 * List available session templates.
 */

static enum cmd_retval	cmd_list_templates_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_list_templates_entry = {
	.name = "list-templates",
	.alias = "lst",

	.args = { "", 0, 0, NULL },
	.usage = "",

	.flags = 0,
	.exec = cmd_list_templates_exec
};

static enum cmd_retval
cmd_list_templates_exec(struct cmd *self, struct cmdq_item *item)
{
	char	**names;
	int	  count, i;
	struct session_template	*tmpl;

	(void)self;

	/* Get all template names */
	names = session_template_list_all(&count);
	if (names == NULL || count == 0) {
		cmdq_print(item, "No templates available");
		return (CMD_RETURN_NORMAL);
	}

	cmdq_print(item, "Available Templates:");
	cmdq_print(item, "");

	/* Display each template with details */
	for (i = 0; i < count; i++) {
		tmpl = session_template_load_builtin(names[i]);
		if (tmpl != NULL) {
			cmdq_print(item, "  %s", tmpl->name);
			cmdq_print(item, "    Description: %s", tmpl->description);
			cmdq_print(item, "    Agent Type: %s", tmpl->agent_type);
			cmdq_print(item, "    Windows: %d", tmpl->window_count);

			if (tmpl->mcp_server_count > 0) {
				int j;
				cmdq_print(item, "    MCP Servers: %d", tmpl->mcp_server_count);
				for (j = 0; j < tmpl->mcp_server_count; j++)
					cmdq_print(item, "      - %s", tmpl->mcp_servers[j]);
			}

			if (tmpl->var_count > 0) {
				int j;
				cmdq_print(item, "    Variables:");
				for (j = 0; j < tmpl->var_count; j++) {
					if (tmpl->var_defaults[j][0] != '\0')
						cmdq_print(item, "      {{%s}} (default: %s)",
						    tmpl->var_names[j], tmpl->var_defaults[j]);
					else
						cmdq_print(item, "      {{%s}}", tmpl->var_names[j]);
				}
			}

			cmdq_print(item, "");
			session_template_destroy(tmpl);
		}

		free(names[i]);
	}

	free(names);
	return (CMD_RETURN_NORMAL);
}
