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
#include "session-template.h"
#include "session-agent.h"

/* Template system state */
static int	template_initialized = 0;

/* Forward declarations */
static void	init_builtin_templates(void);

/*
 * Initialize template system
 */
int
session_template_init(void)
{
	if (template_initialized)
		return (0);

	init_builtin_templates();
	template_initialized = 1;
	return (0);
}

/*
 * Free template system resources
 */
void
session_template_free(void)
{
	if (!template_initialized)
		return;

	template_initialized = 0;
}

/*
 * Create a new empty template
 */
static struct session_template *
session_template_new(void)
{
	struct session_template	*tmpl;

	tmpl = xcalloc(1, sizeof(*tmpl));

	tmpl->window_count = 0;
	tmpl->max_windows = MAX_TEMPLATE_WINDOWS;
	tmpl->windows = xcalloc(tmpl->max_windows, sizeof(struct template_window));

	tmpl->mcp_server_count = 0;
	tmpl->max_mcp_servers = MAX_MCP_SERVERS;
	tmpl->mcp_servers = xcalloc(tmpl->max_mcp_servers, sizeof(char *));

	tmpl->var_count = 0;
	tmpl->max_vars = MAX_TEMPLATE_VARS;
	tmpl->var_names = xcalloc(tmpl->max_vars, sizeof(char *));
	tmpl->var_defaults = xcalloc(tmpl->max_vars, sizeof(char *));

	return (tmpl);
}

/*
 * Destroy a template and free resources
 */
void
session_template_destroy(struct session_template *tmpl)
{
	int	i;

	if (tmpl == NULL)
		return;

	free(tmpl->windows);

	for (i = 0; i < tmpl->mcp_server_count; i++)
		free(tmpl->mcp_servers[i]);
	free(tmpl->mcp_servers);

	for (i = 0; i < tmpl->var_count; i++) {
		free(tmpl->var_names[i]);
		free(tmpl->var_defaults[i]);
	}
	free(tmpl->var_names);
	free(tmpl->var_defaults);

	free(tmpl);
}

/*
 * Add a window to template
 */
static int
session_template_add_window(struct session_template *tmpl, const char *name,
    const char *command)
{
	struct template_window	*window;

	if (tmpl->window_count >= tmpl->max_windows)
		return (-1);

	window = &tmpl->windows[tmpl->window_count++];
	strlcpy(window->name, name, sizeof(window->name));
	strlcpy(window->command, command != NULL ? command : "bash",
	    sizeof(window->command));
	window->split_horizontal = 0;
	window->split_vertical = 0;

	return (0);
}

/*
 * Add an MCP server to template
 */
static int
session_template_add_mcp(struct session_template *tmpl, const char *server)
{
	if (tmpl->mcp_server_count >= tmpl->max_mcp_servers)
		return (-1);

	tmpl->mcp_servers[tmpl->mcp_server_count++] = xstrdup(server);
	return (0);
}

/*
 * Add a template variable
 */
static int
session_template_add_var(struct session_template *tmpl, const char *name,
    const char *default_value)
{
	if (tmpl->var_count >= tmpl->max_vars)
		return (-1);

	tmpl->var_names[tmpl->var_count] = xstrdup(name);
	tmpl->var_defaults[tmpl->var_count] = xstrdup(default_value != NULL ?
	    default_value : "");
	tmpl->var_count++;

	return (0);
}

/*
 * Substitute template variables in a string
 * Format: {{VARNAME}} replaced with value from params
 */
char *
session_template_substitute(const char *template, struct template_params *params)
{
	char		*result, *start, *end, *varname;
	const char	*p, *value;
	size_t		 result_len, template_len;
	int		 i, found;

	if (template == NULL)
		return (NULL);

	/* Allocate buffer (worst case: no substitutions) */
	template_len = strlen(template);
	result_len = template_len * 2 + 1;
	result = xmalloc(result_len);
	result[0] = '\0';

	p = template;
	while (*p != '\0') {
		/* Look for {{VARNAME}} pattern */
		if (p[0] == '{' && p[1] == '{') {
			start = (char *)p + 2;
			end = strstr(start, "}}");

			if (end != NULL) {
				/* Extract variable name */
				varname = xmalloc(end - start + 1);
				memcpy(varname, start, end - start);
				varname[end - start] = '\0';

				/* Find value in params */
				value = NULL;
				found = 0;
				if (params != NULL) {
					for (i = 0; i < params->var_count; i++) {
						if (strcmp(varname, "GOAL") == 0 &&
						    params->goal != NULL) {
							value = params->goal;
							found = 1;
							break;
						}
						if (strcmp(varname, "SESSION") == 0 &&
						    params->session_name != NULL) {
							value = params->session_name;
							found = 1;
							break;
						}
						if (strcmp(varname, "GROUP") == 0 &&
						    params->coordination_group != NULL) {
							value = params->coordination_group;
							found = 1;
							break;
						}
						if (params->var_values != NULL &&
						    params->var_values[i] != NULL) {
							value = params->var_values[i];
							found = 1;
							break;
						}
					}
				}

				/* Append substitution or empty string */
				if (found && value != NULL)
					strlcat(result, value, result_len);

				free(varname);
				p = end + 2;  /* Skip past }} */
				continue;
			}
		}

		/* Regular character - append to result */
		strlcat(result, (char[]){*p, '\0'}, result_len);
		p++;
	}

	return (result);
}

/*
 * Validate template structure
 */
int
session_template_validate(struct session_template *tmpl)
{
	if (tmpl == NULL)
		return (-1);

	if (tmpl->name[0] == '\0')
		return (-1);

	if (tmpl->agent_type[0] == '\0')
		return (-1);

	if (tmpl->window_count == 0)
		return (-1);

	return (0);
}

/*
 * Create session from template
 */
int
session_template_create_session(struct session_template *tmpl,
    struct template_params *params)
{
	struct session		*s;
	struct winlink		*wl;
	struct template_window	*window;
	char			*goal, *session_name;
	int			 i;

	if (session_template_validate(tmpl) != 0)
		return (-1);

	if (params == NULL || params->session_name == NULL)
		return (-1);

	/* Substitute variables in goal template */
	goal = session_template_substitute(tmpl->goal_template, params);
	session_name = params->session_name;

	/* Check if session already exists */
	if (session_find(session_name) != NULL) {
		free(goal);
		return (-1);
	}

	/* Create base session - use current working directory */
	s = session_create(session_name, session_name, getcwd(NULL, 0),
	    environ_create(), options_create(global_s_options), NULL);
	if (s == NULL) {
		free(goal);
		return (-1);
	}

	/* Create agent metadata */
	s->agent_metadata = session_agent_create(tmpl->agent_type, goal,
	    session_name);

	free(goal);

	/* Join coordination group if specified */
	if (params->coordination_group != NULL && params->coordination_group[0] != '\0') {
		session_agent_join_group(s->agent_metadata,
		    params->coordination_group);
	} else if (tmpl->coordination_group[0] != '\0') {
		char	*group;
		group = session_template_substitute(tmpl->coordination_group, params);
		if (group != NULL) {
			session_agent_join_group(s->agent_metadata, group);
			free(group);
		}
	}

	/* Create windows from template */
	for (i = 0; i < tmpl->window_count; i++) {
		window = &tmpl->windows[i];

		if (i == 0) {
			/* First window - already exists, just rename */
			wl = s->curw;
			if (wl != NULL && wl->window != NULL) {
				free(wl->window->name);
				wl->window->name = xstrdup(window->name);
			}
		}
		/* Additional windows - TODO: Implement with spawn_window in future */
	}

	/* TODO: Enable MCP servers (requires MCP client integration) */

	return (0);
}

/*
 * Load template from file
 * Future: Parse JSON template files
 */
struct session_template *
session_template_load(const char *path)
{
	/* TODO: Implement JSON parsing */
	(void)path;
	return (NULL);
}

/*
 * Load built-in template by name
 */
struct session_template *
session_template_load_builtin(const char *name)
{
	struct session_template	*tmpl;
	int			 i;

	if (!template_initialized)
		session_template_init();

	/* Search built-in templates */
	for (i = 0; i < builtin_template_count; i++) {
		if (strcmp(builtin_templates[i]->name, name) == 0) {
			/* Return copy of built-in template */
			tmpl = session_template_new();
			memcpy(tmpl, builtin_templates[i],
			    sizeof(struct session_template));

			/* Deep copy dynamic arrays */
			tmpl->windows = xcalloc(tmpl->max_windows,
			    sizeof(struct template_window));
			memcpy(tmpl->windows, builtin_templates[i]->windows,
			    tmpl->window_count * sizeof(struct template_window));

			/* Deep copy MCP servers */
			for (i = 0; i < tmpl->mcp_server_count; i++)
				tmpl->mcp_servers[i] = xstrdup(builtin_templates[i]->mcp_servers[i]);

			/* Deep copy variables */
			for (i = 0; i < tmpl->var_count; i++) {
				tmpl->var_names[i] = xstrdup(builtin_templates[i]->var_names[i]);
				tmpl->var_defaults[i] = xstrdup(builtin_templates[i]->var_defaults[i]);
			}

			return (tmpl);
		}
	}

	return (NULL);
}

/*
 * List built-in template names
 */
char **
session_template_list_builtin(int *count)
{
	char	**names;
	int	  i;

	if (!template_initialized)
		session_template_init();

	*count = builtin_template_count;
	names = xcalloc(builtin_template_count, sizeof(char *));

	for (i = 0; i < builtin_template_count; i++)
		names[i] = xstrdup(builtin_templates[i]->name);

	return (names);
}

/*
 * List user-defined template names
 * Future: Scan user template directory
 */
char **
session_template_list_user(int *count)
{
	*count = 0;
	return (NULL);
}

/*
 * List all templates (built-in + user)
 */
char **
session_template_list_all(int *count)
{
	/* For now, just return built-in */
	return (session_template_list_builtin(count));
}

/* Built-in templates are initialized at runtime to avoid const issues */
static struct session_template builtin_research;
static struct session_template builtin_development;
static struct session_template builtin_simple;

/*
 * Initialize built-in templates
 */
static void
init_builtin_templates(void)
{
	static int initialized = 0;

	if (initialized)
		return;

	/* Research template */
	strlcpy(builtin_research.name, "research", sizeof(builtin_research.name));
	strlcpy(builtin_research.description, "Research session with multiple windows",
	    sizeof(builtin_research.description));
	strlcpy(builtin_research.agent_type, "research", sizeof(builtin_research.agent_type));
	strlcpy(builtin_research.goal_template, "{{GOAL}}", sizeof(builtin_research.goal_template));
	builtin_research.window_count = 1;
	builtin_research.max_windows = MAX_TEMPLATE_WINDOWS;
	builtin_research.windows = xcalloc(MAX_TEMPLATE_WINDOWS, sizeof(struct template_window));
	strlcpy(builtin_research.windows[0].name, "main", sizeof(builtin_research.windows[0].name));
	strlcpy(builtin_research.windows[0].command, "bash", sizeof(builtin_research.windows[0].command));
	builtin_research.coordination_group[0] = '\0';
	builtin_research.mcp_server_count = 0;
	builtin_research.max_mcp_servers = MAX_MCP_SERVERS;
	builtin_research.mcp_servers = xcalloc(MAX_MCP_SERVERS, sizeof(char *));
	builtin_research.var_count = 0;
	builtin_research.max_vars = MAX_TEMPLATE_VARS;
	builtin_research.var_names = xcalloc(MAX_TEMPLATE_VARS, sizeof(char *));
	builtin_research.var_defaults = xcalloc(MAX_TEMPLATE_VARS, sizeof(char *));

	/* Development template */
	strlcpy(builtin_development.name, "development", sizeof(builtin_development.name));
	strlcpy(builtin_development.description, "Development session with editor",
	    sizeof(builtin_development.description));
	strlcpy(builtin_development.agent_type, "development", sizeof(builtin_development.agent_type));
	strlcpy(builtin_development.goal_template, "{{GOAL}}", sizeof(builtin_development.goal_template));
	builtin_development.window_count = 1;
	builtin_development.max_windows = MAX_TEMPLATE_WINDOWS;
	builtin_development.windows = xcalloc(MAX_TEMPLATE_WINDOWS, sizeof(struct template_window));
	strlcpy(builtin_development.windows[0].name, "main", sizeof(builtin_development.windows[0].name));
	strlcpy(builtin_development.windows[0].command, "bash", sizeof(builtin_development.windows[0].command));
	strlcpy(builtin_development.coordination_group, "{{GROUP}}",
	    sizeof(builtin_development.coordination_group));
	builtin_development.mcp_server_count = 0;
	builtin_development.max_mcp_servers = MAX_MCP_SERVERS;
	builtin_development.mcp_servers = xcalloc(MAX_MCP_SERVERS, sizeof(char *));
	builtin_development.var_count = 0;
	builtin_development.max_vars = MAX_TEMPLATE_VARS;
	builtin_development.var_names = xcalloc(MAX_TEMPLATE_VARS, sizeof(char *));
	builtin_development.var_defaults = xcalloc(MAX_TEMPLATE_VARS, sizeof(char *));

	/* Simple template */
	strlcpy(builtin_simple.name, "simple", sizeof(builtin_simple.name));
	strlcpy(builtin_simple.description, "Simple single-window session",
	    sizeof(builtin_simple.description));
	strlcpy(builtin_simple.agent_type, "general", sizeof(builtin_simple.agent_type));
	strlcpy(builtin_simple.goal_template, "{{GOAL}}", sizeof(builtin_simple.goal_template));
	builtin_simple.window_count = 1;
	builtin_simple.max_windows = MAX_TEMPLATE_WINDOWS;
	builtin_simple.windows = xcalloc(MAX_TEMPLATE_WINDOWS, sizeof(struct template_window));
	strlcpy(builtin_simple.windows[0].name, "main", sizeof(builtin_simple.windows[0].name));
	strlcpy(builtin_simple.windows[0].command, "bash", sizeof(builtin_simple.windows[0].command));
	builtin_simple.coordination_group[0] = '\0';
	builtin_simple.mcp_server_count = 0;
	builtin_simple.max_mcp_servers = MAX_MCP_SERVERS;
	builtin_simple.mcp_servers = xcalloc(MAX_MCP_SERVERS, sizeof(char *));
	builtin_simple.var_count = 0;
	builtin_simple.max_vars = MAX_TEMPLATE_VARS;
	builtin_simple.var_names = xcalloc(MAX_TEMPLATE_VARS, sizeof(char *));
	builtin_simple.var_defaults = xcalloc(MAX_TEMPLATE_VARS, sizeof(char *));

	initialized = 1;
}

/* Array of built-in templates */
struct session_template *builtin_templates[] = {
	&builtin_research,
	&builtin_development,
	&builtin_simple,
	NULL
};

int builtin_template_count = 3;
