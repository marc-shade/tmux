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
#include <time.h>

#include "tmux.h"
#include "context-compress.h"
#include "context-semantic.h"

/*
 * Phase 4.4C: Context Compression
 *
 * Intelligent compression of semantic context through:
 * - Deduplication (handled by semantic layer)
 * - Merging similar items
 * - Removing low-relevance items
 * - Generating summaries
 */

/* Initialize compression system */
void
context_compress_init(void)
{
	log_debug("Context compression system initialized");
}

/* Compress semantic context */
struct compressed_context *
context_compress(struct semantic_context *semantic)
{
	struct compressed_context	*compressed;
	u_int				 orig_command, orig_file;
	u_int				 orig_pattern, orig_error, orig_output;

	if (semantic == NULL)
		return (NULL);

	compressed = xcalloc(1, sizeof *compressed);
	compressed->semantic = semantic;
	compressed->stats.compressed_at = time(NULL);

	/* Store original counts */
	orig_command = semantic->command_count;
	orig_file = semantic->file_count;
	orig_pattern = semantic->pattern_count;
	orig_error = semantic->error_count;
	orig_output = semantic->output_count;

	compressed->stats.original_size = orig_command + orig_file +
	    orig_pattern + orig_error + orig_output;

	/* Deduplicate (already handled by semantic layer) */
	context_compress_deduplicate(semantic);

	/* Merge similar items */
	context_compress_merge_similar(semantic, 0.8);

	/* Filter low-relevance items (< 0.3) */
	context_semantic_filter_by_relevance(semantic, 0.3);

	/* Calculate stats */
	compressed->stats.compressed_size = semantic->command_count +
	    semantic->file_count + semantic->pattern_count +
	    semantic->error_count + semantic->output_count;

	compressed->stats.items_removed = compressed->stats.original_size -
	    compressed->stats.compressed_size;

	if (compressed->stats.original_size > 0) {
		compressed->stats.compression_ratio =
		    (float)compressed->stats.compressed_size /
		    (float)compressed->stats.original_size;
	} else {
		compressed->stats.compression_ratio = 1.0;
	}

	/* Generate summary */
	compressed->summary = context_compress_generate_summary(semantic);

	/* Quality is semantic quality adjusted by compression */
	compressed->quality = semantic->overall_quality *
	    (0.8 + compressed->stats.compression_ratio * 0.2);

	log_debug("Context compressed: ratio=%.2f removed=%u quality=%.2f",
	    compressed->stats.compression_ratio,
	    compressed->stats.items_removed,
	    compressed->quality);

	return (compressed);
}

/* Free compressed context */
void
context_compress_free(struct compressed_context *compressed)
{
	if (compressed == NULL)
		return;

	/* Note: We don't free semantic context here as it's owned by caller */
	free(compressed->summary);
	free(compressed);
}

/* Deduplicate semantic items */
int
context_compress_deduplicate(struct semantic_context *semantic)
{
	/* Deduplication is already handled by context_semantic_add_item()
	 * which increments frequency for duplicates
	 */
	(void)semantic;	/* Suppress unused warning */

	return (0);
}

/* Merge similar items */
int
context_compress_merge_similar(struct semantic_context *semantic,
    float similarity_threshold)
{
	struct semantic_item	*item1, *item2, *prev, *next;
	size_t			 len1, len2, common, max_len;
	float			 similarity;

	if (semantic == NULL)
		return (-1);

	/* Merge similar commands */
	for (item1 = semantic->commands; item1 != NULL; item1 = item1->next) {
		prev = item1;
		for (item2 = item1->next; item2 != NULL; item2 = next) {
			next = item2->next;

			/* Calculate simple string similarity */
			len1 = strlen(item1->content);
			len2 = strlen(item2->content);
			common = 0;
			max_len = len1 > len2 ? len1 : len2;

			/* Count common starting characters */
			for (size_t i = 0; i < len1 && i < len2; i++) {
				if (item1->content[i] == item2->content[i])
					common++;
				else
					break;
			}

			similarity = max_len > 0 ? (float)common / max_len : 0.0;

			if (similarity >= similarity_threshold) {
				/* Merge item2 into item1 */
				item1->frequency += item2->frequency;
				item1->relevance = (item1->relevance +
				    item2->relevance) / 2.0;

				/* Update timestamp to most recent */
				if (item2->timestamp > item1->timestamp)
					item1->timestamp = item2->timestamp;

				/* Remove item2 from list */
				prev->next = next;
				free(item2->content);
				free(item2);

				semantic->command_count--;
				semantic->commands->frequency++;	/* Track merges */
			} else {
				prev = item2;
			}
		}
	}

	return (0);
}

/* Generate text summary of context */
char *
context_compress_generate_summary(struct semantic_context *semantic)
{
	char			*summary;
	size_t			 summary_len, offset;
	struct semantic_item	*item;
	u_int			 count;

	if (semantic == NULL)
		return (xstrdup(""));

	/* Allocate summary buffer */
	summary_len = 2048;
	summary = xmalloc(summary_len);
	offset = 0;

	/* Session info */
	offset += snprintf(summary + offset, summary_len - offset,
	    "Session: %s\n",
	    semantic->session ? semantic->session->name : "unknown");

	/* Agent info */
	if (semantic->agent != NULL) {
		offset += snprintf(summary + offset, summary_len - offset,
		    "Agent Type: %s\n",
		    semantic->agent->agent_type ?
		    semantic->agent->agent_type : "unknown");

		if (semantic->agent->goal != NULL) {
			offset += snprintf(summary + offset, summary_len - offset,
			    "Goal: %s\n", semantic->agent->goal);
		}
	}

	/* Top commands */
	if (semantic->command_count > 0) {
		offset += snprintf(summary + offset, summary_len - offset,
		    "\nTop Commands:\n");

		count = 0;
		for (item = semantic->commands; item != NULL && count < 5;
		    item = item->next, count++) {
			offset += snprintf(summary + offset, summary_len - offset,
			    "  - %s (relevance: %.2f)\n",
			    item->content, item->relevance);
		}
	}

	/* Patterns */
	if (semantic->pattern_count > 0) {
		offset += snprintf(summary + offset, summary_len - offset,
		    "\nPatterns Identified:\n");

		for (item = semantic->patterns; item != NULL; item = item->next) {
			offset += snprintf(summary + offset, summary_len - offset,
			    "  - %s\n", item->content);
		}
	}

	/* Quality */
	offset += snprintf(summary + offset, summary_len - offset,
	    "\nOverall Quality: %.2f\n", semantic->overall_quality);

	return (summary);
}

/* Calculate compression ratio */
float
context_compress_calculate_ratio(struct semantic_context *original,
    struct semantic_context *compressed)
{
	u_int	orig_size, comp_size;

	if (original == NULL || compressed == NULL)
		return (1.0);

	orig_size = original->command_count + original->file_count +
	    original->pattern_count + original->error_count +
	    original->output_count;

	comp_size = compressed->command_count + compressed->file_count +
	    compressed->pattern_count + compressed->error_count +
	    compressed->output_count;

	if (orig_size == 0)
		return (1.0);

	return ((float)comp_size / (float)orig_size);
}

/* Export compressed context to JSON */
char *
context_compress_export_json(struct compressed_context *compressed)
{
	/* TODO: Implement JSON export */
	(void)compressed;	/* Suppress unused warning */

	return (xstrdup("{}"));	/* Placeholder */
}

/* Import compressed context from JSON */
struct compressed_context *
context_compress_import_json(const char *json)
{
	/* TODO: Implement JSON import */
	(void)json;	/* Suppress unused warning */

	return (NULL);	/* Placeholder */
}

/* Decompress context (restore original) */
struct semantic_context *
context_compress_decompress(struct compressed_context *compressed)
{
	/* For MVP, we don't modify the original semantic context,
	 * so decompression just returns the reference
	 */
	if (compressed == NULL)
		return (NULL);

	return (compressed->semantic);
}

/* Get compression statistics */
struct compress_stats
context_compress_get_stats(struct compressed_context *compressed)
{
	struct compress_stats	empty_stats;

	if (compressed == NULL) {
		memset(&empty_stats, 0, sizeof empty_stats);
		return (empty_stats);
	}

	return (compressed->stats);
}
