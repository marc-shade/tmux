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

/*
 * Query MCP server and call tool.
 */

static enum cmd_retval	cmd_mcp_query_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_mcp_query_entry = {
	.name = "mcp-query",
	.alias = NULL,

	.args = { "", 2, 3, NULL },
	.usage = "server tool [arguments]",

	.flags = 0,
	.exec = cmd_mcp_query_exec
};

static enum cmd_retval
cmd_mcp_query_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct mcp_response	*resp;
	const char		*server_name, *tool_name, *arguments;
	char			*output;

	/* Get arguments */
	if (args_count(args) < 2) {
		cmdq_error(item, "usage: mcp-query server tool [arguments]");
		return (CMD_RETURN_ERROR);
	}

	server_name = args_string(args, 0);
	tool_name = args_string(args, 1);
	arguments = (args_count(args) > 2) ? args_string(args, 2) : NULL;

	/* Ensure global MCP client exists */
	if (global_mcp_client == NULL) {
		global_mcp_client = mcp_client_create();
		if (global_mcp_client == NULL) {
			cmdq_error(item, "failed to create MCP client");
			return (CMD_RETURN_ERROR);
		}
		mcp_client_init(global_mcp_client);
	}

	/* Call the tool */
	resp = mcp_call_tool(global_mcp_client, server_name, tool_name,
	    arguments);

	if (resp == NULL) {
		cmdq_error(item, "MCP call failed: connection error");
		return (CMD_RETURN_ERROR);
	}

	/* Output result or error */
	if (resp->success) {
		if (resp->result != NULL) {
			/* Output result to client */
			output = xstrdup(resp->result);
			cmdq_print(item, "%s", output);
			free(output);
		}
	} else {
		cmdq_error(item, "MCP error: %s",
		    resp->error_message ? resp->error_message : "unknown");
		mcp_response_free(resp);
		return (CMD_RETURN_ERROR);
	}

	mcp_response_free(resp);
	return (CMD_RETURN_NORMAL);
}
