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
#include "session-agent.h"
#include "session-mcp-integration.h"
#include "agent-analytics.h"

/* Create a new session agent */
struct session_agent *
session_agent_create(const char *agent_type, const char *goal,
    const char *session_name)
{
	struct session_agent	*agent;

	agent = xmalloc(sizeof *agent);
	memset(agent, 0, sizeof *agent);

	agent->agent_type = xstrdup(agent_type != NULL ? agent_type :
	    AGENT_TYPE_NONE);
	agent->goal = xstrdup(goal != NULL ? goal : "");
	agent->session_name = xstrdup(session_name != NULL ? session_name : "");

	agent->runtime_goal_id = NULL;
	agent->runtime_session_id = NULL;
	agent->context_key = NULL;
	agent->context_saved = 0;

	agent->created = time(NULL);
	agent->last_activity = time(NULL);

	agent->tasks_completed = 0;
	agent->interactions = 0;

	/* Phase 4.3: Initialize coordination fields */
	agent->coordination_group = NULL;
	agent->peer_sessions = NULL;
	agent->num_peers = 0;
	agent->max_peers = 32;		/* Default max peers */
	agent->shared_context = NULL;
	agent->shared_context_len = 0;
	agent->is_coordinator = 0;
	agent->last_coordination = 0;

	/* Phase 4.4: Record session start for analytics */
	agent_analytics_record_session_start(agent->agent_type);

	return (agent);
}

/* Destroy session agent */
void
session_agent_destroy(struct session_agent *agent)
{
	if (agent == NULL)
		return;

	/* Complete goal in agent-runtime-mcp before cleanup */
	if (global_mcp_client != NULL && agent->runtime_goal_id != NULL)
		session_mcp_complete_goal(agent);

	/* Phase 4.4: Record session end for analytics (success = goal completed) */
	agent_analytics_record_session_end(agent->agent_type,
	    agent->runtime_goal_id != NULL ? 1 : 0);

	free(agent->agent_type);
	free(agent->goal);
	free(agent->session_name);

	if (agent->runtime_goal_id != NULL)
		free(agent->runtime_goal_id);
	if (agent->runtime_session_id != NULL)
		free(agent->runtime_session_id);
	if (agent->context_key != NULL)
		free(agent->context_key);

	/* Phase 4.3: Free coordination fields */
	if (agent->coordination_group != NULL)
		free(agent->coordination_group);
	if (agent->peer_sessions != NULL) {
		int i;
		for (i = 0; i < agent->num_peers; i++)
			free(agent->peer_sessions[i]);
		free(agent->peer_sessions);
	}
	if (agent->shared_context != NULL)
		free(agent->shared_context);

	free(agent);
}

/* Register agent with agent-runtime-mcp */
int
session_agent_register(struct session_agent *agent)
{
	struct mcp_response	*resp;
	char			*params;
	size_t			 params_len;

	if (agent == NULL || global_mcp_client == NULL)
		return (-1);

	/* Skip if agent type is "none" */
	if (strcmp(agent->agent_type, AGENT_TYPE_NONE) == 0)
		return (0);

	/* Skip if already registered */
	if (agent->runtime_goal_id != NULL)
		return (0);

	/*
	 * Call agent-runtime-mcp create_goal
	 * Params: {"name": "session_name", "description": "goal"}
	 */
	params_len = 256 + strlen(agent->session_name) + strlen(agent->goal);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"name\":\"%s\",\"description\":\"[%s] %s\"}",
	    agent->session_name, agent->agent_type, agent->goal);

	resp = mcp_call_tool(global_mcp_client, "agent-runtime-mcp",
	    "create_goal", params);
	free(params);

	if (resp == NULL)
		return (-1);

	/* Extract goal ID from response if successful */
	if (resp->success && resp->result != NULL) {
		/*
		 * Parse JSON to extract goal_id
		 * For now, store the full result as the ID
		 * TODO: Proper JSON parsing
		 */
		agent->runtime_goal_id = xstrdup(resp->result);
	}

	mcp_response_free(resp);

	return (agent->runtime_goal_id != NULL ? 0 : -1);
}

/* Update agent status */
int
session_agent_update_status(struct session_agent *agent, const char *status)
{
	if (agent == NULL || status == NULL)
		return (-1);

	agent->last_activity = time(NULL);
	agent->interactions++;

	/* TODO: Send update to agent-runtime-mcp */

	return (0);
}

/* Complete agent goal */
int
session_agent_complete(struct session_agent *agent)
{
	struct mcp_response	*resp;
	char			*params;
	size_t			 params_len;

	if (agent == NULL || global_mcp_client == NULL)
		return (-1);

	/* Skip if not registered */
	if (agent->runtime_goal_id == NULL)
		return (0);

	/*
	 * Call agent-runtime-mcp update_task_status
	 * Mark goal as completed
	 * TODO: Extract actual goal ID and call proper API
	 */
	params_len = 256;
	params = xmalloc(params_len);
	snprintf(params, params_len, "{\"status\":\"completed\"}");

	resp = mcp_call_tool(global_mcp_client, "agent-runtime-mcp",
	    "update_task_status", params);
	free(params);

	if (resp != NULL)
		mcp_response_free(resp);

	return (0);
}

/* Save agent context to enhanced-memory */
int
session_agent_save_context(struct session_agent *agent, const char *context)
{
	struct mcp_response	*resp;
	char			*params;
	size_t			 params_len;

	if (agent == NULL || context == NULL || global_mcp_client == NULL)
		return (-1);

	/* Build context key if not set */
	if (agent->context_key == NULL) {
		char key[256];
		snprintf(key, sizeof key, "session-%s-%ld",
		    agent->session_name, agent->created);
		agent->context_key = xstrdup(key);
	}

	/*
	 * Call enhanced-memory create_entities
	 * Store context as an entity
	 */
	params_len = 1024 + strlen(agent->context_key) + strlen(context);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "[{\"name\":\"%s\",\"entityType\":\"session_context\","
	    "\"observations\":[\"%s\"]}]",
	    agent->context_key, context);

	resp = mcp_call_tool(global_mcp_client, "enhanced-memory",
	    "create_entities", params);
	free(params);

	if (resp != NULL) {
		if (resp->success)
			agent->context_saved = 1;
		mcp_response_free(resp);
	}

	return (agent->context_saved ? 0 : -1);
}

/* Restore agent context from enhanced-memory */
int
session_agent_restore_context(struct session_agent *agent)
{
	struct mcp_response	*resp;
	char			*params;
	size_t			 params_len;

	if (agent == NULL || agent->context_key == NULL ||
	    global_mcp_client == NULL)
		return (-1);

	/*
	 * Call enhanced-memory search_nodes
	 * Search for context by key
	 */
	params_len = 256 + strlen(agent->context_key);
	params = xmalloc(params_len);
	snprintf(params, params_len, "{\"query\":\"%s\",\"limit\":1}",
	    agent->context_key);

	resp = mcp_call_tool(global_mcp_client, "enhanced-memory",
	    "search_nodes", params);
	free(params);

	if (resp != NULL) {
		/* TODO: Parse and restore context from response */
		mcp_response_free(resp);
	}

	return (0);
}

/* Get agent type */
const char *
session_agent_get_type(struct session_agent *agent)
{
	return (agent != NULL ? agent->agent_type : AGENT_TYPE_NONE);
}

/* Get agent goal */
const char *
session_agent_get_goal(struct session_agent *agent)
{
	return (agent != NULL ? agent->goal : "");
}

/* Get agent runtime ID */
const char *
session_agent_get_runtime_id(struct session_agent *agent)
{
	return (agent != NULL ? agent->runtime_goal_id : NULL);
}

/* ========================================================================
 * Phase 4.3: Multi-Session Coordination Functions
 * ======================================================================== */

/* Join a coordination group */
int
session_agent_join_group(struct session_agent *agent, const char *group_name)
{
	if (agent == NULL || group_name == NULL || strlen(group_name) == 0)
		return (-1);

	/* Leave existing group if already in one */
	if (agent->coordination_group != NULL)
		session_agent_leave_group(agent);

	/* Set group name */
	agent->coordination_group = xstrdup(group_name);

	/* Allocate peer sessions array */
	agent->peer_sessions = xcalloc(agent->max_peers,
	    sizeof(char *));
	agent->num_peers = 0;

	/* First session in group becomes coordinator */
	agent->is_coordinator = 1;
	agent->last_coordination = time(NULL);

	/* TODO: Register group with agent-runtime-mcp as a shared goal */

	return (0);
}

/* Leave coordination group */
int
session_agent_leave_group(struct session_agent *agent)
{
	int i;

	if (agent == NULL || agent->coordination_group == NULL)
		return (-1);

	/* Free group name */
	free(agent->coordination_group);
	agent->coordination_group = NULL;

	/* Free peer sessions */
	if (agent->peer_sessions != NULL) {
		for (i = 0; i < agent->num_peers; i++)
			free(agent->peer_sessions[i]);
		free(agent->peer_sessions);
		agent->peer_sessions = NULL;
	}
	agent->num_peers = 0;

	/* Free shared context */
	if (agent->shared_context != NULL) {
		free(agent->shared_context);
		agent->shared_context = NULL;
		agent->shared_context_len = 0;
	}

	agent->is_coordinator = 0;
	agent->last_coordination = 0;

	/* TODO: Unregister from group in agent-runtime-mcp */

	return (0);
}

/* Add peer to coordination group */
int
session_agent_add_peer(struct session_agent *agent, const char *peer_name)
{
	int i;

	if (agent == NULL || peer_name == NULL ||
	    agent->coordination_group == NULL)
		return (-1);

	/* Check if already a peer */
	for (i = 0; i < agent->num_peers; i++) {
		if (strcmp(agent->peer_sessions[i], peer_name) == 0)
			return (0);	/* Already exists */
	}

	/* Check if group is full */
	if (agent->num_peers >= agent->max_peers)
		return (-1);

	/* Add peer */
	agent->peer_sessions[agent->num_peers++] = xstrdup(peer_name);
	agent->last_coordination = time(NULL);

	return (0);
}

/* Remove peer from coordination group */
int
session_agent_remove_peer(struct session_agent *agent, const char *peer_name)
{
	int i, found = -1;

	if (agent == NULL || peer_name == NULL ||
	    agent->coordination_group == NULL)
		return (-1);

	/* Find peer */
	for (i = 0; i < agent->num_peers; i++) {
		if (strcmp(agent->peer_sessions[i], peer_name) == 0) {
			found = i;
			break;
		}
	}

	if (found < 0)
		return (-1);

	/* Free peer name */
	free(agent->peer_sessions[found]);

	/* Shift remaining peers */
	for (i = found; i < agent->num_peers - 1; i++)
		agent->peer_sessions[i] = agent->peer_sessions[i + 1];

	agent->num_peers--;
	agent->last_coordination = time(NULL);

	return (0);
}

/* Share context with coordination group */
int
session_agent_share_context(struct session_agent *agent, const char *key,
    const char *value)
{
	char	*new_context;

	if (agent == NULL || key == NULL || value == NULL ||
	    agent->coordination_group == NULL)
		return (-1);

	/*
	 * Simple key-value storage using newline-separated format:
	 * key=value\n
	 * In production, would use proper JSON library
	 */
	if (agent->shared_context == NULL) {
		/* First entry */
		if (asprintf(&new_context, "%s=%s\n", key, value) < 0)
			return (-1);
	} else {
		/* Append to existing context */
		if (asprintf(&new_context, "%s%s=%s\n",
		    agent->shared_context, key, value) < 0)
			return (-1);
	}

	/* Replace context */
	free(agent->shared_context);
	agent->shared_context = new_context;
	agent->shared_context_len = strlen(new_context);
	agent->last_coordination = time(NULL);

	/* TODO: Sync to group via agent-runtime-mcp goal metadata */

	return (0);
}

/* Get shared context value */
const char *
session_agent_get_shared_context(struct session_agent *agent, const char *key)
{
	static char	 value[1024];
	char		*data, *line, *eq;
	size_t		 keylen;

	if (agent == NULL || key == NULL ||
	    agent->shared_context == NULL)
		return (NULL);

	keylen = strlen(key);
	data = xstrdup(agent->shared_context);

	/* Simple linear search for key=value\n */
	line = strtok(data, "\n");
	while (line != NULL) {
		if (strncmp(line, key, keylen) == 0 && line[keylen] == '=') {
			eq = strchr(line, '=');
			if (eq != NULL) {
				strlcpy(value, eq + 1, sizeof value);
				free(data);
				return (value);
			}
		}
		line = strtok(NULL, "\n");
	}

	free(data);
	return (NULL);
}

/* Synchronize with coordination group */
int
session_agent_sync_group(struct session_agent *agent)
{
	if (agent == NULL || agent->coordination_group == NULL)
		return (-1);

	agent->last_coordination = time(NULL);

	/*
	 * TODO: Implement full group synchronization:
	 * 1. Query agent-runtime-mcp for group goal metadata
	 * 2. Update shared_context from goal metadata
	 * 3. Update peer_sessions from goal member list
	 * 4. Aggregate progress from all peers
	 */

	return (0);
}

/* Check if agent is coordinated */
int
session_agent_is_coordinated(struct session_agent *agent)
{
	return (agent != NULL && agent->coordination_group != NULL);
}

/* Check if agent is group coordinator */
int
session_agent_is_coordinator(struct session_agent *agent)
{
	return (agent != NULL && agent->is_coordinator);
}

/* List peer sessions */
const char **
session_agent_list_peers(struct session_agent *agent, int *count)
{
	if (agent == NULL || count == NULL ||
	    agent->coordination_group == NULL) {
		if (count != NULL)
			*count = 0;
		return (NULL);
	}

	*count = agent->num_peers;
	return ((const char **)agent->peer_sessions);
}
