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
#include <time.h>

#include "tmux.h"

/*
 * Agent metadata management for agentic system integration.
 * This module provides tracking and management of AI agent metadata
 * associated with tmux panes.
 */

/* Initialize agent metadata for a pane */
void
agent_metadata_init(struct window_pane *wp)
{
	if (wp->agent_meta != NULL)
		return;

	wp->agent_meta = xcalloc(1, sizeof *wp->agent_meta);
	wp->agent_meta->spawn_time = time(NULL);
	wp->agent_meta->last_activity = time(NULL);
	wp->agent_meta->status = xstrdup("idle");
	wp->agent_meta->model_name = xstrdup("unknown");
	wp->agent_meta->agent_type = xstrdup("generic");
	wp->agent_meta->token_count = 0.0;
	wp->agent_meta->cost_usd = 0.0;
	wp->agent_meta->budget_limit = 10.0;  /* Default $10 limit */
	wp->agent_meta->budget_alert_sent = 0;
	wp->agent_meta->mcp_connections = 0;
}

/* Free agent metadata */
void
agent_metadata_free(struct window_pane *wp)
{
	if (wp->agent_meta == NULL)
		return;

	free(wp->agent_meta->agent_type);
	free(wp->agent_meta->task_id);
	free(wp->agent_meta->parent_agent);
	free(wp->agent_meta->model_name);
	free(wp->agent_meta->status);
	free(wp->agent_meta);
	wp->agent_meta = NULL;
}

/* Set agent metadata field */
void
agent_metadata_set(struct window_pane *wp, const char *key, const char *value)
{
	if (wp->agent_meta == NULL)
		agent_metadata_init(wp);

	if (strcmp(key, "agent_type") == 0) {
		free(wp->agent_meta->agent_type);
		wp->agent_meta->agent_type = xstrdup(value);
	} else if (strcmp(key, "task_id") == 0) {
		free(wp->agent_meta->task_id);
		wp->agent_meta->task_id = xstrdup(value);
	} else if (strcmp(key, "parent_agent") == 0) {
		free(wp->agent_meta->parent_agent);
		wp->agent_meta->parent_agent = xstrdup(value);
	} else if (strcmp(key, "model") == 0) {
		free(wp->agent_meta->model_name);
		wp->agent_meta->model_name = xstrdup(value);
	} else if (strcmp(key, "status") == 0) {
		free(wp->agent_meta->status);
		wp->agent_meta->status = xstrdup(value);
		wp->agent_meta->last_activity = time(NULL);
	}
}

/* Get agent metadata field */
const char *
agent_metadata_get(struct window_pane *wp, const char *key)
{
	if (wp->agent_meta == NULL)
		return NULL;

	if (strcmp(key, "agent_type") == 0)
		return wp->agent_meta->agent_type;
	if (strcmp(key, "task_id") == 0)
		return wp->agent_meta->task_id;
	if (strcmp(key, "parent_agent") == 0)
		return wp->agent_meta->parent_agent;
	if (strcmp(key, "model") == 0)
		return wp->agent_meta->model_name;
	if (strcmp(key, "status") == 0)
		return wp->agent_meta->status;

	return NULL;
}

/* Update cost tracking based on model pricing */
void
agent_metadata_update_cost(struct window_pane *wp, double input_tokens, double output_tokens)
{
	double cost = 0.0;

	if (wp->agent_meta == NULL)
		agent_metadata_init(wp);

	/* Calculate cost based on model (prices per 1K tokens) */
	if (strcmp(wp->agent_meta->model_name, "opus-4") == 0) {
		cost = (input_tokens / 1000.0) * 0.015 + (output_tokens / 1000.0) * 0.075;
	} else if (strcmp(wp->agent_meta->model_name, "sonnet-4") == 0) {
		cost = (input_tokens / 1000.0) * 0.003 + (output_tokens / 1000.0) * 0.015;
	} else if (strcmp(wp->agent_meta->model_name, "haiku") == 0) {
		cost = (input_tokens / 1000.0) * 0.00025 + (output_tokens / 1000.0) * 0.00125;
	} else {
		/* Default to Sonnet pricing */
		cost = (input_tokens / 1000.0) * 0.003 + (output_tokens / 1000.0) * 0.015;
	}

	wp->agent_meta->cost_usd += cost;
	wp->agent_meta->token_count += input_tokens + output_tokens;

	/* Check budget and trigger alerts if needed */
	if (!wp->agent_meta->budget_alert_sent &&
	    wp->agent_meta->cost_usd >= wp->agent_meta->budget_limit * 0.8) {
		wp->agent_meta->budget_alert_sent = 1;
		/* TODO: Trigger visual/audio alert via Arduino */
		/* TODO: Change pane border color to yellow/red */
	}
}
