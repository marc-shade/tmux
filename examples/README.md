# Tmux Agentic Features - Examples

This directory contains example scripts demonstrating the agentic features of this tmux fork.

## Scripts

### test-agent-features.sh

**Purpose:** Comprehensive test suite for agentic features

**What it tests:**
- Tmux version verification
- Custom command availability (`show-agent`, `mcp-query`)
- Agent-aware session creation
- Agent metadata retrieval
- Multiple agent types (research, development, debugging, testing)
- Sessions without agent metadata

**Usage:**
```bash
cd examples
./test-agent-features.sh
```

**Expected output:**
- ✓ All 10 tests pass
- Green checkmarks for each successful test
- Summary of supported agent types

**Use this when:**
- Verifying your tmux installation
- Testing after making changes to agentic features
- Confirming all commands work correctly

---

### workflow-example.sh

**Purpose:** Demonstrates a complete development workflow using agent-aware sessions

**What it shows:**
1. **Research Phase** - Investigating requirements
2. **Design Phase** - Creating architecture (3-pane layout)
3. **Implementation Phase** - Building the feature
4. **Testing Phase** - Validating implementation
5. **Debugging Phase** - Fixing issues
6. **Documentation Phase** - Writing docs

**Usage:**
```bash
cd examples
./workflow-example.sh
```

**What happens:**
- Creates 6 sessions, each with different agent types
- Simulates work in each phase
- Shows agent metadata for each session
- Provides a summary of all active sessions

**After running:**
```bash
# View all sessions
tmux list-sessions

# Attach to any session
tmux attach -t auth-research
tmux attach -t auth-impl
tmux attach -t auth-test

# View agent info
tmux show-agent -t auth-research

# Clean up
tmux kill-server
```

**Use this when:**
- Learning how to use agentic features
- Planning your own workflow structure
- Demonstrating capabilities to others

---

## Creating Your Own Workflows

### Basic Pattern

```bash
# 1. Create agent-aware session
tmux new-session -G <type> -o "<goal>" -s <name>

# 2. Do your work...

# 3. View agent metadata
tmux show-agent -t <name>

# 4. Session auto-saves on detach
tmux detach-client

# 5. Session auto-restores on attach
tmux attach -t <name>
```

### Agent Types

- **research** - Investigation, learning, exploration
- **development** - Building features, writing code
- **debugging** - Fixing bugs, troubleshooting
- **testing** - Running tests, validation
- **writing** - Documentation, content creation
- **analysis** - Data analysis, performance analysis
- **custom** - Your own workflow type

### Multi-Pane Layouts

```bash
# Create session with agent tracking
tmux new-session -G development -o "Build API" -s api-dev

# Split into multiple panes
tmux split-window -h -t api-dev  # Horizontal split
tmux split-window -v -t api-dev  # Vertical split

# Send commands to specific panes
tmux send-keys -t api-dev:0.0 "nvim server.js" C-m
tmux send-keys -t api-dev:0.1 "npm run dev" C-m
tmux send-keys -t api-dev:0.2 "npm test" C-m
```

### MCP Integration Example

```bash
# Create research session
tmux new-session -G research -o "Study memory optimization" -s mem-research

# Query MCP servers directly
tmux mcp-query enhanced-memory get_memory_status '{}'

# Save research findings to memory
tmux mcp-query enhanced-memory create_entities '{
  "entities": [{
    "name": "memory-optimization-finding",
    "entityType": "research_outcome",
    "observations": [
      "Discovered cache-line optimization pattern",
      "50% performance improvement possible"
    ]
  }]
}'

# Query agent runtime
tmux mcp-query agent-runtime-mcp list_goals '{}'
```

## Tips

### 1. Session Persistence

Sessions auto-save on detach and auto-restore on attach:

```bash
# Work on something
tmux new-session -G development -o "Feature X" -s feature-x

# Detach (saves state)
tmux detach-client

# Later... (restores state)
tmux attach -t feature-x
```

### 2. Multiple Agent Workflows

Run different agents in parallel:

```bash
# Backend development
tmux new-session -G development -o "API endpoints" -s backend -d

# Frontend development
tmux new-session -G development -o "UI components" -s frontend -d

# Testing
tmux new-session -G testing -o "Integration tests" -s testing -d

# Switch between them
tmux switch-client -t backend
tmux switch-client -t frontend
tmux switch-client -t testing
```

### 3. Tracking Progress

Agent metadata includes counters:

```bash
tmux show-agent -t my-session

# Shows:
# - Tasks Completed: 5
# - Interactions: 12
# - Runtime Goal ID: (if connected to agent-runtime-mcp)
# - Context: (if saved to enhanced-memory)
```

### 4. Goal-Driven Development

Connect sessions to agent-runtime-mcp goals:

```bash
# Create runtime goal first (via Claude Code or other tool)
# Then reference it in your session

# Agent automatically tracks:
# - Session duration
# - Task completion
# - Progress towards goal
```

## Troubleshooting

### Commands Not Found

```bash
# Verify installation
tmux -V  # Should show: tmux next-3.6
tmux list-commands | grep -E "(show-agent|mcp-query)"

# If missing, kill server and retry
tmux kill-server
tmux new-session
```

### Agent Metadata Not Showing

```bash
# Ensure you used -G flag when creating session
tmux new-session -G research -o "My goal" -s my-session

# Check agent info
tmux show-agent -t my-session

# If no agent metadata, session was created without -G
```

### MCP Connection Issues

```bash
# Verify MCP config
cat ~/.claude.json | jq '.mcpServers | keys'

# Test config helper
/path/to/tmux/mcp-config-helper.py

# Check server paths
which python  # Should match config
```

## Contributing Examples

Have a useful workflow? Contribute it!

1. Create a new script in this directory
2. Make it executable: `chmod +x your-script.sh`
3. Add documentation here
4. Submit a pull request

## License

These examples are distributed under the same ISC license as tmux.
