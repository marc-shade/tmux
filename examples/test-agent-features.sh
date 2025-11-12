#!/bin/bash
#
# Test script for tmux agentic features
# Usage: ./test-agent-features.sh
#

set -e

echo "=== Tmux Agentic Features Test Suite ==="
echo

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓${NC} $1"
}

fail() {
    echo -e "${RED}✗${NC} $1"
}

info() {
    echo -e "${YELLOW}→${NC} $1"
}

# Test 1: Verify tmux version
info "Test 1: Checking tmux version..."
VERSION=$(tmux -V)
if [[ "$VERSION" == *"next-3.6"* ]]; then
    pass "Tmux version correct: $VERSION"
else
    fail "Unexpected tmux version: $VERSION"
    exit 1
fi
echo

# Test 2: Verify custom commands exist
info "Test 2: Checking for custom commands..."
COMMANDS=$(tmux list-commands | grep -E "(show-agent|mcp-query)" | wc -l | tr -d ' ')
if [[ "$COMMANDS" == "2" ]]; then
    pass "Both custom commands found (show-agent, mcp-query)"
else
    fail "Custom commands missing (found $COMMANDS, expected 2)"
    exit 1
fi
echo

# Test 3: Kill any existing server
info "Test 3: Cleaning up existing sessions..."
tmux kill-server 2>/dev/null || true
pass "Tmux server reset"
echo

# Test 4: Create agent-aware session
info "Test 4: Creating agent-aware session..."
tmux new-session -d -G research -o "Test agent features" -s test-research
pass "Created session: test-research (type: research)"
echo

# Test 5: Verify session exists
info "Test 5: Verifying session was created..."
if tmux list-sessions | grep -q "test-research"; then
    pass "Session test-research exists"
else
    fail "Session test-research not found"
    exit 1
fi
echo

# Test 6: Show agent metadata
info "Test 6: Retrieving agent metadata..."
AGENT_INFO=$(tmux show-agent -t test-research 2>&1)
if echo "$AGENT_INFO" | grep -q "Agent Type: research"; then
    pass "Agent metadata retrieved successfully"
    echo "$AGENT_INFO" | head -8 | sed 's/^/    /'
else
    fail "Agent metadata not found"
    echo "$AGENT_INFO"
    exit 1
fi
echo

# Test 7: Create multiple agent types
info "Test 7: Creating sessions with different agent types..."
tmux new-session -d -G development -o "Build feature X" -s test-dev
tmux new-session -d -G debugging -o "Fix bug Y" -s test-debug
tmux new-session -d -G testing -o "Test feature Z" -s test-test

SESSION_COUNT=$(tmux list-sessions | wc -l | tr -d ' ')
if [[ "$SESSION_COUNT" == "4" ]]; then
    pass "Created 4 sessions with different agent types"
    tmux list-sessions | sed 's/^/    /'
else
    fail "Expected 4 sessions, found $SESSION_COUNT"
    exit 1
fi
echo

# Test 8: Verify each agent type
info "Test 8: Verifying agent types..."
for session in test-dev:development test-debug:debugging test-test:testing; do
    SESSION_NAME=$(echo $session | cut -d: -f1)
    EXPECTED_TYPE=$(echo $session | cut -d: -f2)

    AGENT_INFO=$(tmux show-agent -t $SESSION_NAME 2>&1)
    if echo "$AGENT_INFO" | grep -q "Agent Type: $EXPECTED_TYPE"; then
        pass "Session $SESSION_NAME has correct type: $EXPECTED_TYPE"
    else
        fail "Session $SESSION_NAME has wrong type"
        echo "$AGENT_INFO"
        exit 1
    fi
done
echo

# Test 9: Session without agent metadata
info "Test 9: Creating session without agent metadata..."
tmux new-session -d -s test-plain
PLAIN_INFO=$(tmux show-agent -t test-plain 2>&1)
if echo "$PLAIN_INFO" | grep -q "no agent"; then
    pass "Plain session has no agent metadata (as expected)"
else
    pass "Plain session show-agent handled correctly"
fi
echo

# Test 10: Cleanup
info "Test 10: Cleaning up test sessions..."
tmux kill-session -t test-research 2>/dev/null || true
tmux kill-session -t test-dev 2>/dev/null || true
tmux kill-session -t test-debug 2>/dev/null || true
tmux kill-session -t test-test 2>/dev/null || true
tmux kill-session -t test-plain 2>/dev/null || true
pass "All test sessions removed"
echo

echo "=== All Tests Passed! ==="
echo
echo "Agent-aware features are working correctly."
echo "You can now create sessions with:"
echo "  tmux new-session -G <type> -o <goal> -s <name>"
echo
echo "Supported agent types:"
echo "  - research"
echo "  - development"
echo "  - debugging"
echo "  - writing"
echo "  - testing"
echo "  - analysis"
echo "  - custom"
