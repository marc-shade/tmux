# Testing with Live MCP Servers

This guide explains how to test tmux's MCP integration with running MCP servers.

## Prerequisites

### 1. Configure MCP Servers

Create or update `~/.claude.json` with MCP server configurations:

```json
{
  "mcpServers": {
    "enhanced-memory": {
      "command": "node",
      "args": ["/path/to/enhanced-memory-mcp/build/index.js"],
      "env": {
        "MEMORY_DB_PATH": "/path/to/memory.db"
      }
    },
    "agent-runtime-mcp": {
      "command": "python",
      "args": ["-m", "agent_runtime_mcp"],
      "env": {
        "DATABASE_PATH": "/path/to/agent-runtime.db"
      }
    }
  }
}
```

### 2. Verify MCP Servers

Test that servers start correctly:

```bash
# Test enhanced-memory
node /path/to/enhanced-memory-mcp/build/index.js

# Test agent-runtime-mcp
python -m agent_runtime_mcp
```

Both servers should start and respond to stdio.

## Test Scenarios

### Test 1: MCP Protocol Handshake

Verify the proper initialize/initialized sequence:

```bash
# Start tmux
./tmux

# In tmux, query a server (triggers handshake)
tmux mcp-query enhanced-memory get_memory_status
```

**Expected behavior:**
1. tmux sends `initialize` request with protocol version
2. Server responds with capabilities
3. tmux sends `initialized` notification
4. Connection established
5. Tool call succeeds

**Debug output:**
Check tmux server log for protocol messages:
```bash
tmux show-messages
```

### Test 2: Session Context Save to enhanced-memory

Test automatic context saving on detach:

```bash
# Create session with agent metadata
tmux new-session -G "development" -o "Implement user authentication" -s dev

# Do some work
echo "Working on auth module"
# ... simulate development work ...

# Detach from session
tmux detach

# Verify context was saved to enhanced-memory
tmux mcp-query enhanced-memory search_nodes '{"query":"session-dev","limit":5}'
```

**Expected result:**
Should return entity with:
- Name: `session-dev-[timestamp]`
- Type: `session_context`
- Observations: agent type, goal, stats, timestamp

**Sample output:**
```json
{
  "success": true,
  "results": [
    {
      "name": "session-dev-1735924800",
      "entityType": "session_context",
      "observations": [
        "Agent type: development",
        "Goal: Implement user authentication",
        "Tasks completed: 0",
        "Interactions: 5",
        "Session duration: 300 seconds",
        "Last activity: 2025-01-03 10:00:00"
      ]
    }
  ]
}
```

### Test 3: Goal Registration with agent-runtime-mcp

Test automatic goal registration:

```bash
# Create session (automatically registers goal)
tmux new-session -G "feature-development" -o "Add dark mode support" -s feature

# Show agent metadata (includes runtime_goal_id)
tmux show-agent -t feature
```

**Expected output:**
```
Agent metadata for session 'feature':
  Agent type: feature-development
  Goal: Add dark mode support
  Created: 1735924800
  Last activity: 1735924800
  Tasks completed: 0
  Interactions: 0
  Runtime goal ID: 42
  Context saved: 0
```

**Verify goal in agent-runtime:**
```bash
tmux mcp-query agent-runtime-mcp get_goal '{"goal_id":42}'
```

**Expected result:**
```json
{
  "goal_id": 42,
  "name": "session-feature",
  "description": "Add dark mode support",
  "status": "active",
  "created_at": "2025-01-03T10:00:00Z"
}
```

### Test 4: Goal Completion

Test automatic goal completion on session destruction:

```bash
# Create session
tmux new-session -G "bugfix" -o "Fix memory leak in parser" -s bugfix

# Get goal ID
GOAL_ID=$(tmux show-agent -t bugfix | grep "Runtime goal ID" | awk '{print $4}')

# Destroy session (should complete goal)
tmux kill-session -t bugfix

# Verify goal status
tmux mcp-query agent-runtime-mcp get_goal "{\"goal_id\":$GOAL_ID}"
```

**Expected result:**
```json
{
  "goal_id": 42,
  "status": "completed",
  "completed_at": "2025-01-03T11:00:00Z"
}
```

### Test 5: Connection Retry Logic

Test automatic reconnection on failure:

```bash
# Start session with MCP integration
tmux new-session -G "testing" -o "Test retry logic" -s retry-test

# In another terminal, stop the MCP server
# (simulate connection failure)
pkill -f "enhanced-memory"

# Try to use MCP (should retry automatically)
tmux mcp-query enhanced-memory get_memory_status

# Restart the server
node /path/to/enhanced-memory-mcp/build/index.js &

# Retry should succeed after server restart
```

**Expected behavior:**
- First call fails after retries (3 attempts with backoff)
- After server restart, next call succeeds
- Connection re-established automatically

### Test 6: Stale Connection Detection

Test that stale connections are detected and refreshed:

```bash
# Create session
tmux new-session -G "testing" -o "Test stale connection" -s stale-test

# Wait for connection to become idle (5+ minutes)
sleep 360

# Make MCP call (should detect stale connection and reconnect)
tmux mcp-query enhanced-memory get_memory_status
```

**Expected behavior:**
- Detects connection has been idle > 5 minutes
- Automatically reconnects before making call
- Call succeeds with fresh connection

### Test 7: Multiple Sessions with Different Goals

Test concurrent sessions with independent goals:

```bash
# Session 1: Frontend work
tmux new-session -d -G "frontend" -o "Redesign dashboard" -s frontend

# Session 2: Backend work
tmux new-session -d -G "backend" -o "Optimize database queries" -s backend

# Session 3: Documentation
tmux new-session -d -G "documentation" -o "Update API docs" -s docs

# List all goals
tmux mcp-query agent-runtime-mcp list_goals

# Each should have separate goal in agent-runtime
# Each will save separate context to enhanced-memory
```

**Expected result:**
```json
{
  "goals": [
    {
      "goal_id": 1,
      "name": "session-frontend",
      "description": "Redesign dashboard",
      "status": "active"
    },
    {
      "goal_id": 2,
      "name": "session-backend",
      "description": "Optimize database queries",
      "status": "active"
    },
    {
      "goal_id": 3,
      "name": "session-docs",
      "description": "Update API docs",
      "status": "active"
    }
  ]
}
```

### Test 8: Session Restore and Context Continuity

Test that context persists across session detach/attach:

```bash
# Create session and do work
tmux new-session -G "development" -o "Implement auth module" -s dev
# ... do work ...

# Detach (saves context)
tmux detach -s dev

# Context is now in enhanced-memory

# Later, re-attach
tmux attach -t dev

# Query for previous context
tmux mcp-query enhanced-memory search_nodes '{"query":"session-dev","limit":10}'

# Should show history of all context saves from this session
```

## Debugging

### Enable Debug Logging

```bash
# Start tmux with verbose logging
TMUX_DEBUG=1 ./tmux -vv
```

### Check MCP Server Logs

```bash
# enhanced-memory logs
tail -f /path/to/enhanced-memory.log

# agent-runtime logs
tail -f /path/to/agent-runtime.log
```

### View tmux Server Messages

```bash
tmux show-messages
```

### Inspect Connection State

```bash
# Show agent metadata (includes connection info)
tmux show-agent -t session-name
```

## Common Issues

### "Connection error or timeout"

**Cause:** MCP server not running or not configured correctly.

**Solution:**
1. Check `~/.claude.json` configuration
2. Verify server command works manually
3. Check server logs for errors

### "Goal registration failed"

**Cause:** agent-runtime-mcp not accessible.

**Solution:**
1. Verify agent-runtime-mcp server is running
2. Test with direct `mcp-query` command
3. Check database permissions

### "Context save failed"

**Cause:** enhanced-memory not accessible.

**Solution:**
1. Verify enhanced-memory server is running
2. Check database path and permissions
3. Test with `get_memory_status` query

## Performance Considerations

### Connection Pooling

- tmux maintains persistent connections to MCP servers
- Connections are reused across multiple sessions
- Stale connections (idle > 5 minutes) are automatically refreshed

### Retry Strategy

- Maximum 3 retry attempts per operation
- Exponential backoff: 1s, 2s, 4s
- Failed operations return error after all retries exhausted

### Memory Usage

- Each session context save: ~1-2KB
- Goal metadata: ~500 bytes
- Connection state: ~1KB per server

## Next Steps

1. **Monitor**: Watch MCP server logs during testing
2. **Verify**: Check enhanced-memory and agent-runtime databases
3. **Benchmark**: Test with multiple concurrent sessions
4. **Integrate**: Connect to production MCP servers

## Support

For issues with:
- **tmux MCP integration**: https://github.com/marc-shade/tmux/issues
- **enhanced-memory**: Check enhanced-memory-mcp documentation
- **agent-runtime-mcp**: Check agent-runtime-mcp documentation
