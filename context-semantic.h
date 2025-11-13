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

#ifndef CONTEXT_SEMANTIC_H
#define CONTEXT_SEMANTIC_H

/*
 * Phase 4.4C: Advanced Context Management - Semantic Extraction
 *
 * This module provides intelligent extraction of semantic information
 * from session context to enable smart context restoration and compression.
 */

#define MAX_SEMANTIC_COMMANDS	50
#define MAX_SEMANTIC_FILES	100
#define MAX_SEMANTIC_PATTERNS	20
#define MAX_SEMANTIC_TERM_LEN	256

/* Semantic context item types */
enum semantic_type {
	SEMANTIC_COMMAND,	/* Command executed */
	SEMANTIC_FILE,		/* File accessed */
	SEMANTIC_PATTERN,	/* Pattern identified */
	SEMANTIC_ERROR,		/* Error encountered */
	SEMANTIC_OUTPUT		/* Important output */
};

/* Semantic context item */
struct semantic_item {
	enum semantic_type	 type;
	char			*content;	/* The actual content */
	float			 relevance;	/* 0.0-1.0 relevance score */
	time_t			 timestamp;	/* When captured */
	u_int			 frequency;	/* How many times seen */
	struct semantic_item	*next;
};

/* Semantic context extracted from session */
struct semantic_context {
	struct session		*session;
	struct session_agent	*agent;

	/* Extracted items organized by type */
	struct semantic_item	*commands;	/* Command list */
	struct semantic_item	*files;		/* File list */
	struct semantic_item	*patterns;	/* Pattern list */
	struct semantic_item	*errors;	/* Error list */
	struct semantic_item	*outputs;	/* Output list */

	u_int			 command_count;
	u_int			 file_count;
	u_int			 pattern_count;
	u_int			 error_count;
	u_int			 output_count;

	time_t			 extracted_at;	/* When extraction occurred */
	float			 overall_quality;	/* 0.0-1.0 quality score */
};

/* Initialize semantic extraction system */
void		 context_semantic_init(void);

/* Create semantic context from session */
struct semantic_context	*context_semantic_extract(struct session *,
		     struct session_agent *);

/* Free semantic context */
void		 context_semantic_free(struct semantic_context *);

/* Add semantic item */
int		 context_semantic_add_item(struct semantic_context *,
		     enum semantic_type, const char *, float);

/* Extract commands from window history */
int		 context_semantic_extract_commands(struct semantic_context *);

/* Extract file references from pane content */
int		 context_semantic_extract_files(struct semantic_context *);

/* Identify patterns in session activity */
int		 context_semantic_identify_patterns(struct semantic_context *);

/* Extract errors from pane output */
int		 context_semantic_extract_errors(struct semantic_context *);

/* Extract important output */
int		 context_semantic_extract_outputs(struct semantic_context *);

/* Calculate relevance score for item */
float		 context_semantic_calculate_relevance(struct semantic_item *,
		     struct semantic_context *);

/* Update relevance scores based on recency and frequency */
void		 context_semantic_update_scores(struct semantic_context *);

/* Get top N most relevant items of type */
struct semantic_item	*context_semantic_get_top(struct semantic_context *,
			     enum semantic_type, u_int);

/* Filter items by minimum relevance */
void		 context_semantic_filter_by_relevance(struct semantic_context *,
		     float);

/* Export semantic context to JSON */
char		*context_semantic_export_json(struct semantic_context *);

/* Import semantic context from JSON */
struct semantic_context	*context_semantic_import_json(const char *);

#endif /* !CONTEXT_SEMANTIC_H */
