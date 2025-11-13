# Phase 4.3: Multi-Session Coordination - Architecture Revision

**Date**: 2025-11-13
**Status**: Architecture Design
**Phase**: 4.3 Multi-Session Coordination

## Problem Discovered

During Phase 4.3 implementation, discovered that tmux already has a `session_group` structure defined in tmux.h:1406 for managing linked windows across sessions. This creates a namespace conflict with our proposed agentic session grouping.

**Existing tmux session_group:**
- Purpose: Window-level session grouping (linked windows)
- Structure: RB_HEAD-based tree in tmux.h:1412
- Usage: session.c, server.c for window synchronization
- Not suitable for agentic multi-session coordination

## Revised Approach

Rather than creating a conflicting structure, extend the existing **session-agent** system with coordination capabilities.

### Design Principles

1. **No Namespace Conflicts**: Use existing agent metadata structure
2. **Backward Compatible**: Phase 4.0-4.2 features remain unchanged
3. **Leverage MCP**: Use agent-runtime-mcp for coordination
4. **Minimal Surface Area**: Add only coordination-specific fields

### Architecture

#### Option A: Extend `struct session_agent_metadata` (Recommended)

Add coordination fields to existing session-agent.h structure:

```c
struct session_agent_metadata {
	/* Existing fields (Phase 2.1-4.2) */
	enum agent_type type;
	char *objective;
	time_t created_at;
	time_t last_activity;
	int tasks_completed;
	int interactions;
	char *runtime_goal_id;
	int context_saved;

	/* NEW: Phase 4.3 - Multi-Session Coordination */
	char *coordination_group;      /* Optional group name */
	char **peer_sessions;           /* Array of peer session names */
	int num_peers;                  /* Number of peers */
	int max_peers;                  /* Max peers (32) */
	char *shared_context;           /* JSON shared state */
	size_t shared_context_len;
	int is_coordinator;             /* Leader flag */
};
```

**Advantages:**
- No namespace conflicts
- Backward compatible (coordination_group NULL = standalone)
- Reuses existing agent metadata infrastructure
- Simpler implementation (session-agent.c already exists)

**Disadvantages:**
- Mixes standalone and coordinated agent concerns
- Larger struct even for standalone sessions

#### Option B: Separate Agent Coordination Layer

Create `agent-coordination.c/h` for multi-session features:

```c
struct agent_coordination {
	char *group_name;
	struct session **member_sessions;
	int num_members;
	int max_members;
	char *shared_goal;
	char *shared_context;
	time_t created_at;
	time_t last_sync;
	TAILQ_ENTRY(agent_coordination) entry;
};

TAILQ_HEAD(agent_coordination_list, agent_coordination);
extern struct agent_coordination_list agent_coordinations;
```

**Advantages:**
- Clean separation of concerns
- No struct bloat for standalone sessions
- Independent evolution of coordination features

**Disadvantages:**
- More complex (two parallel systems)
- Potential for coordination/session state drift
- More code to maintain

### Recommendation: Option A (Extend Existing)

Extend `session_agent_metadata` with coordination fields for these reasons:

1. **Simplicity**: Single source of truth for agent state
2. **Integration**: Coordination is part of agent lifecycle, not separate
3. **Performance**: No extra lookups/joins between systems
4. **Evolution**: Easier to add features incrementally

### Implementation Plan

#### Phase 4.3A: Agent Coordination Fields (1 week, ~400 lines)
- Extend `session_agent_metadata` in session-agent.h
- Add coordination functions to session-agent.c:
  - `agent_join_coordination(session, group_name)`
  - `agent_leave_coordination(session)`
  - `agent_share_context(session, key, value)`
  - `agent_get_shared_context(session, key)`
  - `agent_list_peers(session)`
- Update session-mcp-integration.c for shared context

#### Phase 4.3B: Coordination Commands (1 week, ~600 lines)
- `cmd-agent-join-group.c` - Join coordination group
- `cmd-agent-leave-group.c` - Leave group
- `cmd-agent-share.c` - Share context with group
- `cmd-agent-peers.c` - List peer agents
- `cmd-list-agent-groups.c` - List all groups

#### Phase 4.3C: Testing & Documentation (1 week, ~500 lines)
- Multi-session coordination test suite
- Update AGENTIC_FEATURES.md
- Add coordination examples
- Performance testing with 10+ coordinated sessions

**Total**: 3 weeks, ~1,500 lines (vs original 2-3 weeks, ~1,400 lines)

### MCP Integration Strategy

Use **agent-runtime-mcp** for coordination persistence:

1. **Group Registration**: `create_goal` with group name
2. **Peer Discovery**: Goal metadata stores member sessions
3. **Context Sharing**: Goal metadata stores shared context JSON
4. **Progress Tracking**: Aggregate tasks across group members

**Enhanced-memory** for learning patterns:
- Successful coordination patterns
- Inter-agent communication logs
- Group performance metrics

### Commands Design

#### Standalone Session (Existing)
```bash
tmux new-session -G research -o "Study OAuth2" -s oauth-research
tmux show-agent -t oauth-research
```

#### Coordinated Sessions (NEW - Phase 4.3)
```bash
# Create research group
tmux agent-join-group -t session1 research-team

# Add more agents to group
tmux agent-join-group -t session2 research-team
tmux agent-join-group -t session3 research-team

# Share findings
tmux agent-share -t session1 research-team "found_pattern=X"

# List peers
tmux agent-peers -t session1

# View all groups
tmux list-agent-groups
```

### Migration from Phase 4.3 Original Design

Original design had separate `session_group` structure. New design absorbs those features into agent metadata:

| Original Concept | New Implementation |
|------------------|-------------------|
| `session_group` struct | `coordination_group` field in agent metadata |
| `group->sessions[]` | `peer_sessions[]` in each agent |
| `group->shared_data` | `shared_context` in agent metadata |
| `group->state` | Derived from coordinator's state |
| `group->tasks_*` | Aggregated from peer tasks |

### Backward Compatibility

**Phase 2.1-4.2 sessions continue to work unchanged:**
- `coordination_group == NULL` → standalone agent
- `num_peers == 0` → no coordination
- All existing commands work identically

**Upgrade path:**
```c
if (s->agent_metadata->coordination_group != NULL) {
	/* Use coordination features */
} else {
	/* Standalone agent (existing behavior) */
}
```

## Next Steps

1. User approval of revised architecture
2. Implement Phase 4.3A (extend agent metadata)
3. Implement Phase 4.3B (commands)
4. Implement Phase 4.3C (tests & docs)

## Questions for Review

1. **Naming**: Is `coordination_group` clear? Alternatives: `agent_group`, `team_name`
2. **Scope**: Should we support hierarchical groups (groups of groups)?
3. **Limits**: 32 peers max reasonable? Or increase to 64?
4. **Persistence**: Store coordination state in enhanced-memory or agent-runtime only?

---

**Status**: Awaiting user feedback on revised architecture before proceeding with implementation.
