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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"
#include "context-semantic.h"
#include "session-agent.h"

/*
 * Phase 4.4C: Semantic Context Extraction
 *
 * Intelligent extraction of meaningful information from session context:
 * - Commands executed
 * - Files accessed
 * - Patterns identified
 * - Errors encountered
 * - Important output
 *
 * Uses relevance scoring to prioritize information.
 */

/* Initialize semantic extraction system */
void
context_semantic_init(void)
{
	/* Currently no global state to initialize */
	log_debug("Semantic extraction system initialized");
}

/* Create semantic context from session */
struct semantic_context *
context_semantic_extract(struct session *s, struct session_agent *agent)
{
	struct semantic_context	*ctx;

	if (s == NULL)
		return (NULL);

	ctx = xcalloc(1, sizeof *ctx);
	ctx->session = s;
	ctx->agent = agent;
	ctx->extracted_at = time(NULL);

	/* Extract semantic information */
	context_semantic_extract_commands(ctx);
	context_semantic_extract_files(ctx);
	context_semantic_identify_patterns(ctx);
	context_semantic_extract_errors(ctx);
	context_semantic_extract_outputs(ctx);

	/* Update relevance scores */
	context_semantic_update_scores(ctx);

	/* Calculate overall quality */
	ctx->overall_quality = (ctx->command_count > 0 ? 0.3 : 0.0) +
	    (ctx->file_count > 0 ? 0.3 : 0.0) +
	    (ctx->pattern_count > 0 ? 0.2 : 0.0) +
	    (ctx->error_count > 0 ? 0.1 : 0.0) +
	    (ctx->output_count > 0 ? 0.1 : 0.0);

	log_debug("Semantic extraction complete: quality=%.2f commands=%u files=%u",
	    ctx->overall_quality, ctx->command_count, ctx->file_count);

	return (ctx);
}

/* Free semantic context */
void
context_semantic_free(struct semantic_context *ctx)
{
	struct semantic_item	*item, *next;

	if (ctx == NULL)
		return;

	/* Free all item lists */
	for (item = ctx->commands; item != NULL; item = next) {
		next = item->next;
		free(item->content);
		free(item);
	}

	for (item = ctx->files; item != NULL; item = next) {
		next = item->next;
		free(item->content);
		free(item);
	}

	for (item = ctx->patterns; item != NULL; item = next) {
		next = item->next;
		free(item->content);
		free(item);
	}

	for (item = ctx->errors; item != NULL; item = next) {
		next = item->next;
		free(item->content);
		free(item);
	}

	for (item = ctx->outputs; item != NULL; item = next) {
		next = item->next;
		free(item->content);
		free(item);
	}

	free(ctx);
}

/* Add semantic item */
int
context_semantic_add_item(struct semantic_context *ctx, enum semantic_type type,
    const char *content, float relevance)
{
	struct semantic_item	*item, **list, *existing;
	u_int			*count;

	if (ctx == NULL || content == NULL)
		return (-1);

	/* Select appropriate list and counter */
	switch (type) {
	case SEMANTIC_COMMAND:
		list = &ctx->commands;
		count = &ctx->command_count;
		break;
	case SEMANTIC_FILE:
		list = &ctx->files;
		count = &ctx->file_count;
		break;
	case SEMANTIC_PATTERN:
		list = &ctx->patterns;
		count = &ctx->pattern_count;
		break;
	case SEMANTIC_ERROR:
		list = &ctx->errors;
		count = &ctx->error_count;
		break;
	case SEMANTIC_OUTPUT:
		list = &ctx->outputs;
		count = &ctx->output_count;
		break;
	default:
		return (-1);
	}

	/* Check for existing item (deduplicate) */
	for (existing = *list; existing != NULL; existing = existing->next) {
		if (strcmp(existing->content, content) == 0) {
			/* Update frequency and relevance */
			existing->frequency++;
			existing->relevance = (existing->relevance + relevance) / 2.0;
			existing->timestamp = time(NULL);
			return (0);
		}
	}

	/* Create new item */
	item = xcalloc(1, sizeof *item);
	item->type = type;
	item->content = xstrdup(content);
	item->relevance = relevance;
	item->timestamp = time(NULL);
	item->frequency = 1;

	/* Add to list */
	item->next = *list;
	*list = item;
	(*count)++;

	return (0);
}

/* Extract commands from window history */
int
context_semantic_extract_commands(struct semantic_context *ctx)
{
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	char			 command[256];
	float			 relevance;

	if (ctx == NULL || ctx->session == NULL)
		return (-1);

	/* Iterate through session windows */
	RB_FOREACH(wl, winlinks, &ctx->session->windows) {
		w = wl->window;
		if (w == NULL)
			continue;

		/* Check each pane */
		TAILQ_FOREACH(wp, &w->panes, entry) {
			/* For MVP, we'll use window name as a proxy for command activity
			 * since pane command history requires screen content access */
			if (w->name != NULL && w->name[0] != '\0') {
				snprintf(command, sizeof command, "window: %s", w->name);

				/* Calculate relevance based on window activity */
				relevance = 0.5;
				if (w == ctx->session->curw->window)
					relevance += 0.3;	/* Current window */
				if (wp == w->active)
					relevance += 0.2;	/* Active pane */

				context_semantic_add_item(ctx, SEMANTIC_COMMAND,
				    command, relevance);
			}
			/* Only process active pane for each window to avoid duplicates */
			break;
		}
	}

	return (0);
}

/* Extract file references from pane content */
int
context_semantic_extract_files(struct semantic_context *ctx)
{
	/* TODO: Parse pane content for file paths
	 * This would require access to screen content which is complex
	 * For MVP, we mark this as TODO
	 */
	(void)ctx;	/* Suppress unused warning */

	return (0);
}

/* Identify patterns in session activity */
int
context_semantic_identify_patterns(struct semantic_context *ctx)
{
	struct semantic_item	*cmd;
	u_int			 vim_count, git_count, make_count;

	if (ctx == NULL)
		return (-1);

	/* Count command patterns */
	vim_count = git_count = make_count = 0;

	for (cmd = ctx->commands; cmd != NULL; cmd = cmd->next) {
		if (strstr(cmd->content, "vim") != NULL)
			vim_count++;
		if (strstr(cmd->content, "git") != NULL)
			git_count++;
		if (strstr(cmd->content, "make") != NULL)
			make_count++;
	}

	/* Add patterns with high frequency */
	if (vim_count >= 3)
		context_semantic_add_item(ctx, SEMANTIC_PATTERN,
		    "Frequent vim usage", 0.8);

	if (git_count >= 3)
		context_semantic_add_item(ctx, SEMANTIC_PATTERN,
		    "Active git workflow", 0.8);

	if (make_count >= 2)
		context_semantic_add_item(ctx, SEMANTIC_PATTERN,
		    "Build/compile activity", 0.7);

	return (0);
}

/* Extract errors from pane output */
int
context_semantic_extract_errors(struct semantic_context *ctx)
{
	/* TODO: Parse pane output for error messages
	 * This would require screen content access
	 * For MVP, marked as TODO
	 */
	(void)ctx;	/* Suppress unused warning */

	return (0);
}

/* Extract important output */
int
context_semantic_extract_outputs(struct semantic_context *ctx)
{
	/* TODO: Parse pane output for important messages
	 * This would require screen content access
	 * For MVP, marked as TODO
	 */
	(void)ctx;	/* Suppress unused warning */

	return (0);
}

/* Calculate relevance score for item */
float
context_semantic_calculate_relevance(struct semantic_item *item,
    struct semantic_context *ctx)
{
	time_t		 now;
	float		 recency_score, frequency_score, base_score;
	long		 age;

	if (item == NULL || ctx == NULL)
		return (0.0);

	now = time(NULL);
	age = now - item->timestamp;

	/* Base score from initial assignment */
	base_score = item->relevance;

	/* Recency score: exponential decay over 1 hour */
	recency_score = exp(-age / 3600.0);

	/* Frequency score: logarithmic growth */
	frequency_score = log(1.0 + item->frequency) / log(10.0);

	/* Combined score with weights */
	return (base_score * 0.4 + recency_score * 0.3 + frequency_score * 0.3);
}

/* Update relevance scores based on recency and frequency */
void
context_semantic_update_scores(struct semantic_context *ctx)
{
	struct semantic_item	*item;

	if (ctx == NULL)
		return;

	/* Update all item lists */
	for (item = ctx->commands; item != NULL; item = item->next)
		item->relevance = context_semantic_calculate_relevance(item, ctx);

	for (item = ctx->files; item != NULL; item = item->next)
		item->relevance = context_semantic_calculate_relevance(item, ctx);

	for (item = ctx->patterns; item != NULL; item = item->next)
		item->relevance = context_semantic_calculate_relevance(item, ctx);

	for (item = ctx->errors; item != NULL; item = item->next)
		item->relevance = context_semantic_calculate_relevance(item, ctx);

	for (item = ctx->outputs; item != NULL; item = item->next)
		item->relevance = context_semantic_calculate_relevance(item, ctx);
}

/* Get top N most relevant items of type */
struct semantic_item *
context_semantic_get_top(struct semantic_context *ctx, enum semantic_type type,
    u_int n)
{
	struct semantic_item	*list, *item, *top, *result, *last;
	u_int			 i;

	if (ctx == NULL || n == 0)
		return (NULL);

	/* Select list by type */
	switch (type) {
	case SEMANTIC_COMMAND:
		list = ctx->commands;
		break;
	case SEMANTIC_FILE:
		list = ctx->files;
		break;
	case SEMANTIC_PATTERN:
		list = ctx->patterns;
		break;
	case SEMANTIC_ERROR:
		list = ctx->errors;
		break;
	case SEMANTIC_OUTPUT:
		list = ctx->outputs;
		break;
	default:
		return (NULL);
	}

	/* Build result list of top N items */
	result = NULL;
	last = NULL;

	for (i = 0; i < n; i++) {
		/* Find highest relevance not yet in result */
		top = NULL;
		for (item = list; item != NULL; item = item->next) {
			/* Skip if already in result */
			int found = 0;
			struct semantic_item *r;
			for (r = result; r != NULL; r = r->next) {
				if (r == item) {
					found = 1;
					break;
				}
			}
			if (found)
				continue;

			if (top == NULL || item->relevance > top->relevance)
				top = item;
		}

		if (top == NULL)
			break;

		/* Add to result (just reference, don't copy) */
		if (result == NULL) {
			result = top;
			last = top;
		} else {
			last->next = top;
			last = top;
		}
	}

	if (last != NULL)
		last->next = NULL;

	return (result);
}

/* Filter items by minimum relevance */
void
context_semantic_filter_by_relevance(struct semantic_context *ctx,
    float min_relevance)
{
	struct semantic_item	*item, *prev, *next, **list;
	u_int			*count;
	int			 i;

	if (ctx == NULL)
		return;

	/* Filter each list */
	for (i = 0; i < 5; i++) {
		switch (i) {
		case 0:
			list = &ctx->commands;
			count = &ctx->command_count;
			break;
		case 1:
			list = &ctx->files;
			count = &ctx->file_count;
			break;
		case 2:
			list = &ctx->patterns;
			count = &ctx->pattern_count;
			break;
		case 3:
			list = &ctx->errors;
			count = &ctx->error_count;
			break;
		case 4:
			list = &ctx->outputs;
			count = &ctx->output_count;
			break;
		default:
			continue;
		}

		prev = NULL;
		for (item = *list; item != NULL; item = next) {
			next = item->next;

			if (item->relevance < min_relevance) {
				/* Remove item */
				if (prev == NULL)
					*list = next;
				else
					prev->next = next;

				free(item->content);
				free(item);
				(*count)--;
			} else {
				prev = item;
			}
		}
	}
}

/* Export semantic context to JSON */
char *
context_semantic_export_json(struct semantic_context *ctx)
{
	/* TODO: Implement JSON export for context persistence */
	(void)ctx;	/* Suppress unused warning */

	return (xstrdup("{}")); /* Placeholder */
}

/* Import semantic context from JSON */
struct semantic_context *
context_semantic_import_json(const char *json)
{
	/* TODO: Implement JSON import for context restoration */
	(void)json;	/* Suppress unused warning */

	return (NULL);	/* Placeholder */
}
