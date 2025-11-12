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

	return (agent);
}

/* Destroy session agent */
void
session_agent_destroy(struct session_agent *agent)
{
	if (agent == NULL)
		return;

	free(agent->agent_type);
	free(agent->goal);
	free(agent->session_name);

	if (agent->runtime_goal_id != NULL)
		free(agent->runtime_goal_id);
	if (agent->runtime_session_id != NULL)
		free(agent->runtime_session_id);
	if (agent->context_key != NULL)
		free(agent->context_key);

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
