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
#include "mcp-client.h"
#include "mcp-pool.h"

/* Create a new connection pool */
struct mcp_pool *
mcp_pool_create(struct mcp_client *client, u_int default_max_size)
{
	struct mcp_pool	*pool;

	if (client == NULL)
		return (NULL);

	pool = xcalloc(1, sizeof *pool);
	pool->client = client;
	pool->default_max_size = (default_max_size > 0) ? default_max_size :
	    MCP_POOL_DEFAULT_SIZE;
	pool->max_pools = MCP_MAX_SERVERS;
	pool->server_pools = xcalloc(pool->max_pools,
	    sizeof *pool->server_pools);

	return (pool);
}

/* Destroy the connection pool and all pooled connections */
void
mcp_pool_destroy(struct mcp_pool *pool)
{
	struct mcp_server_pool	*server_pool;
	struct mcp_pool_entry	*entry, *next;
	u_int			 i;

	if (pool == NULL)
		return;

	/* Destroy all server pools */
	for (i = 0; i < pool->num_pools; i++) {
		server_pool = pool->server_pools[i];
		if (server_pool == NULL)
			continue;

		/* Destroy all entries in this server pool */
		entry = server_pool->entries;
		while (entry != NULL) {
			next = entry->next;

			/* Disconnect and free the connection */
			if (entry->conn != NULL) {
				mcp_disconnect_server(pool->client,
				    server_pool->server_name);
				entry->conn = NULL;
			}

			free(entry);
			entry = next;
		}

		free(server_pool->server_name);
		free(server_pool);
	}

	free(pool->server_pools);
	free(pool);
}

/* Find server pool by name */
struct mcp_server_pool *
mcp_pool_find_server(struct mcp_pool *pool, const char *server_name)
{
	struct mcp_server_pool	*server_pool;
	u_int			 i;

	for (i = 0; i < pool->num_pools; i++) {
		server_pool = pool->server_pools[i];
		if (server_pool != NULL &&
		    strcmp(server_pool->server_name, server_name) == 0)
			return (server_pool);
	}

	return (NULL);
}

/* Create a new server pool */
struct mcp_server_pool *
mcp_pool_create_server(struct mcp_pool *pool, const char *server_name)
{
	struct mcp_server_pool	*server_pool;

	if (pool->num_pools >= pool->max_pools)
		return (NULL);

	server_pool = xcalloc(1, sizeof *server_pool);
	server_pool->server_name = xstrdup(server_name);
	server_pool->max_size = pool->default_max_size;
	server_pool->entries = NULL;

	pool->server_pools[pool->num_pools++] = server_pool;

	return (server_pool);
}

/* Remove an entry from the server pool */
void
mcp_pool_remove_entry(struct mcp_server_pool *server_pool,
    struct mcp_pool_entry *target)
{
	struct mcp_pool_entry	*entry, *prev;

	prev = NULL;
	entry = server_pool->entries;

	while (entry != NULL) {
		if (entry == target) {
			if (prev == NULL)
				server_pool->entries = entry->next;
			else
				prev->next = entry->next;

			/* Update counters */
			if (entry->state == MCP_POOL_ACTIVE)
				server_pool->active--;
			else if (entry->state == MCP_POOL_IDLE)
				server_pool->idle--;

			server_pool->size--;
			server_pool->destroys++;

			free(entry);
			return;
		}

		prev = entry;
		entry = entry->next;
	}
}

/* Acquire a connection from the pool */
struct mcp_connection *
mcp_pool_acquire(struct mcp_pool *pool, const char *server_name)
{
	struct mcp_server_pool	*server_pool;
	struct mcp_pool_entry	*entry;
	struct mcp_connection	*conn;
	time_t			 now;

	if (pool == NULL || server_name == NULL)
		return (NULL);

	now = time(NULL);

	/* Find or create server pool */
	server_pool = mcp_pool_find_server(pool, server_name);
	if (server_pool == NULL) {
		server_pool = mcp_pool_create_server(pool, server_name);
		if (server_pool == NULL)
			return (NULL);
	}

	/* Look for an idle connection */
	entry = server_pool->entries;
	while (entry != NULL) {
		if (entry->state == MCP_POOL_IDLE) {
			/* Check if connection is still healthy */
			if (mcp_connection_healthy(entry->conn)) {
				/* Reuse this connection */
				entry->state = MCP_POOL_ACTIVE;
				entry->ref_count++;
				entry->last_used = now;
				server_pool->idle--;
				server_pool->active++;
				server_pool->hits++;

				return (entry->conn);
			} else {
				/* Connection is dead, remove it */
				mcp_pool_remove_entry(server_pool, entry);
				entry = server_pool->entries;
				continue;
			}
		}
		entry = entry->next;
	}

	/* No idle connection found - create new one if under limit */
	server_pool->misses++;

	if (server_pool->size >= server_pool->max_size)
		return (NULL);

	/* Create new connection */
	if (mcp_connect_server(pool->client, server_name) != 0)
		return (NULL);

	conn = mcp_find_connection(pool->client, server_name);
	if (conn == NULL)
		return (NULL);

	/* Add to pool */
	entry = xcalloc(1, sizeof *entry);
	entry->conn = conn;
	entry->state = MCP_POOL_ACTIVE;
	entry->ref_count = 1;
	entry->last_used = now;
	entry->next = server_pool->entries;
	server_pool->entries = entry;

	server_pool->size++;
	server_pool->active++;
	server_pool->creates++;

	return (conn);
}

/* Release a connection back to the pool */
void
mcp_pool_release(struct mcp_pool *pool, struct mcp_connection *conn)
{
	struct mcp_server_pool	*server_pool;
	struct mcp_pool_entry	*entry;
	const char		*server_name;
	u_int			 i;

	if (pool == NULL || conn == NULL || conn->config == NULL)
		return;

	server_name = conn->config->name;

	/* Find the server pool */
	server_pool = mcp_pool_find_server(pool, server_name);
	if (server_pool == NULL)
		return;

	/* Find the entry for this connection */
	entry = server_pool->entries;
	while (entry != NULL) {
		if (entry->conn == conn) {
			if (entry->ref_count > 0)
				entry->ref_count--;

			if (entry->ref_count == 0) {
				/* No more references - mark as idle */
				entry->state = MCP_POOL_IDLE;
				entry->last_used = time(NULL);
				server_pool->active--;
				server_pool->idle++;
			}

			return;
		}
		entry = entry->next;
	}
}

/* Clean up idle connections across all server pools */
void
mcp_pool_cleanup(struct mcp_pool *pool)
{
	u_int	i;

	if (pool == NULL)
		return;

	for (i = 0; i < pool->num_pools; i++) {
		if (pool->server_pools[i] != NULL)
			mcp_pool_cleanup_server(pool,
			    pool->server_pools[i]->server_name);
	}
}

/* Clean up idle connections for a specific server */
void
mcp_pool_cleanup_server(struct mcp_pool *pool, const char *server_name)
{
	struct mcp_server_pool	*server_pool;
	struct mcp_pool_entry	*entry, *next, *prev;
	time_t			 now, idle_time;

	if (pool == NULL || server_name == NULL)
		return;

	server_pool = mcp_pool_find_server(pool, server_name);
	if (server_pool == NULL)
		return;

	now = time(NULL);
	prev = NULL;
	entry = server_pool->entries;

	while (entry != NULL) {
		next = entry->next;

		if (entry->state == MCP_POOL_IDLE) {
			idle_time = now - entry->last_used;

			if (idle_time >= MCP_POOL_IDLE_TIMEOUT) {
				/* Disconnect and remove this entry */
				if (entry->conn != NULL) {
					mcp_disconnect_server(pool->client,
					    server_name);
					entry->conn = NULL;
				}

				/* Remove from list */
				if (prev == NULL)
					server_pool->entries = next;
				else
					prev->next = next;

				server_pool->idle--;
				server_pool->size--;
				server_pool->evictions++;
				server_pool->destroys++;

				free(entry);
				entry = next;
				continue;
			}
		}

		prev = entry;
		entry = next;
	}
}

/* Get pool statistics for a specific server */
struct mcp_pool_stats *
mcp_pool_stats(struct mcp_pool *pool, const char *server_name)
{
	struct mcp_server_pool	*server_pool;
	struct mcp_pool_stats	*stats;
	u_int			 total_requests;

	if (pool == NULL || server_name == NULL)
		return (NULL);

	server_pool = mcp_pool_find_server(pool, server_name);
	if (server_pool == NULL)
		return (NULL);

	stats = xcalloc(1, sizeof *stats);
	stats->server_name = server_pool->server_name;
	stats->total_connections = server_pool->size;
	stats->active_connections = server_pool->active;
	stats->idle_connections = server_pool->idle;
	stats->hits = server_pool->hits;
	stats->misses = server_pool->misses;
	stats->evictions = server_pool->evictions;

	/* Calculate hit rate */
	total_requests = server_pool->hits + server_pool->misses;
	if (total_requests > 0)
		stats->hit_rate = (double)server_pool->hits / total_requests;
	else
		stats->hit_rate = 0.0;

	return (stats);
}

/* Free pool statistics */
void
mcp_pool_stats_free(struct mcp_pool_stats *stats)
{
	free(stats);
}

/* Get total number of connections across all pools */
u_int
mcp_pool_total_connections(struct mcp_pool *pool)
{
	u_int	total, i;

	if (pool == NULL)
		return (0);

	total = 0;
	for (i = 0; i < pool->num_pools; i++) {
		if (pool->server_pools[i] != NULL)
			total += pool->server_pools[i]->size;
	}

	return (total);
}
