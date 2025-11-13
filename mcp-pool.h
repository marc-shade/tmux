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

#ifndef MCP_POOL_H
#define MCP_POOL_H

/*
 * MCP connection pooling for tmux.
 * Maintains reusable connections to MCP servers to reduce overhead
 * of repeated connect/disconnect cycles.
 */

#define MCP_POOL_DEFAULT_SIZE 5		/* Default max connections per server */
#define MCP_POOL_IDLE_TIMEOUT 300	/* 5 minutes in seconds */

/* Pool entry state */
enum mcp_pool_state {
	MCP_POOL_FREE,		/* Available for use */
	MCP_POOL_ACTIVE,	/* Currently in use */
	MCP_POOL_IDLE		/* Connected but unused */
};

/* Pooled connection entry */
struct mcp_pool_entry {
	struct mcp_connection	*conn;		/* Actual MCP connection */
	enum mcp_pool_state	 state;		/* Current state */
	time_t			 last_used;	/* Last activity timestamp */
	u_int			 ref_count;	/* Reference count */
	struct mcp_pool_entry	*next;		/* Next in linked list */
};

/* Connection pool for a single MCP server */
struct mcp_server_pool {
	char			*server_name;	/* Server identifier */
	struct mcp_pool_entry	*entries;	/* Linked list of entries */
	u_int			 size;		/* Current pool size */
	u_int			 max_size;	/* Maximum pool size */
	u_int			 active;	/* Active connections */
	u_int			 idle;		/* Idle connections */

	/* Statistics */
	u_int			 hits;		/* Pool hits */
	u_int			 misses;	/* Pool misses */
	u_int			 evictions;	/* Idle evictions */
	u_int			 creates;	/* New connections created */
	u_int			 destroys;	/* Connections destroyed */
};

/* Global connection pool manager */
struct mcp_pool {
	struct mcp_server_pool	**server_pools;	/* Array of server pools */
	u_int			 num_pools;	/* Number of server pools */
	u_int			 max_pools;	/* Maximum server pools */
	u_int			 default_max_size; /* Default pool size */
	struct mcp_client	*client;	/* Associated MCP client */
};

/* Pool statistics */
struct mcp_pool_stats {
	const char	*server_name;
	u_int		 total_connections;
	u_int		 active_connections;
	u_int		 idle_connections;
	u_int		 hits;
	u_int		 misses;
	u_int		 evictions;
	double		 hit_rate;	/* hits / (hits + misses) */
};

/* Function prototypes */

/* Pool lifecycle */
struct mcp_pool		*mcp_pool_create(struct mcp_client *, u_int);
void			 mcp_pool_destroy(struct mcp_pool *);

/* Connection management */
struct mcp_connection	*mcp_pool_acquire(struct mcp_pool *, const char *);
void			 mcp_pool_release(struct mcp_pool *,
				    struct mcp_connection *);

/* Maintenance */
void			 mcp_pool_cleanup(struct mcp_pool *);
void			 mcp_pool_cleanup_server(struct mcp_pool *,
				    const char *);

/* Statistics and monitoring */
struct mcp_pool_stats	*mcp_pool_stats(struct mcp_pool *, const char *);
void			 mcp_pool_stats_free(struct mcp_pool_stats *);
u_int			 mcp_pool_total_connections(struct mcp_pool *);

/* Internal helpers */
struct mcp_server_pool	*mcp_pool_find_server(struct mcp_pool *, const char *);
struct mcp_server_pool	*mcp_pool_create_server(struct mcp_pool *,
				    const char *);
void			 mcp_pool_remove_entry(struct mcp_server_pool *,
				    struct mcp_pool_entry *);

#endif /* MCP_POOL_H */
