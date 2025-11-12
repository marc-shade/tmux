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
#include "mcp-protocol.h"
#include "session-agent.h"

/*
 * Session-MCP Integration
 * Automatic integration between sessions and MCP servers
 * (enhanced-memory and agent-runtime-mcp)
 */

/*
 * Save session context to enhanced-memory
 */
int
session_mcp_save_to_memory(struct session_agent *agent, struct session *s)
{
	struct mcp_response	*resp;
	char			*params, *observations;
	size_t			 params_len, obs_len;
	char			 timestamp[64];
	time_t			 now;
	struct tm		*tm_info;

	if (agent == NULL || s == NULL || global_mcp_client == NULL)
		return (-1);

	/* Format timestamp */
	now = time(NULL);
	tm_info = localtime(&now);
	strftime(timestamp, sizeof timestamp, "%Y-%m-%d %H:%M:%S", tm_info);

	/* Build observations array */
	obs_len = 512 + (agent->goal ? strlen(agent->goal) : 0);
	observations = xmalloc(obs_len);
	snprintf(observations, obs_len,
	    "[\"Agent type: %s\","
	    "\"Goal: %s\","
	    "\"Tasks completed: %u\","
	    "\"Interactions: %u\","
	    "\"Session duration: %ld seconds\","
	    "\"Last activity: %s\"]",
	    agent->agent_type ? agent->agent_type : "unknown",
	    agent->goal ? agent->goal : "none",
	    agent->tasks_completed,
	    agent->interactions,
	    (long)(now - agent->created),
	    timestamp);

	/* Build entity creation params */
	params_len = 1024 + strlen(observations);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"entities\":[{"
	    "\"name\":\"session-%s-%ld\","
	    "\"entityType\":\"session_context\","
	    "\"observations\":%s"
	    "}]}",
	    agent->session_name ? agent->session_name : "unknown",
	    (long)agent->created,
	    observations);

	free(observations);

	/* Call enhanced-memory create_entities */
	resp = mcp_call_tool_safe(global_mcp_client, "enhanced-memory",
	    "create_entities", params);
	free(params);

	if (resp == NULL || !resp->success) {
		if (resp != NULL)
			mcp_response_free(resp);
		return (-1);
	}

	mcp_response_free(resp);
	agent->context_saved = 1;

	log_debug("Session context saved to enhanced-memory for %s",
	    agent->session_name);
	return (0);
}

/*
 * Register session goal with agent-runtime-mcp
 */
int
session_mcp_register_goal(struct session_agent *agent)
{
	struct mcp_response	*resp;
	char			*params, *goal_id;
	const char		*p, *id_start;
	size_t			 params_len, id_len;

	if (agent == NULL || agent->goal == NULL || global_mcp_client == NULL)
		return (-1);

	/* Skip if already registered */
	if (agent->runtime_goal_id != NULL)
		return (0);

	/* Build goal creation params */
	params_len = 512 + strlen(agent->goal);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"name\":\"session-%s\","
	    "\"description\":\"%s\"}",
	    agent->session_name ? agent->session_name : "unknown",
	    agent->goal);

	/* Call agent-runtime-mcp create_goal */
	resp = mcp_call_tool_safe(global_mcp_client, "agent-runtime-mcp",
	    "create_goal", params);
	free(params);

	if (resp == NULL || !resp->success) {
		if (resp != NULL)
			mcp_response_free(resp);
		return (-1);
	}

	/* Extract goal_id from response */
	/* Expected format: {"goal_id": 123, ...} */
	if (resp->result != NULL) {
		id_start = strstr(resp->result, "\"goal_id\"");
		if (id_start != NULL) {
			id_start = strchr(id_start, ':');
			if (id_start != NULL) {
				id_start++;
				while (*id_start == ' ')
					id_start++;

				/* Find end of number */
				p = id_start;
				while (*p >= '0' && *p <= '9')
					p++;

				if (p > id_start) {
					id_len = p - id_start;
					goal_id = xmalloc(id_len + 1);
					memcpy(goal_id, id_start, id_len);
					goal_id[id_len] = '\0';
					agent->runtime_goal_id = goal_id;
				}
			}
		}
	}

	mcp_response_free(resp);

	if (agent->runtime_goal_id != NULL) {
		log_debug("Session goal registered with agent-runtime-mcp: %s",
		    agent->runtime_goal_id);
		return (0);
	}

	return (-1);
}

/*
 * Update goal status in agent-runtime-mcp
 */
int
session_mcp_update_goal_status(struct session_agent *agent, const char *status)
{
	struct mcp_response	*resp;
	char			*params;
	size_t			 params_len;

	if (agent == NULL || agent->runtime_goal_id == NULL ||
	    status == NULL || global_mcp_client == NULL)
		return (-1);

	/* Build status update params */
	params_len = 256 + strlen(agent->runtime_goal_id) + strlen(status);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"goal_id\":%s,"
	    "\"status\":\"%s\"}",
	    agent->runtime_goal_id,
	    status);

	/* Call agent-runtime-mcp update_task_status */
	resp = mcp_call_tool_safe(global_mcp_client, "agent-runtime-mcp",
	    "update_task_status", params);
	free(params);

	if (resp == NULL || !resp->success) {
		if (resp != NULL)
			mcp_response_free(resp);
		return (-1);
	}

	mcp_response_free(resp);

	log_debug("Goal status updated to '%s' for %s", status,
	    agent->session_name);
	return (0);
}

/*
 * Complete goal in agent-runtime-mcp
 */
int
session_mcp_complete_goal(struct session_agent *agent)
{
	return session_mcp_update_goal_status(agent, "completed");
}

/*
 * Search enhanced-memory for similar sessions
 */
struct mcp_response *
session_mcp_find_similar(struct session_agent *agent)
{
	struct mcp_response	*resp;
	char			*params;
	size_t			 params_len;

	if (agent == NULL || agent->agent_type == NULL ||
	    global_mcp_client == NULL)
		return (NULL);

	/* Build search query */
	params_len = 256 + strlen(agent->agent_type);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"query\":\"session_context type:%s\","
	    "\"limit\":5}",
	    agent->agent_type);

	/* Call enhanced-memory search_nodes */
	resp = mcp_call_tool_safe(global_mcp_client, "enhanced-memory",
	    "search_nodes", params);
	free(params);

	return (resp);
}

/*
 * Get agent-runtime-mcp tasks for this goal
 */
struct mcp_response *
session_mcp_get_tasks(struct session_agent *agent)
{
	struct mcp_response	*resp;
	char			*params;
	size_t			 params_len;

	if (agent == NULL || agent->runtime_goal_id == NULL ||
	    global_mcp_client == NULL)
		return (NULL);

	/* Build task list params */
	params_len = 256 + strlen(agent->runtime_goal_id);
	params = xmalloc(params_len);
	snprintf(params, params_len,
	    "{\"goal_id\":%s}",
	    agent->runtime_goal_id);

	/* Call agent-runtime-mcp list_tasks */
	resp = mcp_call_tool_safe(global_mcp_client, "agent-runtime-mcp",
	    "list_tasks", params);
	free(params);

	return (resp);
}
