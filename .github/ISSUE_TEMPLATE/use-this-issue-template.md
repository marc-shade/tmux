---
name: Use this issue template
about: Report issues for tmux with agentic features fork
title: ''
labels: ''
assignees: ''

---

### Note: This is a Fork

This is a fork of tmux with agentic AI integration. Please read [CONTRIBUTING.md](../.github/CONTRIBUTING.md) before opening an issue.

**Issue type (check one):**
- [ ] Bug in agentic features (agent-aware sessions, MCP integration, session persistence)
- [ ] Bug in standard tmux functionality
- [ ] Feature request for agentic features
- [ ] Documentation issue
- [ ] Question

**For standard tmux bugs:** Consider reporting to [upstream tmux](https://github.com/tmux/tmux/issues) instead.

### Issue description

Describe the problem and the steps to reproduce. Add a minimal tmux config if
necessary. Screenshots can be helpful, but no more than one or two.

Do not report bugs (crashes, incorrect behaviour) without reproducing on a tmux
built from the latest code in Git (agentic-features branch).

### Required information

Please provide the following information. These are **required**. Note that bug reports without logs may be ignored or closed without comment.

* tmux version (`tmux -V`).
* Branch (`git branch --show-current` - should be `agentic-features`).
* Platform (`uname -sp`).
* Terminal in use (xterm, rxvt, etc).
* $TERM *inside* tmux (`echo $TERM`).
* $TERM *outside* tmux (`echo $TERM`).
* Logs from tmux (`tmux kill-server; tmux -vv new`).

### Agentic features information (if applicable)

If your issue relates to agentic features:

* Verify custom commands exist (`tmux list-commands | grep -E "(show-agent|mcp-query)"`).
* MCP configuration location (`~/.claude.json` or other).
* Agent metadata output (`tmux show-agent -t <session-name>`).
* MCP server status (if using `mcp-query`).
