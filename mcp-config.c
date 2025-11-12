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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"
#include "mcp-client.h"
#include "mcp-config.h"

#define MAX_LINE_LEN 4096
#define MAX_ARGS 64

/*
 * Parse a single line from config helper output.
 * Format: "key=value"
 * Returns key and value pointers (both must be freed by caller).
 */
static int
parse_line(const char *line, char **key, char **value)
{
	const char	*eq;
	size_t		 key_len;

	*key = NULL;
	*value = NULL;

	/* Find equals sign */
	eq = strchr(line, '=');
	if (eq == NULL)
		return (-1);

	/* Extract key */
	key_len = eq - line;
	if (key_len == 0)
		return (-1);

	*key = xmalloc(key_len + 1);
	memcpy(*key, line, key_len);
	(*key)[key_len] = '\0';

	/* Extract value */
	*value = xstrdup(eq + 1);

	return (0);
}

/*
 * Load MCP servers from config file using Python helper.
 * Returns number of servers loaded, or -1 on error.
 */
int
mcp_load_config_from_file(struct mcp_client *client, const char *config_path)
{
	FILE			*fp;
	char			 line[MAX_LINE_LEN];
	char			*key, *value;
	char			 cmd[MAX_LINE_LEN];
	int			 servers_loaded = 0;

	/* Current server being parsed */
	struct mcp_server_config	*server = NULL;
	char				*args[MAX_ARGS];
	int				 args_count = 0;

	/* Build command to run helper script */
	if (config_path != NULL) {
		/* Not implemented: custom config path */
		return (-1);
	}

	/* Use default ~/.claude.json via helper script */
	snprintf(cmd, sizeof cmd, "%s/mcp-config-helper.py",
	    TMUX_CONF);  /* Placeholder - use build dir for now */

	/* For testing, use absolute path */
	snprintf(cmd, sizeof cmd, "/tmp/tmux-agentic/mcp-config-helper.py");

	fp = popen(cmd, "r");
	if (fp == NULL)
		return (-1);

	/* Parse helper output */
	while (fgets(line, sizeof line, fp) != NULL) {
		/* Remove trailing newline */
		line[strcspn(line, "\n")] = '\0';

		/* Skip empty lines */
		if (line[0] == '\0')
			continue;

		/* Check for section markers */
		if (strcmp(line, "SERVER_START") == 0) {
			/* Start new server config */
			if (server != NULL) {
				/* This shouldn't happen - missing SERVER_END */
				free(server->name);
				free(server->command);
				free(server);
			}

			server = xmalloc(sizeof *server);
			memset(server, 0, sizeof *server);
			server->transport = MCP_TRANSPORT_STDIO;
			server->socket_path = NULL;
			server->auto_start = 1;
			args_count = 0;

			continue;
		}

		if (strcmp(line, "SERVER_END") == 0) {
			/* Finish server config and add to client */
			if (server == NULL)
				continue;

			/*
			 * Build args array for execv().
			 * args[0] must be the command itself.
			 */
			if (args_count > 0 && server->command != NULL) {
				int	i;

				server->args = xmalloc((args_count + 2) *
				    sizeof(char *));
				server->args[0] = xstrdup(server->command);
				for (i = 0; i < args_count; i++)
					server->args[i + 1] = args[i];
				server->args[args_count + 1] = NULL;
			} else if (server->command != NULL) {
				/* No args, just command */
				server->args = xmalloc(2 * sizeof(char *));
				server->args[0] = xstrdup(server->command);
				server->args[1] = NULL;
			} else {
				server->args = NULL;
			}

			/* Add server to client */
			if (mcp_add_server(client, server) == 0)
				servers_loaded++;

			server = NULL;
			args_count = 0;
			continue;
		}

		/* Parse key=value line */
		if (server == NULL)
			continue;

		if (parse_line(line, &key, &value) < 0)
			continue;

		/* Process based on key */
		if (strcmp(key, "name") == 0) {
			server->name = xstrdup(value);
		} else if (strcmp(key, "command") == 0) {
			server->command = xstrdup(value);
		} else if (strcmp(key, "arg") == 0) {
			/* Add to args array */
			if (args_count < MAX_ARGS - 1) {
				args[args_count] = xstrdup(value);
				args_count++;
			}
		}

		free(key);
		free(value);
	}

	pclose(fp);

	return (servers_loaded);
}

/*
 * Load MCP servers from default ~/.claude.json.
 */
int
mcp_load_config(struct mcp_client *client)
{
	return mcp_load_config_from_file(client, NULL);
}
