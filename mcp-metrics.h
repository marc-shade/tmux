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

#ifndef MCP_METRICS_H
#define MCP_METRICS_H

/*
 * MCP metrics tracking for performance monitoring.
 * Tracks latency, success rates, throughput, and connection health.
 */

#include <sys/time.h>

#define MCP_METRICS_HISTORY_SIZE 1000	/* Number of samples to track */
#define MCP_MAX_ERROR_TYPES 32		/* Maximum error types per server */

/* Latency percentile statistics */
struct mcp_latency_stats {
	u_int64_t	samples[MCP_METRICS_HISTORY_SIZE];
	size_t		sample_count;
	size_t		sample_index;	/* Circular buffer index */

	/* Computed statistics */
	u_int64_t	min_us;		/* Minimum latency (microseconds) */
	u_int64_t	max_us;		/* Maximum latency (microseconds) */
	u_int64_t	avg_us;		/* Average latency (microseconds) */
	u_int64_t	p95_us;		/* 95th percentile (microseconds) */
	u_int64_t	p99_us;		/* 99th percentile (microseconds) */

	int		dirty;		/* Stats need recalculation */
};

/* Error type tracking */
struct mcp_error_info {
	char		*error_type;	/* Error category (e.g., "timeout") */
	u_int		count;		/* Number of occurrences */
	time_t		last_seen;	/* Last occurrence timestamp */
};

/* Throughput tracking */
struct mcp_throughput {
	u_int64_t	bytes_sent;
	u_int64_t	bytes_received;
	u_int		messages_sent;
	u_int		messages_received;
	struct timeval	window_start;	/* Start of measurement window */

	/* Computed rates */
	double		bytes_per_sec;
	double		messages_per_sec;
};

/* Connection health tracking */
struct mcp_health {
	time_t		connected_at;	/* Connection start time */
	time_t		last_activity;	/* Last successful activity */
	u_int		reconnections;	/* Number of reconnections */
	u_int		timeouts;	/* Number of timeout events */
	double		uptime_ratio;	/* Uptime percentage */
};

/* Per-server metrics */
struct mcp_metrics {
	char			*server_name;

	/* Latency tracking */
	struct mcp_latency_stats latency;

	/* Success/failure rates */
	u_int			calls_total;
	u_int			calls_success;
	u_int			calls_failed;
	double			success_rate;

	/* Error tracking */
	struct mcp_error_info	errors[MCP_MAX_ERROR_TYPES];
	size_t			error_type_count;

	/* Throughput */
	struct mcp_throughput	throughput;

	/* Connection health */
	struct mcp_health	health;

	/* Timestamps */
	time_t			created_at;
	time_t			last_reset;
};

/* Metrics snapshot (for reporting) */
struct mcp_metrics_snapshot {
	char		*server_name;
	time_t		timestamp;

	/* Latency summary */
	u_int64_t	latency_min_us;
	u_int64_t	latency_max_us;
	u_int64_t	latency_avg_us;
	u_int64_t	latency_p95_us;
	u_int64_t	latency_p99_us;

	/* Call statistics */
	u_int		total_calls;
	u_int		successful_calls;
	u_int		failed_calls;
	double		success_rate;

	/* Throughput */
	double		bytes_per_sec;
	double		messages_per_sec;

	/* Health */
	double		uptime_ratio;
	u_int		reconnections;

	/* Top errors */
	char		*top_error_type;
	u_int		top_error_count;
};

/* Function prototypes */

/* Lifecycle */
struct mcp_metrics	*mcp_metrics_create(const char *);
void			 mcp_metrics_destroy(struct mcp_metrics *);
void			 mcp_metrics_reset(struct mcp_metrics *);

/* Recording */
void			 mcp_metrics_record_call(struct mcp_metrics *,
				     u_int64_t, int);
void			 mcp_metrics_record_error(struct mcp_metrics *,
				     const char *);
void			 mcp_metrics_record_bytes(struct mcp_metrics *,
				     size_t, size_t);
void			 mcp_metrics_record_reconnection(struct mcp_metrics *);
void			 mcp_metrics_record_timeout(struct mcp_metrics *);

/* Statistics */
void			 mcp_metrics_update_stats(struct mcp_metrics *);
struct mcp_metrics_snapshot *mcp_metrics_get_snapshot(struct mcp_metrics *);
void			 mcp_metrics_snapshot_free(struct mcp_metrics_snapshot *);

/* Helpers */
u_int64_t		 mcp_metrics_now_us(void);
u_int64_t		 mcp_metrics_percentile(u_int64_t *, size_t, double);

#endif /* MCP_METRICS_H */
