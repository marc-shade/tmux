# tmux - Terminal Multiplexer with Agentic AI Integration

[![Version](https://img.shields.io/badge/version-next--3.7-blue.svg)](https://github.com/marc-shade/tmux)
[![License](https://img.shields.io/badge/license-ISC-green.svg)](COPYING)
[![Build Status](https://img.shields.io/badge/build-passing-success.svg)]()
[![Phase](https://img.shields.io/badge/phase-5.0%20complete-brightgreen.svg)](AGENTIC_FEATURES.md)
[![Commands](https://img.shields.io/badge/commands-111%20total%20%7C%2015%20agentic-informational.svg)]()

**tmux** is a terminal multiplexer that enables multiple terminals to be created, accessed, and controlled from a single screen. This fork adds **native Model Context Protocol (MCP) integration** and **agentic AI workflow support** for advanced AI-assisted development workflows.

## 🚀 What's New: Agentic Features

This fork extends tmux with powerful AI agent integration capabilities:

### ✨ Key Features

- **🤖 Agent-Aware Sessions** - Create sessions with agent metadata that tracks purpose, goals, and progress
- **🔌 Native MCP Integration** - Query MCP servers directly from tmux without external tools
- **💾 Session Persistence** - Auto-save/restore session state with agent context
- **📊 Agent Analytics** - Track session performance, success rates, and per-type metrics
- **🔗 Agent Runtime Integration** - Connect sessions to agent-runtime-mcp goals
- **🧠 Memory Integration** - Save context to enhanced-memory for cross-session learning
- **👥 Multi-Session Coordination** - Coordinate multiple AI agents across sessions with shared context
- **📋 Session Templates** - Create sessions from built-in templates (research, development, simple)
- **🧬 Learning & Optimization** - Pattern recognition and workflow optimization from session history
- **🔍 Semantic Context** - Smart context extraction with relevance scoring and compression

### Quick Example

```bash
# Create an agent-aware research session
tmux new-session -G research -o "Study OAuth2 flows" -s oauth-research

# Or create from a built-in template
tmux new-from-template -t research -s oauth-research -g "Study OAuth2 flows"

# View agent metadata
tmux show-agent -t oauth-research

# Query MCP servers directly
tmux mcp-query enhanced-memory get_memory_status '{}'
tmux mcp-query agent-runtime-mcp list_goals '{}'

# View performance analytics
tmux agent-analytics
tmux agent-analytics -t research    # Per-type breakdown

# Coordinate multiple agents
tmux agent-join-group -g oauth-team
tmux agent-share -k status -v "researching grant types"
tmux agent-peers                    # List peers and shared context

# Get optimization recommendations
tmux agent-optimize -s auto

# Send notifications between agent sessions
tmux agent-notify -g oauth-team -m "found critical pattern"
tmux agent-notify -t dev-oauth -m "update dependency"

# View agent dashboard
tmux agent-dashboard        # Full dashboard with all sessions
tmux agent-dashboard -s     # Summary only

# Use tmux as an MCP server (for AI tools to control tmux)
tmux mcp-serve              # Start JSON-RPC 2.0 MCP server on stdio

# Use agent format strings in your status bar
# set -g status-right '#{agent_type}: #{agent_goal} [#{agent_tasks} tasks]'

# Session auto-saves on detach, auto-restores on attach
tmux detach-client
tmux attach-session -t oauth-research  # Restores full context
```

## 📦 Installation

### Prerequisites

**macOS:**
```bash
brew install libevent ncurses utf8proc automake pkg-config
```

**Debian/Ubuntu:**
```bash
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

## 📖 Documentation

- **[Agentic Features Guide](AGENTIC_FEATURES.md)** - Complete guide to AI agent integration
- **[Standard tmux Documentation](README)** - Original tmux documentation
- **[tmux Wiki](https://github.com/tmux/tmux/wiki)** - Community wiki and FAQ
- **Man Page** - `man tmux` or `nroff -mdoc tmux.1 | less`

## 🎯 Use Cases

### 1. AI-Assisted Development
Track development sessions with agent metadata, automatically save context, and integrate with AI tools.

```bash
tmux new-session -G development -o "Build authentication system" -s auth-dev
# Work on your code with full agent tracking...
tmux show-agent  # View progress statistics
```

### 2. Research Workflows
Document research sessions with goals and outcomes, persist findings across sessions.

```bash
tmux new-session -G research -o "Investigate memory optimization" -s perf-research
# Research and experiment...
tmux mcp-query enhanced-memory create_entities '{"entities":[{"name":"research-finding","entityType":"insight","observations":["discovered X pattern"]}]}'
```

### 3. Multi-Agent Coordination
Run multiple specialized AI agents across tmux sessions with shared context.

```bash
# Research agent
tmux new-session -G research -o "Study API patterns" -s research-api -d
tmux agent-join-group -t research-api -g api-team

# Development agent
tmux new-session -G development -o "Build API client" -s dev-api -d
tmux agent-join-group -t dev-api -g api-team

# Testing agent
tmux new-session -G testing -o "Test API integration" -s test-api -d
tmux agent-join-group -t test-api -g api-team

# Share context between agents
tmux agent-share -t research-api -k findings -v "REST preferred over GraphQL"
tmux agent-peers -t dev-api  # See shared context from peers

# View coordination groups
tmux list-agent-groups
```

## 🏗️ Architecture

### Core Components

| Component | Description |
|-----------|-------------|
| **mcp-client.c/h** | Native MCP client with stdio/socket transports, JSON-RPC 2.0 |
| **mcp-config.c/h** | C-native JSON parser for ~/.claude.json configuration |
| **mcp-protocol.c** | MCP protocol handshake (initialize/initialized sequence) |
| **mcp-async.c** | Async MCP operations with libevent, priority queuing |
| **mcp-pool.c** | Connection pooling with idle timeout and reference counting |
| **mcp-metrics.c** | Per-server latency tracking (min/max/avg/p95/p99) |
| **mcp-socket.c** | Unix domain socket transport with non-blocking I/O |
| **session-agent.c/h** | Session-agent lifecycle management and coordination |
| **session-mcp-integration.c** | Enhanced-memory and agent-runtime-mcp integration |
| **mcp-server.c/h** | tmux as MCP server (JSON-RPC 2.0, 7 tools) |
| **mcp-events.c/h** | Event-driven MCP hooks for agent lifecycle |
| **session-template.c** | Built-in session templates (research, development, simple) |
| **agent-manager.c** | Global agent coordination |
| **agent-analytics.c** | Performance analytics engine with per-type metrics |
| **agent-learning.c** | Pattern recognition from session history |
| **agent-optimizer.c** | Workflow optimization strategies and recommendations |
| **context-semantic.c** | Semantic context extraction with relevance scoring |
| **context-compress.c** | Context compression via deduplication and filtering |

### Agentic Commands (15)

| Command | Alias | Description |
|---------|-------|-------------|
| `show-agent` | | Display agent metadata for a session |
| `mcp-query` | | Query MCP servers directly |
| `mcp-stats` | | Display MCP connection and performance statistics |
| `mcp-serve` | | Run tmux as an MCP server (JSON-RPC 2.0, 7 tools) |
| `agent-analytics` | `aanalytics` | View session performance analytics |
| `agent-optimize` | `optim` | Get optimization recommendations |
| `agent-join-group` | `ajoin` | Join a coordination group |
| `agent-leave-group` | `aleave` | Leave a coordination group |
| `agent-share` | `ashare` | Share key-value context with group |
| `agent-peers` | `apeers` | List peers and shared context |
| `agent-notify` | `anotify` | Send messages to group peers or specific sessions |
| `agent-dashboard` | `adash` | View agent summary, session table, and groups |
| `list-agent-groups` | `lsag` | List all coordination groups |
| `list-templates` | `lst` | List available session templates |
| `new-from-template` | `newt` | Create session from template |

### Agent Format Strings (10)

Use these in your status bar or format strings:

| Variable | Description |
|----------|-------------|
| `#{agent_type}` | Agent type (research, development, etc.) |
| `#{agent_goal}` | Session goal description |
| `#{agent_tasks}` | Tasks completed count |
| `#{agent_interactions}` | Interaction count |
| `#{agent_runtime_id}` | Agent runtime goal ID |
| `#{agent_group}` | Coordination group name |
| `#{agent_peers}` | Number of peers in group |
| `#{agent_is_coordinator}` | Whether session is group coordinator |
| `#{agent_context_saved}` | Whether context has been saved |
| `#{agent_duration}` | Session duration in human-readable format |

### Agent Metadata Structure

Each agent-aware session tracks:
- Agent type (research, development, debugging, testing, analysis, writing, custom)
- Session goal description
- Creation and last activity timestamps
- Tasks completed counter
- Interaction counter
- Runtime goal ID (agent-runtime-mcp integration)
- Context persistence status

### MCP Integration

Supports MCP servers configured in `~/.claude.json`:
- **Automatic configuration loading** via C-native JSON parser
- **stdio transport** for spawning MCP server processes
- **Unix socket transport** with connection pooling and idle timeout
- **JSON-RPC 2.0** with proper initialize/initialized handshake
- **Async operations** with priority queuing (urgent/high/normal/low)
- **Performance metrics** with per-server latency tracking (p95/p99)
- **Native C implementation** for optimal performance

## 🔧 Configuration

### MCP Server Setup

Configure MCP servers in `~/.claude.json`:

```json
{
  "mcpServers": {
    "enhanced-memory": {
      "command": "/path/to/venv/bin/python",
      "args": ["/path/to/enhanced-memory-server.py"],
      "env": {}
    },
    "agent-runtime-mcp": {
      "command": "/path/to/venv/bin/python",
      "args": ["/path/to/agent-runtime-server.py"]
    }
  }
}
```

tmux automatically loads this configuration when you run your first `mcp-query` command.

## 🧪 Development Status

### ✅ Phase 2.1-2.4: Foundation (Complete)
- [x] Native MCP client with stdio transport (752 lines)
- [x] Automatic config loading from ~/.claude.json
- [x] Session-agent lifecycle integration (274 lines)
- [x] Agent metadata tracking and management
- [x] `show-agent` command (95 lines)
- [x] `mcp-query` command framework (106 lines)
- [x] **Session persistence (auto-save/restore)** ✨
- [x] Comprehensive documentation (3 guides + examples)
- [x] Test suite and workflow demonstrations

### ✅ Phase 2.5: MCP Protocol (Complete)
- [x] MCP protocol initialization handshake (416 lines)
- [x] Full MCP tool calling integration with retry logic
- [x] Enhanced error handling and reconnection logic
- [x] Connection health monitoring and stale detection
- [x] Exponential backoff retry strategy (1s, 2s, 4s)

### ✅ Phase 3.0: MCP Integration (Complete)
- [x] Enhanced-memory integration for automatic context saving (292 lines)
- [x] Agent-runtime-mcp integration for goal registration
- [x] Goal lifecycle management (register, update, complete)
- [x] Session lifecycle hooks (create, detach, destroy)
- [x] Automatic context persistence on detach

### ✅ Phase 4.0: Runtime Testing (Complete - 2025-11-12)
- [x] Comprehensive runtime testing with live MCP servers
- [x] 7/7 automated tests passing
- [x] Database corruption issues resolved
- [x] All integration points verified working
- [x] Complete testing documentation (600+ lines)

### ✅ Phase 4.1: Socket Transport & Performance (Complete - 2025-11-12)
- [x] Unix domain socket transport (mcp-socket.c, 400+ lines)
- [x] Connection pooling with idle timeout (mcp-pool.c, 400+ lines)
- [x] Performance metrics tracking (mcp-metrics.c, 300+ lines)
- [x] mcp-stats command for statistics (cmd-mcp-stats.c, 237 lines)
- [x] Automatic fallback to stdio if socket unavailable
- [x] Non-blocking I/O with proper buffering
- [x] Per-server latency tracking (min/max/avg/p95/p99)
- [x] Connection health monitoring and pool statistics

See [TESTING_RESULTS.md](TESTING_RESULTS.md) and [PHASE_4.1_FEATURES.md](PHASE_4.1_FEATURES.md) for detailed results.

### ✅ Phase 4.2: Async Operations (Complete - 2025-11-13)
- [x] Asynchronous MCP operations framework (mcp-async.c, 700+ lines)
- [x] Priority-based request queuing (urgent, high, normal, low)
- [x] Callback-based completion handlers
- [x] Timeout handling with libevent integration
- [x] Parallel request execution support
- [x] Background context saving (non-blocking detach)
- [x] Request cancellation support
- [x] Per-server concurrency limits (5 concurrent max)
- [x] Event loop integration with libevent
- [x] Comprehensive async test suite (12/12 tests passing)

**Responsiveness Improvement**: MCP operations no longer block tmux UI

### ✅ Phase 4.3: Multi-Session Coordination (Complete - 2025-11-13)
- [x] Agent coordination fields in session_agent structure (285 lines)
- [x] 10 coordination functions (join, leave, share, sync, etc.)
- [x] `agent-join-group` / `ajoin` - Join coordination group with automatic peer discovery
- [x] `agent-leave-group` / `aleave` - Leave group and remove from peer lists
- [x] `agent-share` / `ashare` - Share key=value context with group
- [x] `agent-peers` / `apeers` - List peers, show role, display shared context
- [x] `list-agent-groups` / `lsag` - List all coordination groups system-wide
- [x] Coordinator/member roles (first session becomes coordinator)
- [x] Automatic bidirectional peer discovery
- [x] Context sharing between coordinated sessions
- [x] Comprehensive test suite (20/20 tests, 38/38 assertions passing)

**Collaboration Enabled**: Multiple AI agents can now coordinate across tmux sessions

### ✅ Phase 4.4: Advanced Features (Complete - 2025-11-13)
- [x] Agent performance analytics engine with per-type metrics (agent-analytics.c, 877 lines)
- [x] `agent-analytics` command with summary and type-filtered views
- [x] Session templates with 3 built-in templates (session-template.c, 890 lines)
- [x] `list-templates` / `new-from-template` commands
- [x] Semantic context extraction with relevance scoring (context-semantic.c, 410 lines)
- [x] Context compression via deduplication and filtering (context-compress.c, 330 lines)
- [x] Learning engine with pattern recognition (agent-learning.c, 550 lines)
- [x] Workflow optimization with 4 strategies (agent-optimizer.c, 380 lines)
- [x] `agent-optimize` command with strategy selection

### ✅ Phase 5.0: Deep Agentic Integration (Complete - 2026-03-02)
- [x] **MCP Server Mode** (`tmux mcp-serve`) - tmux as JSON-RPC 2.0 MCP server with 7 tools (mcp-server.c/h, 1056 lines)
- [x] **Agent Format Strings** - 10 new `#{agent_*}` variables for status bar display (format.c, 133 lines)
- [x] **Pane Content Pipeline** - Auto-capture and classify scrollback on detach (session-mcp-integration.c, 160 lines)
- [x] **Event-driven MCP Hooks** - Fire-and-forget events on agent lifecycle changes (mcp-events.c/h, 292 lines)
- [x] **Inter-session Message Bus** - `agent-notify` for group/direct messaging (cmd-agent-notify.c, 145 lines)
- [x] **Agent Dashboard** - `agent-dashboard` for summary stats and session table (cmd-agent-dashboard.c, 213 lines)

### Bug Fixes (2026-03-02)
- [x] MCP initialize/initialized handshake per JSON-RPC 2.0 spec
- [x] JSON injection prevention via `json_escape()` in all MCP payloads
- [x] Zombie process prevention with SIGKILL fallback after SIGTERM
- [x] Non-blocking session lifecycle with `mcp_server_ready()` guard
- [x] C-native JSON config parser (removed Python helper dependency)
- [x] MCP socket timeout reduced from 5s to 2s for responsiveness
- [x] Connection health checks cover both socket and stdio transports
- [x] Proper forward declarations for all static functions

**Total Implementation**: 11,200+ lines of agentic C code across 35 source files

## 🤝 Contributing

This is a personal fork with agentic features. Contributions are welcome!

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test thoroughly (`tmux kill-server && tmux new-session`)
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## 📝 License

tmux is distributed under the ISC license. See [COPYING](COPYING) for details.

This agentic feature set maintains the same ISC license.

## 🙏 Acknowledgments

- **Original tmux** by [Nicholas Marriott](https://github.com/nicm) and contributors
- **Model Context Protocol** by [Anthropic](https://modelcontextprotocol.io/)
- **Agentic features** by [Marc Shade](https://github.com/marc-shade)

## 📚 Documentation

- **[AGENTIC_FEATURES.md](AGENTIC_FEATURES.md)** - Complete feature documentation and architecture
- **[TESTING_RESULTS.md](TESTING_RESULTS.md)** - Comprehensive test results and analysis
- **[QUICKSTART.md](examples/QUICKSTART.md)** - Quick start guide for new users
- **[test-with-live-servers.md](examples/test-with-live-servers.md)** - Live server testing guide
- **[test-mcp-integration.sh](examples/test-mcp-integration.sh)** - Automated test suite

## 🔗 Links

- [tmux Official Repository](https://github.com/tmux/tmux)
- [tmux Wiki](https://github.com/tmux/tmux/wiki)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [Enhanced Memory MCP](https://github.com/marc-shade/enhanced-memory-mcp)
- [Agent Runtime MCP](https://github.com/marc-shade/agent-runtime-mcp)

## 📧 Support

- **Issues**: [GitHub Issues](https://github.com/marc-shade/tmux/issues)
- **Discussions**: [GitHub Discussions](https://github.com/marc-shade/tmux/discussions)
- **Original tmux mailing list**: [tmux-users](https://groups.google.com/forum/#!forum/tmux-users)

---

**Note**: This is a fork of tmux with experimental agentic features. For production use of standard tmux, see the [official tmux repository](https://github.com/tmux/tmux).
