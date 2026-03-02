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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"
#include "mcp-client.h"
#include "mcp-config.h"

#define MAX_ARGS 64

/*
 * Minimal JSON string extractor.
 * Given a pointer at or before a quoted string, extract and return the value.
 * Advances *pos past the closing quote.  Caller must free the result.
 */
static char *
json_extract_string(const char **pos)
{
	const char	*p = *pos;
	const char	*start;
	size_t		 len;
	char		*result;

	/* Find opening quote */
	while (*p && *p != '"')
		p++;
	if (*p != '"')
		return (NULL);
	p++;
	start = p;

	/* Find closing quote (skip escaped chars) */
	while (*p && *p != '"') {
		if (*p == '\\' && *(p + 1))
			p++;
		p++;
	}
	if (*p != '"')
		return (NULL);

	len = p - start;
	result = xmalloc(len + 1);
	memcpy(result, start, len);
	result[len] = '\0';

	*pos = p + 1;
	return (result);
}

/*
 * Skip whitespace and the given character.
 */
static const char *
json_skip(const char *p, char c)
{
	while (*p && (isspace((unsigned char)*p) || *p == c))
		p++;
	return (p);
}

/*
 * Find the value for a JSON key in an object.
 * Returns pointer to start of value (after the colon).
 */
static const char *
json_find_key(const char *json, const char *key)
{
	char		 search[256];
	const char	*p;

	snprintf(search, sizeof search, "\"%s\"", key);
	p = strstr(json, search);
	if (p == NULL)
		return (NULL);

	p += strlen(search);
	/* Skip whitespace and colon */
	while (*p && (isspace((unsigned char)*p) || *p == ':'))
		p++;
	return (p);
}

/*
 * Find matching closing brace/bracket, accounting for nesting.
 */
static const char *
json_find_close(const char *p, char open, char close)
{
	int	depth = 1;

	p++; /* skip opening char */
	while (*p && depth > 0) {
		if (*p == '"') {
			/* Skip strings (they may contain braces) */
			p++;
			while (*p && *p != '"') {
				if (*p == '\\' && *(p + 1))
					p++;
				p++;
			}
		} else if (*p == open) {
			depth++;
		} else if (*p == close) {
			depth--;
		}
		if (*p)
			p++;
	}
	return (p);
}

/*
 * Parse a single MCP server entry from JSON and add it to the client.
 * server_json points to the opening { of the server config object.
 * Returns 0 on success, -1 on error.
 */
static int
parse_server_entry(struct mcp_client *client, const char *name,
    const char *server_json)
{
	struct mcp_server_config	*config;
	const char			*p, *arr_start, *arr_end;
	char				*args[MAX_ARGS];
	int				 argc = 0;
	int				 i;

	config = xcalloc(1, sizeof *config);
	config->name = xstrdup(name);
	config->transport = MCP_TRANSPORT_STDIO;
	config->auto_start = 1;

	/* Extract "command" */
	p = json_find_key(server_json, "command");
	if (p != NULL && *p == '"')
		config->command = json_extract_string(&p);

	if (config->command == NULL) {
		free(config->name);
		free(config);
		return (-1);
	}

	/* Extract "args" array */
	p = json_find_key(server_json, "args");
	if (p != NULL && *p == '[') {
		arr_start = p + 1;
		arr_end = json_find_close(p, '[', ']');

		p = arr_start;
		while (p < arr_end && argc < MAX_ARGS) {
			/* Find next string in array */
			while (p < arr_end && *p != '"')
				p++;
			if (p >= arr_end || *p != '"')
				break;
			args[argc] = json_extract_string(&p);
			if (args[argc] != NULL)
				argc++;
		}
	}

	/* Build args array for execv: [command, arg1, ..., NULL] */
	config->args = xmalloc((argc + 2) * sizeof(char *));
	config->args[0] = xstrdup(config->command);
	for (i = 0; i < argc; i++)
		config->args[i + 1] = args[i];
	config->args[argc + 1] = NULL;

	if (mcp_add_server(client, config) < 0) {
		/* Clean up on failure */
		for (i = 0; i < argc; i++)
			free(args[i]);
		free(config->args[0]);
		free(config->args);
		free(config->command);
		free(config->name);
		free(config);
		return (-1);
	}

	return (0);
}

/*
 * Load MCP servers from a JSON config file.
 * Returns number of servers loaded, or -1 on error.
 */
int
mcp_load_config_from_file(struct mcp_client *client, const char *config_path)
{
	FILE		*fp;
	char		*data = NULL;
	size_t		 data_len = 0, data_cap = 0;
	char		 buf[4096];
	size_t		 n;
	const char	*servers_obj, *p, *server_start;
	char		*server_name;
	int		 loaded = 0;

	if (client == NULL || config_path == NULL)
		return (-1);

	fp = fopen(config_path, "r");
	if (fp == NULL)
		return (-1);

	/* Read entire file into memory */
	while ((n = fread(buf, 1, sizeof buf, fp)) > 0) {
		if (data_len + n >= data_cap) {
			data_cap = (data_cap == 0) ? 8192 : data_cap * 2;
			data = xrealloc(data, data_cap);
		}
		memcpy(data + data_len, buf, n);
		data_len += n;
	}
	fclose(fp);

	if (data_len == 0) {
		free(data);
		return (0);
	}
	data[data_len] = '\0';

	/* Find "mcpServers" object */
	servers_obj = json_find_key(data, "mcpServers");
	if (servers_obj == NULL || *servers_obj != '{') {
		free(data);
		return (0);
	}

	/* Iterate over server entries in the mcpServers object */
	p = servers_obj + 1; /* skip opening { */
	while (*p) {
		/* Skip whitespace */
		while (*p && isspace((unsigned char)*p))
			p++;

		if (*p == '}')
			break; /* end of mcpServers */

		if (*p == ',') {
			p++;
			continue;
		}

		/* Expect a quoted key (server name) */
		if (*p != '"')
			break;

		server_name = json_extract_string(&p);
		if (server_name == NULL)
			break;

		/* Skip colon */
		p = json_skip(p, ':');

		/* Expect server config object */
		if (*p != '{') {
			free(server_name);
			break;
		}

		server_start = p;
		p = json_find_close(p, '{', '}');

		if (parse_server_entry(client, server_name,
		    server_start) == 0)
			loaded++;

		free(server_name);
	}

	free(data);
	return (loaded);
}

/*
 * Load MCP servers from default ~/.claude.json.
 */
int
mcp_load_config(struct mcp_client *client)
{
	char	path[1024];

	snprintf(path, sizeof path, "%s/.claude.json", find_home());
	return (mcp_load_config_from_file(client, path));
}
