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

#ifndef MCP_ASYNC_H
#define MCP_ASYNC_H

#include <event.h>
#include "mcp-client.h"

/*
 * MCP Async Operations Framework
 *
 * Phase 4.2: Non-blocking asynchronous MCP operations using libevent.
 * Prevents tmux from blocking while waiting for MCP server responses.
 *
 * Features:
 * - Asynchronous tool calls with callbacks
 * - Request queuing and prioritization
 * - Timeout handling
 * - Parallel request multiplexing
 * - Background context saving
 */

/* Request priorities */
enum mcp_async_priority {
	MCP_PRIORITY_LOW = 0,
	MCP_PRIORITY_NORMAL = 1,
	MCP_PRIORITY_HIGH = 2,
	MCP_PRIORITY_URGENT = 3
};

/* Request state */
enum mcp_async_state {
	MCP_ASYNC_QUEUED,		/* Waiting to be sent */
	MCP_ASYNC_SENDING,		/* Being sent to server */
	MCP_ASYNC_WAITING,		/* Waiting for response */
	MCP_ASYNC_COMPLETED,		/* Response received */
	MCP_ASYNC_FAILED,		/* Request failed */
	MCP_ASYNC_TIMEOUT,		/* Request timed out */
	MCP_ASYNC_CANCELLED		/* Cancelled by user */
};

/* Forward declarations */
struct mcp_async_request;
struct mcp_async_context;

/* Async callback signature */
typedef void (*mcp_async_callback)(struct mcp_async_request *, void *);

/* Async request structure */
struct mcp_async_request {
	/* Request identification */
	int			id;		/* Unique request ID */
	char			*server_name;	/* Target server */
	char			*tool_name;	/* Tool to call */
	char			*arguments;	/* JSON arguments */

	/* Request state */
	enum mcp_async_state	state;
	enum mcp_async_priority	priority;
	time_t			queued_at;	/* When queued */
	time_t			sent_at;	/* When sent */
	time_t			completed_at;	/* When completed */

	/* Timeout configuration */
	int			timeout_ms;	/* Timeout in milliseconds */
	struct event		*timeout_event;	/* libevent timeout */

	/* Callback */
	mcp_async_callback	callback;	/* Completion callback */
	void			*callback_data;	/* User data for callback */

	/* Response data */
	struct mcp_response	*response;	/* Response (if completed) */
	char			*error_msg;	/* Error message (if failed) */

	/* Queue linkage */
	TAILQ_ENTRY(mcp_async_request) entry;
};

/* Request queue */
TAILQ_HEAD(mcp_async_queue, mcp_async_request);

/* Async context (one per MCP client) */
struct mcp_async_context {
	struct mcp_client	*client;	/* Associated MCP client */
	struct event_base	*event_base;	/* libevent base */

	/* Request queues by priority */
	struct mcp_async_queue	urgent_queue;
	struct mcp_async_queue	high_queue;
	struct mcp_async_queue	normal_queue;
	struct mcp_async_queue	low_queue;

	/* Active requests (waiting for response) */
	struct mcp_async_queue	active_queue;

	/* Per-server limits */
	int			max_concurrent;	/* Max concurrent per server */
	int			*server_active;	/* Active count per server */

	/* Statistics */
	int			next_request_id;
	u_int			total_queued;
	u_int			total_completed;
	u_int			total_failed;
	u_int			total_timeout;
	u_int			total_cancelled;

	/* Event monitoring */
	struct event		**read_events;	/* Read events per connection */
	int			num_read_events;
};

/* Function prototypes */

/* Context lifecycle */
struct mcp_async_context	*mcp_async_create(struct mcp_client *);
void				mcp_async_destroy(struct mcp_async_context *);
int				mcp_async_init(struct mcp_async_context *);

/* Async operations */
struct mcp_async_request	*mcp_async_call_tool(struct mcp_async_context *,
					const char *server_name,
					const char *tool_name,
					const char *arguments,
					enum mcp_async_priority priority,
					int timeout_ms,
					mcp_async_callback callback,
					void *callback_data);

struct mcp_async_request	*mcp_async_list_tools(struct mcp_async_context *,
					const char *server_name,
					enum mcp_async_priority priority,
					int timeout_ms,
					mcp_async_callback callback,
					void *callback_data);

/* Request management */
int				mcp_async_cancel(struct mcp_async_context *,
					struct mcp_async_request *);
void				mcp_async_request_free(struct mcp_async_request *);
struct mcp_async_request	*mcp_async_find_request(struct mcp_async_context *,
					int request_id);

/* Queue management */
void				mcp_async_queue_request(struct mcp_async_context *,
					struct mcp_async_request *);
struct mcp_async_request	*mcp_async_dequeue_next(struct mcp_async_context *);
int				mcp_async_process_queue(struct mcp_async_context *);

/* Event loop integration */
void				mcp_async_event_loop(struct mcp_async_context *);
void				mcp_async_stop_loop(struct mcp_async_context *);

/* Callbacks (internal) */
void				mcp_async_read_callback(evutil_socket_t,
					short, void *);
void				mcp_async_timeout_callback(evutil_socket_t,
					short, void *);
void				mcp_async_dispatch_callback(evutil_socket_t,
					short, void *);

/* Statistics and monitoring */
void				mcp_async_get_stats(struct mcp_async_context *,
					u_int *queued, u_int *active,
					u_int *completed, u_int *failed);
int				mcp_async_queue_depth(struct mcp_async_context *);
int				mcp_async_active_count(struct mcp_async_context *,
					const char *server_name);

/* Parallel operations */
int				mcp_async_call_parallel(struct mcp_async_context *,
					struct mcp_async_request **requests,
					int num_requests);
void				mcp_async_wait_all(struct mcp_async_context *,
					struct mcp_async_request **requests,
					int num_requests);

/* Background context saving */
int				mcp_async_save_context_bg(struct mcp_async_context *,
					const char *session_name,
					const char *context_data,
					mcp_async_callback callback,
					void *callback_data);

#endif /* MCP_ASYNC_H */
