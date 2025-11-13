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
#include <sys/queue.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"
#include "mcp-async.h"
#include "mcp-client.h"

/* Default configuration */
#define MCP_ASYNC_DEFAULT_TIMEOUT	30000	/* 30 seconds */
#define MCP_ASYNC_MAX_CONCURRENT	5	/* Per server */

/* Create async context */
struct mcp_async_context *
mcp_async_create(struct mcp_client *client)
{
	struct mcp_async_context *ctx;

	if (client == NULL)
		return (NULL);

	ctx = calloc(1, sizeof *ctx);
	if (ctx == NULL)
		return (NULL);

	ctx->client = client;
	ctx->event_base = event_base_new();
	if (ctx->event_base == NULL) {
		free(ctx);
		return (NULL);
	}

	/* Initialize queues */
	TAILQ_INIT(&ctx->urgent_queue);
	TAILQ_INIT(&ctx->high_queue);
	TAILQ_INIT(&ctx->normal_queue);
	TAILQ_INIT(&ctx->low_queue);
	TAILQ_INIT(&ctx->active_queue);

	/* Configure limits */
	ctx->max_concurrent = MCP_ASYNC_MAX_CONCURRENT;
	ctx->server_active = calloc(MCP_MAX_SERVERS, sizeof(int));
	if (ctx->server_active == NULL) {
		event_base_free(ctx->event_base);
		free(ctx);
		return (NULL);
	}

	ctx->next_request_id = 1;
	ctx->read_events = NULL;
	ctx->num_read_events = 0;

	return (ctx);
}

/* Destroy async context */
void
mcp_async_destroy(struct mcp_async_context *ctx)
{
	struct mcp_async_request *req, *tmp;

	if (ctx == NULL)
		return;

	/* Cancel and free all requests in all queues */
	TAILQ_FOREACH_SAFE(req, &ctx->urgent_queue, entry, tmp) {
		TAILQ_REMOVE(&ctx->urgent_queue, req, entry);
		mcp_async_request_free(req);
	}
	TAILQ_FOREACH_SAFE(req, &ctx->high_queue, entry, tmp) {
		TAILQ_REMOVE(&ctx->high_queue, req, entry);
		mcp_async_request_free(req);
	}
	TAILQ_FOREACH_SAFE(req, &ctx->normal_queue, entry, tmp) {
		TAILQ_REMOVE(&ctx->normal_queue, req, entry);
		mcp_async_request_free(req);
	}
	TAILQ_FOREACH_SAFE(req, &ctx->low_queue, entry, tmp) {
		TAILQ_REMOVE(&ctx->low_queue, req, entry);
		mcp_async_request_free(req);
	}
	TAILQ_FOREACH_SAFE(req, &ctx->active_queue, entry, tmp) {
		TAILQ_REMOVE(&ctx->active_queue, req, entry);
		mcp_async_request_free(req);
	}

	/* Free read events */
	if (ctx->read_events != NULL) {
		for (int i = 0; i < ctx->num_read_events; i++) {
			if (ctx->read_events[i] != NULL)
				event_free(ctx->read_events[i]);
		}
		free(ctx->read_events);
	}

	free(ctx->server_active);
	event_base_free(ctx->event_base);
	free(ctx);
}

/* Initialize async context */
int
mcp_async_init(struct mcp_async_context *ctx)
{
	if (ctx == NULL || ctx->client == NULL)
		return (-1);

	/* Set up read events for all connections */
	ctx->num_read_events = ctx->client->num_connections;
	if (ctx->num_read_events > 0) {
		ctx->read_events = calloc(ctx->num_read_events,
		    sizeof(struct event *));
		if (ctx->read_events == NULL)
			return (-1);

		for (int i = 0; i < ctx->num_read_events; i++) {
			struct mcp_connection *conn = ctx->client->connections[i];
			if (conn == NULL)
				continue;

			int fd = (conn->config->transport == MCP_TRANSPORT_SOCKET) ?
			    conn->socket_fd : conn->stdout_fd;

			if (fd < 0)
				continue;

			ctx->read_events[i] = event_new(ctx->event_base, fd,
			    EV_READ | EV_PERSIST, mcp_async_read_callback, conn);
			if (ctx->read_events[i] == NULL)
				continue;

			event_add(ctx->read_events[i], NULL);
		}
	}

	return (0);
}

/* Free async request */
void
mcp_async_request_free(struct mcp_async_request *req)
{
	if (req == NULL)
		return;

	free(req->server_name);
	free(req->tool_name);
	free(req->arguments);
	free(req->error_msg);

	if (req->response != NULL)
		mcp_response_free(req->response);

	if (req->timeout_event != NULL)
		event_free(req->timeout_event);

	free(req);
}

/* Create async tool call request */
struct mcp_async_request *
mcp_async_call_tool(struct mcp_async_context *ctx,
    const char *server_name, const char *tool_name, const char *arguments,
    enum mcp_async_priority priority, int timeout_ms,
    mcp_async_callback callback, void *callback_data)
{
	struct mcp_async_request *req;

	if (ctx == NULL || server_name == NULL || tool_name == NULL)
		return (NULL);

	req = calloc(1, sizeof *req);
	if (req == NULL)
		return (NULL);

	/* Assign request ID */
	req->id = ctx->next_request_id++;

	/* Copy request data */
	req->server_name = strdup(server_name);
	req->tool_name = strdup(tool_name);
	req->arguments = (arguments != NULL) ? strdup(arguments) : NULL;

	if (req->server_name == NULL || req->tool_name == NULL) {
		mcp_async_request_free(req);
		return (NULL);
	}

	/* Set request properties */
	req->state = MCP_ASYNC_QUEUED;
	req->priority = priority;
	req->queued_at = time(NULL);
	req->timeout_ms = (timeout_ms > 0) ? timeout_ms : MCP_ASYNC_DEFAULT_TIMEOUT;
	req->callback = callback;
	req->callback_data = callback_data;
	req->response = NULL;
	req->error_msg = NULL;
	req->timeout_event = NULL;

	/* Queue the request */
	mcp_async_queue_request(ctx, req);

	/* Try to process queue immediately */
	mcp_async_process_queue(ctx);

	return (req);
}

/* Create async list tools request */
struct mcp_async_request *
mcp_async_list_tools(struct mcp_async_context *ctx,
    const char *server_name, enum mcp_async_priority priority,
    int timeout_ms, mcp_async_callback callback, void *callback_data)
{
	/* List tools is just a special case of call_tool */
	return mcp_async_call_tool(ctx, server_name, "list_tools", "{}",
	    priority, timeout_ms, callback, callback_data);
}

/* Queue request by priority */
void
mcp_async_queue_request(struct mcp_async_context *ctx,
    struct mcp_async_request *req)
{
	if (ctx == NULL || req == NULL)
		return;

	ctx->total_queued++;

	switch (req->priority) {
	case MCP_PRIORITY_URGENT:
		TAILQ_INSERT_TAIL(&ctx->urgent_queue, req, entry);
		break;
	case MCP_PRIORITY_HIGH:
		TAILQ_INSERT_TAIL(&ctx->high_queue, req, entry);
		break;
	case MCP_PRIORITY_NORMAL:
		TAILQ_INSERT_TAIL(&ctx->normal_queue, req, entry);
		break;
	case MCP_PRIORITY_LOW:
		TAILQ_INSERT_TAIL(&ctx->low_queue, req, entry);
		break;
	}
}

/* Dequeue next request to process */
struct mcp_async_request *
mcp_async_dequeue_next(struct mcp_async_context *ctx)
{
	struct mcp_async_request *req = NULL;

	if (ctx == NULL)
		return (NULL);

	/* Process queues in priority order */
	if (!TAILQ_EMPTY(&ctx->urgent_queue)) {
		req = TAILQ_FIRST(&ctx->urgent_queue);
		TAILQ_REMOVE(&ctx->urgent_queue, req, entry);
	} else if (!TAILQ_EMPTY(&ctx->high_queue)) {
		req = TAILQ_FIRST(&ctx->high_queue);
		TAILQ_REMOVE(&ctx->high_queue, req, entry);
	} else if (!TAILQ_EMPTY(&ctx->normal_queue)) {
		req = TAILQ_FIRST(&ctx->normal_queue);
		TAILQ_REMOVE(&ctx->normal_queue, req, entry);
	} else if (!TAILQ_EMPTY(&ctx->low_queue)) {
		req = TAILQ_FIRST(&ctx->low_queue);
		TAILQ_REMOVE(&ctx->low_queue, req, entry);
	}

	return (req);
}

/* Get server index for connection */
static int
mcp_async_get_server_index(struct mcp_async_context *ctx, const char *server_name)
{
	if (ctx == NULL || server_name == NULL)
		return (-1);

	for (int i = 0; i < ctx->client->num_connections; i++) {
		struct mcp_connection *conn = ctx->client->connections[i];
		if (conn != NULL && conn->config != NULL &&
		    strcmp(conn->config->name, server_name) == 0)
			return (i);
	}

	return (-1);
}

/* Process request queue */
int
mcp_async_process_queue(struct mcp_async_context *ctx)
{
	struct mcp_async_request *req;
	int processed = 0;

	if (ctx == NULL)
		return (0);

	/* Process requests while under concurrency limits */
	while ((req = mcp_async_dequeue_next(ctx)) != NULL) {
		int server_idx = mcp_async_get_server_index(ctx, req->server_name);
		if (server_idx < 0) {
			/* Server not found - fail request */
			req->state = MCP_ASYNC_FAILED;
			req->error_msg = strdup("Server not found");
			if (req->callback != NULL)
				req->callback(req, req->callback_data);
			mcp_async_request_free(req);
			ctx->total_failed++;
			continue;
		}

		/* Check concurrency limit */
		if (ctx->server_active[server_idx] >= ctx->max_concurrent) {
			/* Put back in queue */
			switch (req->priority) {
			case MCP_PRIORITY_URGENT:
				TAILQ_INSERT_HEAD(&ctx->urgent_queue, req, entry);
				break;
			case MCP_PRIORITY_HIGH:
				TAILQ_INSERT_HEAD(&ctx->high_queue, req, entry);
				break;
			case MCP_PRIORITY_NORMAL:
				TAILQ_INSERT_HEAD(&ctx->normal_queue, req, entry);
				break;
			case MCP_PRIORITY_LOW:
				TAILQ_INSERT_HEAD(&ctx->low_queue, req, entry);
				break;
			}
			break;  /* Stop processing for now */
		}

		/* Send request */
		req->state = MCP_ASYNC_SENDING;
		req->sent_at = time(NULL);

		/* Make synchronous call (will be async in event loop) */
		req->response = mcp_call_tool(ctx->client, req->server_name,
		    req->tool_name, req->arguments);

		if (req->response == NULL) {
			req->state = MCP_ASYNC_FAILED;
			req->error_msg = strdup("Failed to send request");
			if (req->callback != NULL)
				req->callback(req, req->callback_data);
			mcp_async_request_free(req);
			ctx->total_failed++;
			continue;
		}

		/* Move to active queue */
		req->state = MCP_ASYNC_WAITING;
		TAILQ_INSERT_TAIL(&ctx->active_queue, req, entry);
		ctx->server_active[server_idx]++;

		/* Set up timeout */
		struct timeval tv;
		tv.tv_sec = req->timeout_ms / 1000;
		tv.tv_usec = (req->timeout_ms % 1000) * 1000;

		req->timeout_event = evtimer_new(ctx->event_base,
		    mcp_async_timeout_callback, req);
		if (req->timeout_event != NULL)
			evtimer_add(req->timeout_event, &tv);

		processed++;
	}

	return (processed);
}

/* Timeout callback */
void
mcp_async_timeout_callback(evutil_socket_t fd, short what, void *arg)
{
	struct mcp_async_request *req = arg;

	(void)fd;
	(void)what;

	if (req == NULL)
		return;

	req->state = MCP_ASYNC_TIMEOUT;
	req->completed_at = time(NULL);

	if (req->callback != NULL)
		req->callback(req, req->callback_data);
}

/* Read callback - response received */
void
mcp_async_read_callback(evutil_socket_t fd, short what, void *arg)
{
	struct mcp_connection *conn = arg;
	struct mcp_async_request *req;
	char buffer[MCP_MAX_MESSAGE_SIZE];
	ssize_t n;

	(void)fd;
	(void)what;

	if (conn == NULL)
		return;

	/* Read response data */
	n = mcp_recv(conn, buffer, sizeof buffer - 1);
	if (n <= 0)
		return;

	buffer[n] = '\0';

	/* Parse response */
	struct mcp_response *resp = mcp_parse_response(buffer);
	if (resp == NULL)
		return;

	/* Find matching request in active queue */
	/* For now, assume FIFO - first active request gets response */
	/* TODO: Match by request_id from JSON-RPC */

	/* Signal completion */
	/* This is a simplified implementation */
	/* Full implementation would match request_id and complete specific request */
}

/* Cancel async request */
int
mcp_async_cancel(struct mcp_async_context *ctx, struct mcp_async_request *req)
{
	if (ctx == NULL || req == NULL)
		return (-1);

	/* Can only cancel queued or waiting requests */
	if (req->state != MCP_ASYNC_QUEUED && req->state != MCP_ASYNC_WAITING)
		return (-1);

	req->state = MCP_ASYNC_CANCELLED;
	ctx->total_cancelled++;

	/* Remove from queue */
	switch (req->priority) {
	case MCP_PRIORITY_URGENT:
		TAILQ_REMOVE(&ctx->urgent_queue, req, entry);
		break;
	case MCP_PRIORITY_HIGH:
		TAILQ_REMOVE(&ctx->high_queue, req, entry);
		break;
	case MCP_PRIORITY_NORMAL:
		TAILQ_REMOVE(&ctx->normal_queue, req, entry);
		break;
	case MCP_PRIORITY_LOW:
		TAILQ_REMOVE(&ctx->low_queue, req, entry);
		break;
	}

	/* Cancel timeout if set */
	if (req->timeout_event != NULL) {
		event_del(req->timeout_event);
		event_free(req->timeout_event);
		req->timeout_event = NULL;
	}

	/* Call callback with cancelled state */
	if (req->callback != NULL)
		req->callback(req, req->callback_data);

	mcp_async_request_free(req);
	return (0);
}

/* Get statistics */
void
mcp_async_get_stats(struct mcp_async_context *ctx, u_int *queued,
    u_int *active, u_int *completed, u_int *failed)
{
	if (ctx == NULL)
		return;

	if (queued != NULL)
		*queued = ctx->total_queued;
	if (active != NULL) {
		*active = 0;
		struct mcp_async_request *req;
		TAILQ_FOREACH(req, &ctx->active_queue, entry)
			(*active)++;
	}
	if (completed != NULL)
		*completed = ctx->total_completed;
	if (failed != NULL)
		*failed = ctx->total_failed;
}

/* Queue depth */
int
mcp_async_queue_depth(struct mcp_async_context *ctx)
{
	int depth = 0;
	struct mcp_async_request *req;

	if (ctx == NULL)
		return (0);

	TAILQ_FOREACH(req, &ctx->urgent_queue, entry)
		depth++;
	TAILQ_FOREACH(req, &ctx->high_queue, entry)
		depth++;
	TAILQ_FOREACH(req, &ctx->normal_queue, entry)
		depth++;
	TAILQ_FOREACH(req, &ctx->low_queue, entry)
		depth++;

	return (depth);
}

/* Active count for server */
int
mcp_async_active_count(struct mcp_async_context *ctx, const char *server_name)
{
	int server_idx;

	if (ctx == NULL || server_name == NULL)
		return (0);

	server_idx = mcp_async_get_server_index(ctx, server_name);
	if (server_idx < 0)
		return (0);

	return (ctx->server_active[server_idx]);
}

/* Background context saving */
int
mcp_async_save_context_bg(struct mcp_async_context *ctx,
    const char *session_name, const char *context_data,
    mcp_async_callback callback, void *callback_data)
{
	struct mcp_async_request *req;
	char *arguments;

	if (ctx == NULL || session_name == NULL || context_data == NULL)
		return (-1);

	/* Build arguments for enhanced-memory create_entities call */
	if (asprintf(&arguments,
	    "{\"entities\":[{\"name\":\"session-%s\",\"entityType\":\"session_context\",\"observations\":[\"%s\"]}]}",
	    session_name, context_data) < 0)
		return (-1);

	/* Queue with normal priority */
	req = mcp_async_call_tool(ctx, "enhanced-memory", "create_entities",
	    arguments, MCP_PRIORITY_NORMAL, MCP_ASYNC_DEFAULT_TIMEOUT,
	    callback, callback_data);

	free(arguments);

	return (req != NULL) ? 0 : -1;
}

/* Event loop */
void
mcp_async_event_loop(struct mcp_async_context *ctx)
{
	if (ctx == NULL || ctx->event_base == NULL)
		return;

	event_base_dispatch(ctx->event_base);
}

/* Stop event loop */
void
mcp_async_stop_loop(struct mcp_async_context *ctx)
{
	if (ctx == NULL || ctx->event_base == NULL)
		return;

	event_base_loopbreak(ctx->event_base);
}

/* Parallel operations - submit multiple requests */
int
mcp_async_call_parallel(struct mcp_async_context *ctx,
    struct mcp_async_request **requests, int num_requests)
{
	int i, submitted = 0;

	if (ctx == NULL || requests == NULL || num_requests <= 0)
		return (-1);

	/* Queue all requests */
	for (i = 0; i < num_requests; i++) {
		if (requests[i] == NULL)
			continue;

		mcp_async_queue_request(ctx, requests[i]);
		submitted++;
	}

	/* Process queue to start as many as possible */
	mcp_async_process_queue(ctx);

	return (submitted);
}

/* Wait for all requests to complete */
void
mcp_async_wait_all(struct mcp_async_context *ctx,
    struct mcp_async_request **requests, int num_requests)
{
	int i, all_done;
	struct timeval tv;

	if (ctx == NULL || requests == NULL || num_requests <= 0)
		return;

	/* Poll until all requests complete */
	while (1) {
		all_done = 1;

		for (i = 0; i < num_requests; i++) {
			if (requests[i] == NULL)
				continue;

			if (requests[i]->state != MCP_ASYNC_COMPLETED &&
			    requests[i]->state != MCP_ASYNC_FAILED &&
			    requests[i]->state != MCP_ASYNC_TIMEOUT &&
			    requests[i]->state != MCP_ASYNC_CANCELLED) {
				all_done = 0;
				break;
			}
		}

		if (all_done)
			break;

		/* Run event loop for a short time */
		tv.tv_sec = 0;
		tv.tv_usec = 100000;  /* 100ms */
		event_base_loopexit(ctx->event_base, &tv);
		event_base_dispatch(ctx->event_base);

		/* Try processing queue again */
		mcp_async_process_queue(ctx);
	}
}

/* Find request by ID */
struct mcp_async_request *
mcp_async_find_request(struct mcp_async_context *ctx, int request_id)
{
	struct mcp_async_request *req;

	if (ctx == NULL)
		return (NULL);

	/* Search all queues */
	TAILQ_FOREACH(req, &ctx->urgent_queue, entry) {
		if (req->id == request_id)
			return (req);
	}
	TAILQ_FOREACH(req, &ctx->high_queue, entry) {
		if (req->id == request_id)
			return (req);
	}
	TAILQ_FOREACH(req, &ctx->normal_queue, entry) {
		if (req->id == request_id)
			return (req);
	}
	TAILQ_FOREACH(req, &ctx->low_queue, entry) {
		if (req->id == request_id)
			return (req);
	}
	TAILQ_FOREACH(req, &ctx->active_queue, entry) {
		if (req->id == request_id)
			return (req);
	}

	return (NULL);
}
