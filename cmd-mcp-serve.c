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
#include <unistd.h>

#include "tmux.h"
#include "mcp-server.h"

/*
 * Start tmux as an MCP server, reading JSON-RPC from stdin and writing
 * responses to stdout. This allows MCP clients (like Claude Code) to
 * query tmux state directly.
 *
 * Usage: tmux mcp-serve
 *
 * Configure in ~/.claude.json:
 *   "tmux": { "command": "tmux", "args": ["mcp-serve"] }
 */

static enum cmd_retval	cmd_mcp_serve_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_mcp_serve_entry = {
	.name = "mcp-serve",
	.alias = NULL,

	.args = { "", 0, 0, NULL },
	.usage = "",

	.flags = CMD_STARTSERVER|CMD_EXTENSION,
	.exec = cmd_mcp_serve_exec
};

static enum cmd_retval
cmd_mcp_serve_exec(struct cmd *self __attribute__((unused)),
    struct cmdq_item *item)
{
	struct mcp_server	*srv;

	srv = mcp_server_create(STDIN_FILENO, STDOUT_FILENO);
	if (srv == NULL) {
		cmdq_error(item, "failed to create MCP server");
		return (CMD_RETURN_ERROR);
	}

	mcp_server_run(srv);
	mcp_server_destroy(srv);

	return (CMD_RETURN_NORMAL);
}
