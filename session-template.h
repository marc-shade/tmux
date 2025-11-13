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

#ifndef SESSION_TEMPLATE_H
#define SESSION_TEMPLATE_H

#include <sys/types.h>

/* Maximum template constraints */
#define MAX_TEMPLATE_NAME	64
#define MAX_TEMPLATE_DESC	256
#define MAX_TEMPLATE_WINDOWS	16
#define MAX_WINDOW_NAME		32
#define MAX_WINDOW_COMMAND	256
#define MAX_MCP_SERVERS		8
#define MAX_TEMPLATE_VARS	16

/* Template window definition */
struct template_window {
	char		name[MAX_WINDOW_NAME];
	char		command[MAX_WINDOW_COMMAND];
	int		split_horizontal;	/* Split window horizontally */
	int		split_vertical;		/* Split window vertically */
};

/* Session template structure */
struct session_template {
	char		name[MAX_TEMPLATE_NAME];
	char		description[MAX_TEMPLATE_DESC];
	char		agent_type[MAX_TEMPLATE_NAME];
	char		goal_template[MAX_TEMPLATE_DESC];

	/* Windows configuration */
	struct template_window	*windows;
	int			window_count;
	int			max_windows;

	/* Agent coordination (Phase 4.3) */
	char		coordination_group[MAX_TEMPLATE_NAME];

	/* MCP servers to enable */
	char		**mcp_servers;
	int		mcp_server_count;
	int		max_mcp_servers;

	/* Template variables for substitution */
	char		**var_names;
	char		**var_defaults;
	int		var_count;
	int		max_vars;
};

/* Template instantiation parameters */
struct template_params {
	char		*session_name;
	char		*goal;
	char		*coordination_group;

	/* Variable substitutions */
	char		**var_values;
	int		var_count;
};

/* Template management functions */
int				session_template_init(void);
void				session_template_free(void);

/* Template loading */
struct session_template		*session_template_load(const char *);
struct session_template		*session_template_load_builtin(const char *);
void				session_template_destroy(struct session_template *);

/* Template instantiation */
int				session_template_create_session(
				    struct session_template *,
				    struct template_params *);

/* Template listing */
char				**session_template_list_builtin(int *);
char				**session_template_list_user(int *);
char				**session_template_list_all(int *);

/* Template validation */
int				session_template_validate(struct session_template *);

/* Template string substitution */
char				*session_template_substitute(const char *,
				    struct template_params *);

/* Built-in templates */
extern struct session_template	*builtin_templates[];
extern int			builtin_template_count;

#endif /* SESSION_TEMPLATE_H */
