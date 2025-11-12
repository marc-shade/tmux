# tmux - Terminal Multiplexer with Agentic AI Integration

[![Version](https://img.shields.io/badge/version-next--3.6-blue.svg)](https://github.com/marc-shade/tmux)
[![License](https://img.shields.io/badge/license-ISC-green.svg)](COPYING)
[![Build Status](https://img.shields.io/badge/build-passing-success.svg)]()
[![Phase](https://img.shields.io/badge/phase-4.0%20complete-brightgreen.svg)](AGENTIC_FEATURES.md)
[![Tests](https://img.shields.io/badge/tests-7%2F7%20passing-success.svg)](TESTING_RESULTS.md)

**tmux** is a terminal multiplexer that enables multiple terminals to be created, accessed, and controlled from a single screen. This fork adds **native Model Context Protocol (MCP) integration** and **agentic AI workflow support** for advanced AI-assisted development workflows.

## 🚀 What's New: Agentic Features

This fork extends tmux with powerful AI agent integration capabilities:

### ✨ Key Features

- **🤖 Agent-Aware Sessions** - Create sessions with agent metadata that tracks purpose, goals, and progress
- **🔌 Native MCP Integration** - Query MCP servers directly from tmux without external tools
- **💾 Session Persistence** - Auto-save/restore session state with agent context
- **📊 Progress Tracking** - Monitor tasks completed, interactions, and runtime metrics
- **🔗 Agent Runtime Integration** - Connect sessions to agent-runtime-mcp goals
- **🧠 Memory Integration** - Save context to enhanced-memory for cross-session learning

### Quick Example

```bash
# Create an agent-aware research session
tmux new-session -G research -o "Study OAuth2 flows" -s oauth-research

# View agent metadata
tmux show-agent -t oauth-research

# Query MCP servers directly
tmux mcp-query enhanced-memory get_memory_status '{}'
tmux mcp-query agent-runtime-mcp list_goals '{}'

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
Run multiple specialized AI agents across different tmux sessions.

```bash
# Research agent
tmux new-session -G research -o "Study API patterns" -s research-api -d

# Development agent
tmux new-session -G development -o "Build API client" -s dev-api -d

# Testing agent
tmux new-session -G testing -o "Test API integration" -s test-api -d

# Switch between agents
tmux list-sessions
tmux attach -t research-api
```

## 🏗️ Architecture

### Core Components

- **mcp-client.c/h** - Native MCP client with stdio/socket transports
- **mcp-config.c/h** - Automatic configuration from ~/.claude.json
- **session-agent.c/h** - Session-agent lifecycle management
- **agent-manager.c** - Global agent coordination
- **cmd-show-agent.c** - Display agent metadata command
- **cmd-mcp-query.c** - Query MCP servers command

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
- **Automatic configuration loading** on first use
- **stdio transport** for spawning MCP server processes
- **JSON-RPC communication** following MCP specification
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

**Total Implementation**: 2,162+ lines of production code + 1,576 lines of documentation

See [TESTING_RESULTS.md](TESTING_RESULTS.md) for detailed test results.

### 🚧 Planned (Phase 4.1+)
- [ ] Socket transport support (Unix domain sockets)
- [ ] Multi-session coordination and orchestration
- [ ] Agent performance metrics and analytics
- [ ] Connection pooling for improved performance
- [ ] Async MCP operations for better responsiveness
- [ ] Session templates library
- [ ] Cross-session learning and optimization

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
