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

#ifndef CONTEXT_COMPRESS_H
#define CONTEXT_COMPRESS_H

#include "context-semantic.h"

/*
 * Phase 4.4C: Advanced Context Management - Context Compression
 *
 * This module provides intelligent compression of semantic context
 * by removing redundant information and summarizing repetitive patterns.
 */

/* Compression statistics */
struct compress_stats {
	size_t		 original_size;		/* Original context size */
	size_t		 compressed_size;	/* Compressed size */
	float		 compression_ratio;	/* Ratio achieved */
	u_int		 items_removed;		/* Items deduplicated */
	u_int		 items_merged;		/* Items merged */
	time_t		 compressed_at;		/* When compressed */
};

/* Compressed context */
struct compressed_context {
	struct semantic_context	*semantic;	/* Original semantic context */
	char			*summary;	/* Text summary */
	struct compress_stats	 stats;		/* Compression stats */
	float			 quality;	/* Quality after compression */
};

/* Initialize compression system */
void		 context_compress_init(void);

/* Compress semantic context */
struct compressed_context	*context_compress(struct semantic_context *);

/* Free compressed context */
void		 context_compress_free(struct compressed_context *);

/* Deduplicate semantic items */
int		 context_compress_deduplicate(struct semantic_context *);

/* Merge similar items */
int		 context_compress_merge_similar(struct semantic_context *,
		     float);

/* Generate text summary of context */
char		*context_compress_generate_summary(struct semantic_context *);

/* Calculate compression ratio */
float		 context_compress_calculate_ratio(struct semantic_context *,
		     struct semantic_context *);

/* Export compressed context to JSON */
char		*context_compress_export_json(struct compressed_context *);

/* Import compressed context from JSON */
struct compressed_context	*context_compress_import_json(const char *);

/* Decompress context (restore original) */
struct semantic_context	*context_compress_decompress(
			     struct compressed_context *);

/* Get compression statistics */
struct compress_stats	 context_compress_get_stats(
			     struct compressed_context *);

#endif /* !CONTEXT_COMPRESS_H */
