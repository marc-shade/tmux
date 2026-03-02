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

#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"
#include "mcp-metrics.h"

/* Helper: Get current time in microseconds */
u_int64_t
mcp_metrics_now_us(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return ((u_int64_t)tv.tv_sec * 1000000 + tv.tv_usec);
}

/* Helper: Calculate percentile from sorted array */
u_int64_t
mcp_metrics_percentile(u_int64_t *samples, size_t count, double percentile)
{
	size_t index;

	if (count == 0)
		return (0);

	/* Convert percentile (0.0-1.0) to index */
	index = (size_t)((double)(count - 1) * percentile);
	if (index >= count)
		index = count - 1;

	return (samples[index]);
}

/* Compare function for qsort */
static int
compare_u64(const void *a, const void *b)
{
	u_int64_t va = *(const u_int64_t *)a;
	u_int64_t vb = *(const u_int64_t *)b;

	if (va < vb)
		return (-1);
	if (va > vb)
		return (1);
	return (0);
}

/* Create metrics tracker for a server */
struct mcp_metrics *
mcp_metrics_create(const char *server_name)
{
	struct mcp_metrics *metrics;

	metrics = xcalloc(1, sizeof *metrics);
	metrics->server_name = xstrdup(server_name);
	metrics->created_at = time(NULL);
	metrics->last_reset = metrics->created_at;

	/* Initialize latency stats */
	metrics->latency.dirty = 1;

	/* Initialize throughput window */
	gettimeofday(&metrics->throughput.window_start, NULL);

	/* Initialize health */
	metrics->health.connected_at = metrics->created_at;
	metrics->health.last_activity = metrics->created_at;

	return (metrics);
}

/* Destroy metrics tracker */
void
mcp_metrics_destroy(struct mcp_metrics *metrics)
{
	size_t i;

	if (metrics == NULL)
		return;

	free(metrics->server_name);

	/* Free error type strings */
	for (i = 0; i < metrics->error_type_count; i++)
		free(metrics->errors[i].error_type);

	free(metrics);
}

/* Reset all statistics */
void
mcp_metrics_reset(struct mcp_metrics *metrics)
{
	size_t i;

	if (metrics == NULL)
		return;

	/* Reset latency */
	memset(&metrics->latency, 0, sizeof metrics->latency);
	metrics->latency.dirty = 1;

	/* Reset calls */
	metrics->calls_total = 0;
	metrics->calls_success = 0;
	metrics->calls_failed = 0;
	metrics->success_rate = 0.0;

	/* Reset errors */
	for (i = 0; i < metrics->error_type_count; i++)
		free(metrics->errors[i].error_type);
	memset(metrics->errors, 0, sizeof metrics->errors);
	metrics->error_type_count = 0;

	/* Reset throughput */
	memset(&metrics->throughput, 0, sizeof metrics->throughput);
	gettimeofday(&metrics->throughput.window_start, NULL);

	/* Reset health counters (but not timestamps) */
	metrics->health.reconnections = 0;
	metrics->health.timeouts = 0;

	metrics->last_reset = time(NULL);
}

/* Record a call (latency and success/failure) */
void
mcp_metrics_record_call(struct mcp_metrics *metrics, u_int64_t latency_us,
    int success)
{
	struct mcp_latency_stats *lat;

	if (metrics == NULL)
		return;

	/* Record latency */
	lat = &metrics->latency;
	lat->samples[lat->sample_index] = latency_us;
	lat->sample_index = (lat->sample_index + 1) % MCP_METRICS_HISTORY_SIZE;
	if (lat->sample_count < MCP_METRICS_HISTORY_SIZE)
		lat->sample_count++;
	lat->dirty = 1;

	/* Record success/failure */
	metrics->calls_total++;
	if (success)
		metrics->calls_success++;
	else
		metrics->calls_failed++;

	/* Update success rate */
	if (metrics->calls_total > 0) {
		metrics->success_rate = (double)metrics->calls_success /
		    (double)metrics->calls_total;
	}

	/* Update last activity */
	metrics->health.last_activity = time(NULL);
}

/* Record an error */
void
mcp_metrics_record_error(struct mcp_metrics *metrics, const char *error_type)
{
	struct mcp_error_info *error;
	size_t i;

	if (metrics == NULL || error_type == NULL)
		return;

	/* Find existing error type */
	for (i = 0; i < metrics->error_type_count; i++) {
		error = &metrics->errors[i];
		if (strcmp(error->error_type, error_type) == 0) {
			error->count++;
			error->last_seen = time(NULL);
			return;
		}
	}

	/* Add new error type */
	if (metrics->error_type_count < MCP_MAX_ERROR_TYPES) {
		error = &metrics->errors[metrics->error_type_count++];
		error->error_type = xstrdup(error_type);
		error->count = 1;
		error->last_seen = time(NULL);
	}
}

/* Record bytes sent/received */
void
mcp_metrics_record_bytes(struct mcp_metrics *metrics, size_t bytes_sent,
    size_t bytes_received)
{
	if (metrics == NULL)
		return;

	metrics->throughput.bytes_sent += bytes_sent;
	metrics->throughput.bytes_received += bytes_received;

	if (bytes_sent > 0)
		metrics->throughput.messages_sent++;
	if (bytes_received > 0)
		metrics->throughput.messages_received++;
}

/* Record a reconnection event */
void
mcp_metrics_record_reconnection(struct mcp_metrics *metrics)
{
	if (metrics == NULL)
		return;

	metrics->health.reconnections++;
	metrics->health.connected_at = time(NULL);
}

/* Record a timeout event */
void
mcp_metrics_record_timeout(struct mcp_metrics *metrics)
{
	if (metrics == NULL)
		return;

	metrics->health.timeouts++;
}

/* Update computed statistics */
void
mcp_metrics_update_stats(struct mcp_metrics *metrics)
{
	struct mcp_latency_stats *lat;
	struct timeval now, elapsed;
	u_int64_t sorted[MCP_METRICS_HISTORY_SIZE];
	u_int64_t sum;
	size_t i;
	double elapsed_sec;
	time_t now_time, uptime, total_time;

	if (metrics == NULL)
		return;

	/* Update latency statistics */
	lat = &metrics->latency;
	if (lat->dirty && lat->sample_count > 0) {
		/* Copy samples for sorting */
		memcpy(sorted, lat->samples,
		    lat->sample_count * sizeof(u_int64_t));
		qsort(sorted, lat->sample_count, sizeof(u_int64_t),
		    compare_u64);

		/* Calculate statistics */
		lat->min_us = sorted[0];
		lat->max_us = sorted[lat->sample_count - 1];

		sum = 0;
		for (i = 0; i < lat->sample_count; i++)
			sum += sorted[i];
		lat->avg_us = sum / lat->sample_count;

		lat->p95_us = mcp_metrics_percentile(sorted, lat->sample_count,
		    0.95);
		lat->p99_us = mcp_metrics_percentile(sorted, lat->sample_count,
		    0.99);

		lat->dirty = 0;
	}

	/* Update throughput rates */
	gettimeofday(&now, NULL);
	timersub(&now, &metrics->throughput.window_start, &elapsed);
	elapsed_sec = (double)elapsed.tv_sec + (double)elapsed.tv_usec / 1e6;

	if (elapsed_sec > 0.0) {
		metrics->throughput.bytes_per_sec =
		    (double)(metrics->throughput.bytes_sent +
		    metrics->throughput.bytes_received) / elapsed_sec;
		metrics->throughput.messages_per_sec =
		    (double)(metrics->throughput.messages_sent +
		    metrics->throughput.messages_received) / elapsed_sec;
	}

	/* Update uptime ratio */
	now_time = time(NULL);
	total_time = now_time - metrics->created_at;
	uptime = now_time - metrics->health.connected_at;

	if (total_time > 0) {
		metrics->health.uptime_ratio = (double)uptime /
		    (double)total_time;
	}
}

/* Get snapshot of current metrics */
struct mcp_metrics_snapshot *
mcp_metrics_get_snapshot(struct mcp_metrics *metrics)
{
	struct mcp_metrics_snapshot *snapshot;
	struct mcp_error_info *top_error;
	size_t i;

	if (metrics == NULL)
		return (NULL);

	/* Update computed statistics first */
	mcp_metrics_update_stats(metrics);

	snapshot = xcalloc(1, sizeof *snapshot);
	snapshot->server_name = xstrdup(metrics->server_name);
	snapshot->timestamp = time(NULL);

	/* Latency */
	snapshot->latency_min_us = metrics->latency.min_us;
	snapshot->latency_max_us = metrics->latency.max_us;
	snapshot->latency_avg_us = metrics->latency.avg_us;
	snapshot->latency_p95_us = metrics->latency.p95_us;
	snapshot->latency_p99_us = metrics->latency.p99_us;

	/* Calls */
	snapshot->total_calls = metrics->calls_total;
	snapshot->successful_calls = metrics->calls_success;
	snapshot->failed_calls = metrics->calls_failed;
	snapshot->success_rate = metrics->success_rate;

	/* Throughput */
	snapshot->bytes_per_sec = metrics->throughput.bytes_per_sec;
	snapshot->messages_per_sec = metrics->throughput.messages_per_sec;

	/* Health */
	snapshot->uptime_ratio = metrics->health.uptime_ratio;
	snapshot->reconnections = metrics->health.reconnections;

	/* Find top error */
	top_error = NULL;
	for (i = 0; i < metrics->error_type_count; i++) {
		if (top_error == NULL ||
		    metrics->errors[i].count > top_error->count)
			top_error = &metrics->errors[i];
	}

	if (top_error != NULL) {
		snapshot->top_error_type = xstrdup(top_error->error_type);
		snapshot->top_error_count = top_error->count;
	}

	return (snapshot);
}

/* Free snapshot */
void
mcp_metrics_snapshot_free(struct mcp_metrics_snapshot *snapshot)
{
	if (snapshot == NULL)
		return;

	free(snapshot->server_name);
	free(snapshot->top_error_type);
	free(snapshot);
}
