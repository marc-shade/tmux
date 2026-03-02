# Quick Start: tmux with MCP Integration

Get started with tmux's agentic features and MCP integration in 5 minutes.

## Installation

```bash
# Clone the repository
git clone https://github.com/marc-shade/tmux.git
cd tmux

# Build tmux
./autogen.sh
./configure --enable-utf8proc
make

# Optional: Install system-wide
sudo make install
```

## Basic Usage (No MCP Required)

### 1. Create a Session with Agent Metadata

```bash
# Simple session with goal
./tmux new-session -G "development" -o "Build new feature" -s work

# Session with specific agent type
./tmux new-session -G "debugging" -o "Fix memory leak" -s debug
```

**Agent Types:**
- `development` - General development work
- `debugging` - Bug fixing and debugging
- `testing` - Testing and QA
- `documentation` - Writing documentation
- `research` - Research and exploration
- `custom` - Custom work (default)

### 2. View Agent Metadata

```bash
# Show agent information for current session
./tmux show-agent

# Show for specific session
./tmux show-agent -t work
```

**Output example:**
```
Agent metadata for session 'work':
  Agent type: development
  Goal: Build new feature
  Created: 1735924800 (2025-01-03 10:00:00)
  Last activity: 1735924900
  Tasks completed: 0
  Interactions: 5
```

### 3. Session Persistence

Sessions automatically save and restore context:

```bash
# Create session
./tmux new-session -G "development" -o "Auth module" -s dev

# Detach (context auto-saved)
./tmux detach

# Later: attach (context auto-restored)
./tmux attach -t dev
```

## MCP Integration Setup

### 1. Configure MCP Servers

Create `~/.claude.json`:

```json
{
  "mcpServers": {
    "enhanced-memory": {
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-memory"]
    },
    "agent-runtime-mcp": {
      "command": "python",
      "args": ["-m", "agent_runtime_mcp"]
    }
  }
}
```

### 2. Test MCP Connection

```bash
# Query enhanced-memory status
./tmux mcp-query enhanced-memory get_memory_status

# List available tools
./tmux mcp-query enhanced-memory list_tools
```

### 3. Create Session with MCP Integration

```bash
# Create session (automatically registers with MCP)
./tmux new-session -G "feature-dev" -o "Add dark mode" -s feature

# Work in session...
echo "Implementing dark mode toggle"

# Detach (auto-saves to enhanced-memory)
./tmux detach

# View saved context
./tmux mcp-query enhanced-memory search_nodes '{"query":"session-feature","limit":5}'
```

## Common Workflows

### Development Workflow

```bash
# 1. Start dev session
./tmux new-session -G "development" -o "User authentication" -s auth

# 2. Create windows for different tasks
./tmux new-window -t auth -n "backend"
./tmux new-window -t auth -n "frontend"
./tmux new-window -t auth -n "tests"

# 3. Work across windows...

# 4. Detach when done (context saved automatically)
./tmux detach
```

### Debugging Workflow

```bash
# 1. Start debug session
./tmux new-session -G "debugging" -o "Fix memory leak in parser" -s bugfix

# 2. Split panes for code and logs
./tmux split-window -h -t bugfix
./tmux split-window -v -t bugfix

# 3. Run debugger in one pane, logs in another...

# 4. Session tracks debugging progress automatically
```

### Multi-Project Workflow

```bash
# Project 1: Frontend
./tmux new-session -d -G "frontend" -o "Dashboard redesign" -s frontend

# Project 2: Backend
./tmux new-session -d -G "backend" -o "API optimization" -s backend

# Project 3: DevOps
./tmux new-session -d -G "devops" -o "CI/CD pipeline" -s devops

# List all sessions
./tmux list-sessions

# Switch between projects
./tmux attach -t frontend
./tmux switch-client -t backend
```

## Advanced Features

### Query MCP Servers Directly

```bash
# Create entities in enhanced-memory
./tmux mcp-query enhanced-memory create_entities \
  '{"entities":[{"name":"bug-1234","entityType":"bug","observations":["Critical","User reported"]}]}'

# Search memory
./tmux mcp-query enhanced-memory search_nodes \
  '{"query":"bug","limit":10}'

# Create goal in agent-runtime
./tmux mcp-query agent-runtime-mcp create_goal \
  '{"name":"Fix bugs","description":"Resolve all P0 bugs"}'

# List goals
./tmux mcp-query agent-runtime-mcp list_goals
```

### Session Templates

Create reusable session templates:

```bash
# Save session layout
./tmux show-agent -t work > ~/tmux-templates/development.txt

# Recreate from template
# (Manual process for now, automation coming in Phase 4.0)
```

### Monitor Session Activity

```bash
# Watch all sessions
watch -n 1 './tmux list-sessions'

# Monitor specific session
./tmux attach -t work -r  # read-only mode
```

## Tips and Tricks

### 1. Use Descriptive Goals

```bash
# ❌ Vague
./tmux new-session -o "Work stuff" -s work

# ✅ Specific
./tmux new-session -o "Implement OAuth2 authentication with GitHub" -s auth
```

### 2. Track Progress with MCP

```bash
# At start of session
./tmux mcp-query agent-runtime-mcp create_goal \
  '{"name":"auth-module","description":"Complete OAuth2 implementation"}'

# During session, update tasks
./tmux mcp-query agent-runtime-mcp create_task \
  '{"goal_id":1,"title":"Implement GitHub OAuth flow","priority":10}'

# At end, review progress
./tmux mcp-query agent-runtime-mcp list_tasks '{"goal_id":1}'
```

### 3. Organize by Agent Type

- `development` - Active coding
- `debugging` - Problem solving
- `testing` - QA and verification
- `research` - Learning and exploration
- `documentation` - Writing docs

### 4. Leverage Context Persistence

```bash
# Long-running project
./tmux new-session -G "development" -o "Rewrite payment system" -s payments

# Work over multiple days
./tmux attach -t payments
# ... work ...
./tmux detach

# All context saved to enhanced-memory automatically
# Full project history available for AI analysis
```

## Keyboard Shortcuts

### Session Management

- `tmux attach -t <name>` - Attach to session
- `Ctrl-b d` - Detach from session
- `Ctrl-b s` - List sessions
- `Ctrl-b $` - Rename session

### Window Management

- `Ctrl-b c` - Create new window
- `Ctrl-b n` - Next window
- `Ctrl-b p` - Previous window
- `Ctrl-b ,` - Rename window

### Pane Management

- `Ctrl-b %` - Split horizontally
- `Ctrl-b "` - Split vertically
- `Ctrl-b arrow` - Switch pane
- `Ctrl-b x` - Close pane

## Troubleshooting

### "command not found: mcp-query"

Build from source or check installation path.

### "MCP call failed: connection error"

1. Check `~/.claude.json` configuration
2. Verify MCP servers are running
3. Test server commands manually

### "Agent metadata not found"

Session was created without `-G` flag. Use:
```bash
./tmux new-session -G "custom" -o "My goal" -s session-name
```

### Session not persisting

Ensure you're using the custom tmux build:
```bash
./tmux -V  # Should show version with agentic features
```

## Next Steps

1. **Read full documentation**: `AGENTIC_FEATURES.md`
2. **Try examples**: `examples/test-*.sh`
3. **Set up MCP servers**: `examples/test-with-live-servers.md`
4. **Explore workflows**: Experiment with different agent types and goals

## Resources

- **GitHub**: https://github.com/marc-shade/tmux
- **Documentation**: See `AGENTIC_FEATURES.md`
- **Examples**: See `examples/` directory
- **MCP Protocol**: https://modelcontextprotocol.io

## Getting Help

```bash
# Built-in help
./tmux show-agent -h

# List all commands
./tmux list-commands | grep -E "(mcp|agent)"

# Check server messages
./tmux show-messages
```

---

**Happy hacking with agentic tmux! 🚀**
