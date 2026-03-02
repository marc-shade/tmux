#!/usr/bin/env python3
"""
tmux MCP Server - Exposes tmux as a Model Context Protocol server.

Wraps tmux CLI commands to provide MCP tools for AI agents to control
tmux sessions, capture pane content, send keys, and query agent metadata.

Configure in ~/.claude.json:
  "tmux-mcp": {
    "command": "python3",
    "args": ["/path/to/tmux-mcp-server.py"]
  }
"""

import json
import subprocess
import sys
import os

PROTOCOL_VERSION = "2024-11-05"
SERVER_NAME = "tmux"
SERVER_VERSION = "1.0.0"

TOOLS = [
    {
        "name": "tmux_list_sessions",
        "description": "List all tmux sessions with agent metadata",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "tmux_capture_pane",
        "description": "Capture pane content from a tmux session",
        "inputSchema": {
            "type": "object",
            "properties": {
                "session": {
                    "type": "string",
                    "description": "Session name",
                },
                "lines": {
                    "type": "integer",
                    "description": "Lines to capture (default: 50)",
                },
            },
            "required": ["session"],
        },
    },
    {
        "name": "tmux_send_keys",
        "description": "Send keystrokes to a tmux pane",
        "inputSchema": {
            "type": "object",
            "properties": {
                "session": {
                    "type": "string",
                    "description": "Session name",
                },
                "keys": {
                    "type": "string",
                    "description": "Keys to send",
                },
            },
            "required": ["session", "keys"],
        },
    },
    {
        "name": "tmux_show_agent",
        "description": "Show agent metadata for a session",
        "inputSchema": {
            "type": "object",
            "properties": {
                "session": {
                    "type": "string",
                    "description": "Session name",
                },
            },
            "required": ["session"],
        },
    },
    {
        "name": "tmux_agent_dashboard",
        "description": "Show agent dashboard with all sessions, stats, and groups",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "tmux_agent_notify",
        "description": "Send a notification to a session or coordination group",
        "inputSchema": {
            "type": "object",
            "properties": {
                "message": {
                    "type": "string",
                    "description": "Message to send",
                },
                "session": {
                    "type": "string",
                    "description": "Target session (mutually exclusive with group)",
                },
                "group": {
                    "type": "string",
                    "description": "Target coordination group",
                },
            },
            "required": ["message"],
        },
    },
    {
        "name": "tmux_new_agent_session",
        "description": "Create a new agent-aware tmux session",
        "inputSchema": {
            "type": "object",
            "properties": {
                "name": {
                    "type": "string",
                    "description": "Session name",
                },
                "agent_type": {
                    "type": "string",
                    "description": "Agent type (research, development, debugging, testing, analysis, writing)",
                },
                "goal": {
                    "type": "string",
                    "description": "Session goal description",
                },
                "detached": {
                    "type": "boolean",
                    "description": "Create in detached mode (default: true)",
                },
            },
            "required": ["name", "agent_type", "goal"],
        },
    },
]


def run_tmux(*args):
    """Run a tmux command and return stdout."""
    try:
        result = subprocess.run(
            ["tmux"] + list(args),
            capture_output=True,
            text=True,
            timeout=10,
        )
        return result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        return "", "tmux command timed out", 1
    except FileNotFoundError:
        return "", "tmux not found in PATH", 1


def handle_list_sessions(_args):
    stdout, stderr, rc = run_tmux("list-sessions", "-F",
        "#{session_name}|#{session_windows}|#{session_attached}|#{session_created}")
    if rc != 0:
        return {"error": stderr.strip() or "no tmux server running"}

    sessions = []
    for line in stdout.strip().split("\n"):
        if not line:
            continue
        parts = line.split("|")
        name = parts[0]
        info = {
            "name": name,
            "windows": int(parts[1]) if len(parts) > 1 else 0,
            "attached": int(parts[2]) if len(parts) > 2 else 0,
        }
        # Get agent metadata
        agent_out, _, agent_rc = run_tmux("show-agent", "-t", name)
        if agent_rc == 0 and "Agent Type:" in agent_out:
            for aline in agent_out.strip().split("\n"):
                if ": " in aline:
                    k, v = aline.split(": ", 1)
                    k = k.strip().lower().replace(" ", "_")
                    info[k] = v.strip()
        sessions.append(info)

    return {"sessions": sessions, "count": len(sessions)}


def handle_capture_pane(args):
    session = args.get("session", "")
    lines = args.get("lines", 50)
    if not session:
        return {"error": "session name required"}

    stdout, stderr, rc = run_tmux("capture-pane", "-t", session, "-p",
        "-S", str(-lines))
    if rc != 0:
        return {"error": stderr.strip()}
    return {"content": stdout, "session": session, "lines": lines}


def handle_send_keys(args):
    session = args.get("session", "")
    keys = args.get("keys", "")
    if not session or not keys:
        return {"error": "session and keys required"}

    _, stderr, rc = run_tmux("send-keys", "-t", session, keys, "Enter")
    if rc != 0:
        return {"error": stderr.strip()}
    return {"status": "sent", "session": session}


def handle_show_agent(args):
    session = args.get("session", "")
    if not session:
        return {"error": "session name required"}

    stdout, stderr, rc = run_tmux("show-agent", "-t", session)
    if rc != 0:
        return {"error": stderr.strip()}
    return {"output": stdout.strip()}


def handle_agent_dashboard(_args):
    stdout, stderr, rc = run_tmux("agent-dashboard")
    if rc != 0:
        return {"error": stderr.strip()}
    return {"dashboard": stdout.strip()}


def handle_agent_notify(args):
    message = args.get("message", "")
    session = args.get("session")
    group = args.get("group")

    if not message:
        return {"error": "message required"}

    cmd = ["agent-notify", "-m", message]
    if group:
        cmd.extend(["-g", group])
    elif session:
        cmd.extend(["-t", session])
    else:
        return {"error": "either session or group required"}

    stdout, stderr, rc = run_tmux(*cmd)
    if rc != 0:
        return {"error": stderr.strip()}
    return {"status": "notified", "output": stdout.strip()}


def handle_new_agent_session(args):
    name = args.get("name", "")
    agent_type = args.get("agent_type", "")
    goal = args.get("goal", "")
    detached = args.get("detached", True)

    if not name or not agent_type or not goal:
        return {"error": "name, agent_type, and goal required"}

    cmd = ["new-session", "-G", agent_type, "-o", goal, "-s", name]
    if detached:
        cmd.append("-d")

    _, stderr, rc = run_tmux(*cmd)
    if rc != 0:
        return {"error": stderr.strip()}
    return {"status": "created", "session": name, "agent_type": agent_type, "goal": goal}


TOOL_HANDLERS = {
    "tmux_list_sessions": handle_list_sessions,
    "tmux_capture_pane": handle_capture_pane,
    "tmux_send_keys": handle_send_keys,
    "tmux_show_agent": handle_show_agent,
    "tmux_agent_dashboard": handle_agent_dashboard,
    "tmux_agent_notify": handle_agent_notify,
    "tmux_new_agent_session": handle_new_agent_session,
}


def send_response(response):
    """Write a JSON-RPC response to stdout."""
    line = json.dumps(response)
    sys.stdout.write(line + "\n")
    sys.stdout.flush()


def handle_request(request):
    """Handle a single JSON-RPC request."""
    method = request.get("method", "")
    req_id = request.get("id")

    if method == "initialize":
        send_response({
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {"tools": {}},
                "serverInfo": {
                    "name": SERVER_NAME,
                    "version": SERVER_VERSION,
                },
            },
        })
    elif method == "notifications/initialized":
        pass  # No response needed for notifications
    elif method == "tools/list":
        send_response({
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {"tools": TOOLS},
        })
    elif method == "tools/call":
        params = request.get("params", {})
        tool_name = params.get("name", "")
        tool_args = params.get("arguments", {})

        handler = TOOL_HANDLERS.get(tool_name)
        if handler is None:
            send_response({
                "jsonrpc": "2.0",
                "id": req_id,
                "error": {
                    "code": -32601,
                    "message": f"Unknown tool: {tool_name}",
                },
            })
        else:
            try:
                result = handler(tool_args)
                send_response({
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {
                        "content": [
                            {
                                "type": "text",
                                "text": json.dumps(result, indent=2),
                            }
                        ]
                    },
                })
            except Exception as e:
                send_response({
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "error": {
                        "code": -32603,
                        "message": str(e),
                    },
                })
    elif method == "ping":
        send_response({
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {},
        })
    else:
        if req_id is not None:
            send_response({
                "jsonrpc": "2.0",
                "id": req_id,
                "error": {
                    "code": -32601,
                    "message": f"Method not found: {method}",
                },
            })


def main():
    """Main MCP server loop - read from stdin, write to stdout."""
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
            handle_request(request)
        except json.JSONDecodeError:
            send_response({
                "jsonrpc": "2.0",
                "id": None,
                "error": {
                    "code": -32700,
                    "message": "Parse error",
                },
            })


if __name__ == "__main__":
    main()
