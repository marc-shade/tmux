#!/usr/bin/env python3
"""
MCP Configuration Helper
Reads ~/.claude.json and outputs MCP server configs in a simple format for tmux.
"""
import json
import os
import sys

def main():
    config_path = os.path.expanduser("~/.claude.json")

    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
    except FileNotFoundError:
        # No config file, exit silently
        sys.exit(0)
    except json.JSONDecodeError as e:
        print(f"Error parsing {config_path}: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error reading {config_path}: {e}", file=sys.stderr)
        sys.exit(1)

    servers = config.get("mcpServers", {})

    # Output format (one server per section):
    # SERVER_START
    # name=<server-name>
    # command=<command-path>
    # arg=<arg1>
    # arg=<arg2>
    # ...
    # SERVER_END

    for name, cfg in servers.items():
        print("SERVER_START")
        print(f"name={name}")

        command = cfg.get("command", "")
        if command:
            print(f"command={command}")

        args = cfg.get("args", [])
        for arg in args:
            print(f"arg={arg}")

        print("SERVER_END")
        print()  # Blank line between servers

if __name__ == "__main__":
    main()
