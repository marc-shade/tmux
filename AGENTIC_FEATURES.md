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
git checkout agentic-features

./autogen.sh
./configure --enable-utf8proc
make -j8
sudo make install
```

### Verify Installation

```bash
tmux -V
# Should show: tmux next-3.6

tmux list-commands | grep -E "(show-agent|mcp-query)"
# Should show both custom commands
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

1. **mcp-client.c/h**: Native MCP client with stdio and socket transports
2. **mcp-config.c/h**: Configuration parser for `~/.claude.json`
3. **session-agent.c/h**: Session-agent lifecycle management
4. **agent-manager.c**: Global agent coordination (future)
5. **cmd-show-agent.c**: `show-agent` command implementation
6. **cmd-mcp-query.c**: `mcp-query` command implementation

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
- **stdio**: Spawn MCP server process, communicate via stdin/stdout
- **socket**: Connect to running MCP server via Unix socket (future)

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

### ⚠️ In Progress (Phase 2.5)

- [ ] MCP protocol initialization handshake (partial - basic structure in place)
- [ ] Full MCP tool calling integration (framework ready, needs protocol completion)
- [ ] Enhanced error handling and reconnection logic
- [ ] Runtime testing with live MCP servers
- [ ] Socket transport support (stdio working, socket planned)

### 🚧 Planned (Phase 3.0)

- [ ] Enhanced-memory integration for automatic context saving
- [ ] agent-runtime-mcp integration for goal registration
- [ ] Multi-session coordination and orchestration
- [ ] Agent performance metrics and analytics
- [ ] Session templates library
- [ ] Cross-session learning and optimization

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

# Test configuration loading
/Volumes/FILES/code/tmux/mcp-config-helper.py

# Should output server configurations

# Test MCP query (requires MCP protocol completion)
tmux mcp-query enhanced-memory get_memory_status '{}'
```

## Troubleshooting

### Commands Not Found

```bash
# Verify installation
which tmux
# Should show: /usr/local/bin/tmux

# Check version
tmux -V
# Should show: tmux next-3.6

# Verify commands are available
tmux list-commands | wc -l
# Should show: 92 (including show-agent and mcp-query)

# If commands missing, kill server and retry
tmux kill-server
tmux new-session
```

### MCP Connection Errors

```bash
# Verify config helper works
/Volumes/FILES/code/tmux/mcp-config-helper.py

# Check MCP server paths in ~/.claude.json
jq '.mcpServers | keys[]' ~/.claude.json

# Ensure server commands are executable
which python
# Should match paths in config
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
