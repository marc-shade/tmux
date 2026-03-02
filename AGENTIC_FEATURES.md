# Tmux Agentic Features

This fork of tmux adds native support for agentic AI workflows through Model Context Protocol (MCP) integration and session-agent mapping.

## Features

### 1. Agent-Aware Sessions

Create tmux sessions with agent metadata that tracks purpose, goals, and progress:

```bash
# Create session with agent type and goal
tmux new-session -G research -o "Study tmux internals" -s my-session

# View agent information
tmux show-agent -t my-session

# Supported agent types:
# - research: Research and investigation
# - development: Software development
# - debugging: Bug fixing and troubleshooting
# - writing: Documentation and content creation
# - testing: Quality assurance and testing
# - analysis: Data analysis and system analysis
# - custom: Custom workflows
```

### 2. Session Agent Metadata

Each agent-aware session tracks:
- **Agent Type**: Purpose classification
- **Goal**: What the session aims to accomplish
- **Created**: Session start time
- **Last Activity**: Most recent interaction
- **Tasks Completed**: Counter for completed work items
- **Interactions**: Number of user interactions
- **Runtime Goal ID**: Integration with agent-runtime-mcp
- **Context Status**: Whether context is saved to enhanced-memory

### 3. MCP Integration

Native MCP client for querying MCP servers directly from tmux:

```bash
# Query an MCP server
tmux mcp-query <server-name> <tool-name> [arguments]

# Example: Get memory status
tmux mcp-query enhanced-memory get_memory_status '{}'

# Example: List goals from agent runtime
tmux mcp-query agent-runtime-mcp list_goals '{}'
```

**Automatic Configuration**: Loads MCP server configurations from `~/.claude.json`

### 4. Session Lifecycle Hooks

Agent metadata is automatically managed through session lifecycle:
- **session_create()**: Initializes agent_metadata to NULL
- **session_destroy()**: Completes agent goal and cleanup
- **session_free()**: Defensive cleanup of remaining metadata

## Installation

### Prerequisites

```bash
# macOS
brew install libevent ncurses utf8proc automake pkg-config

# Debian/Ubuntu
sudo apt-get install libevent-dev ncurses-dev libutf8proc-dev automake pkg-config
```

### Build from Source

```bash
git clone https://github.com/marc-shade/tmux.git
cd tmux

./autogen.sh
./configure --enable-utf8proc
make -j8
sudo make install
```

### Verify Installation

```bash
tmux -V
# Should show: tmux next-3.7

tmux list-commands | grep -E "(agent|mcp|template)"
# Should show 15 agentic commands
```

## Usage Examples

### Example 1: Research Session

```bash
# Start a research session
tmux new-session -G research -o "Investigate memory leaks" -s debug-leak

# Do your work...

# Check agent info
tmux show-agent -t debug-leak

# Output:
# Session: debug-leak
# Agent Type: research
# Goal: Investigate memory leaks
# Created: Wed Nov 12 15:00:00 2025 (30 minutes ago)
# Last Activity: Wed Nov 12 15:25:00 2025 (5 minutes ago)
# Tasks Completed: 3
# Interactions: 12
# Runtime Goal ID: (not registered)
# Context: not saved
```

### Example 2: Development Workflow

```bash
# Create development session
tmux new-session -G development -o "Implement user authentication" -s auth-dev

# Split for multiple panes
tmux split-window -h
tmux split-window -v

# Work across panes...

# Agent metadata persists for the entire session
tmux show-agent
```

### Example 3: Multiple Agent Sessions

```bash
# Research session
tmux new-session -G research -o "Study OAuth flows" -s research-oauth

# Development session
tmux new-session -G development -o "Build OAuth client" -s dev-oauth -d

# Testing session
tmux new-session -G testing -o "Test OAuth integration" -s test-oauth -d

# List all sessions with their purposes
tmux list-sessions
tmux show-agent -t research-oauth
tmux show-agent -t dev-oauth
tmux show-agent -t test-oauth
```

## Architecture

### Components

**MCP Infrastructure** (7 files):
1. **mcp-client.c/h** - Native MCP client with stdio/socket transports, JSON-RPC 2.0
2. **mcp-config.c/h** - C-native JSON parser for ~/.claude.json
3. **mcp-protocol.c** - MCP protocol handshake (initialize/initialized)
4. **mcp-async.c** - Async operations with libevent and priority queuing
5. **mcp-pool.c** - Connection pooling with idle timeout
6. **mcp-metrics.c** - Per-server latency tracking (min/max/avg/p95/p99)
7. **mcp-socket.c** - Unix domain socket transport

**Agent System** (10 files):
1. **session-agent.c/h** - Session-agent lifecycle and multi-session coordination
2. **session-mcp-integration.c/h** - Enhanced-memory, agent-runtime, pane capture pipeline
3. **session-template.c** - Built-in session templates (research, development, simple)
4. **agent-manager.c** - Global agent coordination
5. **agent-analytics.c** - Performance analytics engine
6. **agent-learning.c** - Pattern recognition from session history
7. **agent-optimizer.c** - Workflow optimization strategies
8. **context-semantic.c** / **context-compress.c** - Smart context extraction and compression
9. **mcp-events.c/h** - Event-driven MCP hooks for agent lifecycle
10. **mcp-server.c/h** - tmux as MCP server (JSON-RPC 2.0, 7 tools)

**Commands** (15 files):
1. **cmd-show-agent.c** - Display agent metadata
2. **cmd-mcp-query.c** - Query MCP servers
3. **cmd-mcp-stats.c** - MCP connection statistics
4. **cmd-mcp-serve.c** - Run tmux as MCP server
5. **cmd-agent-analytics.c** - Performance analytics
6. **cmd-agent-optimize.c** - Optimization recommendations
7. **cmd-agent-join-group.c** - Join coordination group
8. **cmd-agent-leave-group.c** - Leave coordination group
9. **cmd-agent-share.c** - Share context with group
10. **cmd-agent-peers.c** - List peers and shared context
11. **cmd-agent-notify.c** - Send messages to group/session
12. **cmd-agent-dashboard.c** - Agent summary and session table
13. **cmd-list-agent-groups.c** - List coordination groups
14. **cmd-list-templates.c** - List session templates
15. **cmd-new-from-template.c** - Create session from template

### Data Structures

```c
struct agent_metadata {
    char *type;              // Agent type (research, development, etc.)
    char *goal;              // Session goal description
    time_t created_at;       // Creation timestamp
    time_t last_activity;    // Last interaction time
    int tasks_completed;     // Completed task counter
    int interactions;        // Interaction counter
    char *runtime_goal_id;   // agent-runtime-mcp goal ID
    int context_saved;       // Whether context is persisted
};

struct session {
    // ... existing fields ...
    struct agent_metadata *agent_metadata;  // Agent tracking
};
```

### MCP Transport

Supports two transport methods:
- **stdio**: Spawn MCP server process, communicate via stdin/stdout pipes
- **socket**: Connect to running MCP server via Unix domain socket (with automatic fallback to stdio)

## Configuration

### MCP Servers

Configure MCP servers in `~/.claude.json`:

```json
{
  "mcpServers": {
    "enhanced-memory": {
      "command": "/path/to/venv/bin/python",
      "args": ["/path/to/server.py"],
      "env": {}
    },
    "agent-runtime-mcp": {
      "command": "/path/to/venv/bin/python",
      "args": ["/path/to/server.py"]
    }
  }
}
```

Tmux automatically loads this configuration on first MCP query.

## Development Status

### ✅ Completed (Phase 2.1-2.4)

- [x] MCP Socket Bridge - Native MCP client (752 lines)
- [x] Stdio transport support
- [x] Automatic config loading from ~/.claude.json
- [x] Session-agent lifecycle integration (274 lines)
- [x] Agent metadata structure and management
- [x] `show-agent` command (95 lines)
- [x] `mcp-query` command structure (106 lines)
- [x] Session lifecycle hooks
- [x] **Session persistence (auto-save/restore)** - COMPLETED
  - Auto-save context on detach
  - Auto-restore context on attach
  - Integrated in cmd-attach-session.c and cmd-detach-client.c
  - Functions: `session_agent_save_context()` and `session_agent_restore_context()`
- [x] Complete documentation (README.md, AGENTIC_FEATURES.md, examples/)
- [x] Test suite and workflow examples

### ✅ Completed (Phase 2.5)

- [x] **MCP protocol initialization handshake** - COMPLETED
  - Proper JSON-RPC 2.0 protocol with initialize/initialized sequence
  - Protocol version negotiation (2024-11-05)
  - Client capabilities advertisement
  - Implemented in mcp-protocol.c (416 lines)
- [x] **Full MCP tool calling integration** - COMPLETED
  - Enhanced `mcp_call_tool_safe()` with automatic retry
  - Connection health monitoring and stale detection
  - Exponential backoff (1s, 2s, 4s)
  - Error rate tracking and automatic reconnection
- [x] **Enhanced error handling and reconnection logic** - COMPLETED
  - `mcp_connect_with_retry()` for robust connection
  - `mcp_connection_stale()` for health checks
  - Connection statistics tracking
  - Resource management (list_resources, read_resource)

### ✅ Completed (Phase 3.0)

- [x] **Enhanced-memory integration for automatic context saving** - COMPLETED
  - Automatic save to enhanced-memory on session detach
  - Session context stored as entities with observations
  - Implemented in session-mcp-integration.c (292 lines)
  - Function: `session_mcp_save_to_memory()`
  - Integrated in cmd-detach-client.c
- [x] **agent-runtime-mcp integration for goal registration** - COMPLETED
  - Automatic goal registration on session creation
  - Goal lifecycle management (register, update, complete)
  - Functions: `session_mcp_register_goal()`, `session_mcp_update_goal_status()`, `session_mcp_complete_goal()`
  - Integrated in cmd-new-session.c and session-agent.c
  - Goal completion on session destruction

### ✅ Completed (Phase 4.0 - Runtime Testing)

- [x] **Runtime testing with live MCP servers** - COMPLETED (2025-11-12)
  - Comprehensive test suite executed successfully
  - Both enhanced-memory and agent-runtime-mcp tested
  - All integration points verified working
  - See TESTING_RESULTS.md for detailed results
- [x] **Documentation and testing infrastructure** - COMPLETED
  - test-mcp-integration.sh (229 lines, 7 tests)
  - test-with-live-servers.md (397 lines)
  - QUICKSTART.md (350 lines)
  - TESTING_RESULTS.md (comprehensive results)

### ✅ Completed (Phase 4.1)

- [x] **Unix domain socket transport** - COMPLETED
  - Native socket support for better performance
  - Automatic fallback to stdio if socket unavailable
  - Socket path configuration in ~/.claude.json
  - Non-blocking I/O with proper buffering
  - Implemented in mcp-socket.c (400+ lines)
- [x] **Connection pooling** - COMPLETED
  - Per-server connection pools
  - Reference counting and automatic cleanup
  - Idle timeout (5 minutes default)
  - Configurable pool sizes
  - Pool statistics tracking
  - Implemented in mcp-pool.c (400+ lines)
- [x] **Performance metrics tracking** - COMPLETED
  - Per-server latency tracking (min/max/avg/p95/p99)
  - Success/failure rate monitoring
  - Throughput metrics (bytes/sec, messages/sec)
  - Connection health tracking
  - Error type tracking
  - Implemented in mcp-metrics.c (300+ lines)
- [x] **mcp-stats command** - COMPLETED
  - Display per-server statistics
  - Connection pool statistics
  - Performance metrics visualization
  - Implemented in cmd-mcp-stats.c (237 lines)

### ✅ Completed (Phase 4.2)

- [x] **Async MCP operations** - COMPLETED
  - Non-blocking tool calls with libevent integration
  - Priority-based request queuing (urgent, high, normal, low)
  - Callback-based completion handlers
  - Background context saving
  - Parallel request execution support
  - Implemented in mcp-async.c (700+ lines)
- [x] **Enhanced responsiveness** - COMPLETED
  - MCP operations no longer block tmux UI
  - Per-server concurrency limits (5 concurrent max)
  - Request cancellation support
  - Comprehensive async test suite (12/12 tests passing)

### ✅ Completed (Phase 4.3)

- [x] **Multi-session agent coordination** - COMPLETED
  - Extended session_agent structure with coordination fields (285 lines)
  - 10 coordination functions (join, leave, share, sync, etc.)
  - 5 new commands: agent-join-group, agent-leave-group, agent-share, agent-peers, list-agent-groups
  - Automatic peer discovery via RB_FOREACH iteration
  - Coordinator/member roles (first session becomes coordinator)
  - Key-value context sharing between coordinated sessions
  - Comprehensive test suite (20/20 tests, 38/38 assertions passing)

### ✅ Completed (Phase 4.4 - Advanced Features)

- [x] **Agent performance analytics** - COMPLETED
  - Analytics engine with per-type metrics tracking
  - Session lifecycle tracking (start/end/success/failure)
  - `agent-analytics` command with summary and type-filtered views
  - 14/14 tests passing
- [x] **Session templates** - COMPLETED
  - Template engine with variable substitution ({{GOAL}}, {{SESSION}}, {{GROUP}})
  - 3 built-in templates: research, development, simple
  - `list-templates` and `new-from-template` commands
- [x] **Advanced context management** - COMPLETED
  - Semantic context extraction with relevance scoring
  - Context compression via deduplication and filtering
  - Smart context saving integrated with enhanced-memory
- [x] **Learning and optimization** - COMPLETED
  - Pattern recognition (success, failure, workflow, efficiency)
  - 4 optimization strategies: workflow, performance, efficiency, quality
  - `agent-optimize` command with auto-strategy selection

### ✅ Completed (Phase 5.0 - Deep Agentic Integration)

- [x] **MCP Server Mode** - COMPLETED
  - tmux as JSON-RPC 2.0 MCP server via `tmux mcp-serve`
  - 7 tools: list_sessions, capture_pane, send_keys, get_analytics, get_coordination, create_agent_session, notify_session
  - Stdio transport for integration with AI tools (~/.claude.json)
  - Implemented in mcp-server.c/h, cmd-mcp-serve.c (1056 lines)
- [x] **Agent Format Strings** - COMPLETED
  - 10 new `#{agent_*}` format variables for status bar display
  - Variables: type, goal, tasks, interactions, runtime_id, group, peers, is_coordinator, context_saved, duration
  - Added to format.c format_table[] (133 lines)
- [x] **Pane Content Pipeline** - COMPLETED
  - Automatic capture of scrollback on detach
  - Line classification: commands, errors, files, output
  - Semantic extraction and compression
  - Saved to enhanced-memory as pane_context entities
  - Implemented in session-mcp-integration.c (160 lines)
- [x] **Event-driven MCP Hooks** - COMPLETED
  - Fire-and-forget events to enhanced-memory on lifecycle changes
  - Events: agent_created, agent_completed, agent_group_changed, context_saved, session_attached, session_detached
  - Implemented in mcp-events.c/h (292 lines)
- [x] **Inter-session Message Bus** - COMPLETED
  - `agent-notify` / `anotify` command
  - Group notification: `-g <group> -m "message"` (sends to all peers)
  - Direct notification: `-t <session> -m "message"`
  - Display via status bar messages
  - Implemented in cmd-agent-notify.c (145 lines)
- [x] **Agent Dashboard** - COMPLETED
  - `agent-dashboard` / `adash` command
  - Summary stats, agent session table, coordination groups
  - `-s` flag for summary-only mode
  - Implemented in cmd-agent-dashboard.c (213 lines)

### Bug Fixes (2026-03-02)

- [x] MCP initialize/initialized handshake per JSON-RPC 2.0 spec
- [x] JSON injection prevention via `json_escape()` in all MCP payloads
- [x] Zombie process prevention with SIGKILL fallback after SIGTERM
- [x] Non-blocking session lifecycle with `mcp_server_ready()` guard
- [x] C-native JSON config parser (removed Python helper dependency)
- [x] Socket timeout reduced from 5s to 2s
- [x] Connection health checks cover both socket and stdio transports

**Total Implementation**: 11,200+ lines of agentic C code across 35 source files

## Testing

### Test Agent Features

```bash
# Kill any running tmux server
tmux kill-server

# Create test session
tmux new-session -G testing -o "Test agentic features" -s test

# Verify agent metadata
tmux show-agent -t test

# Should show:
# - Agent type: testing
# - Goal: Test agentic features
# - Timestamps
# - Statistics
```

### Test MCP Integration

```bash
# Ensure MCP servers are configured in ~/.claude.json
jq '.mcpServers | keys[]' ~/.claude.json

# Test MCP query
tmux mcp-query enhanced-memory get_memory_status '{}'

# View connection stats
tmux mcp-stats
```

## Troubleshooting

### Commands Not Found

```bash
# Verify installation
which tmux
# Should show: /usr/local/bin/tmux (or /opt/homebrew/bin/tmux on macOS)

# Check version
tmux -V
# Should show: tmux next-3.7

# Verify agentic commands are available
tmux list-commands | grep -cE "(agent|mcp|template)"
# Should show: 15

# If commands missing, kill server and retry
tmux kill-server
tmux new-session
```

### MCP Connection Errors

```bash
# Check MCP server paths in ~/.claude.json
jq '.mcpServers | keys[]' ~/.claude.json

# Test MCP connectivity
tmux mcp-query enhanced-memory get_memory_status '{}'

# View connection statistics
tmux mcp-stats
```

### Agent Metadata Not Showing

```bash
# Ensure you created session with -G flag
tmux new-session -G research -o "My goal" -s my-session

# Without -G, no agent metadata is created
tmux show-agent -t my-session
# Should show agent information

# If agent metadata missing, session was created without -G
```

## Contributing

This is a personal fork with agentic features. For contributions:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

Tmux is distributed under the ISC license. See LICENSE file for details.

This agentic feature set maintains the same license.

## References

- [Tmux Official Documentation](https://github.com/tmux/tmux/wiki)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [Enhanced Memory MCP](https://github.com/marc-shade/enhanced-memory-mcp)
- [Agent Runtime MCP](https://github.com/marc-shade/agent-runtime-mcp)

## Acknowledgments

- Original tmux by Nicholas Marriott and contributors
- MCP specification by Anthropic
- Agentic features by Marc Shade
