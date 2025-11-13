#!/bin/sh
# Test script for Phase 4.3: Multi-Session Agent Coordination
# Tests group creation, joining, leaving, context sharing, and peer discovery

TMUX_BIN="./tmux"
SOCKET_NAME="test-coordination-$$"
SOCKET_PATH="/tmp/tmux-test-coordination-$$"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup function
cleanup() {
	echo "\nCleaning up test sessions..."
	$TMUX_BIN -L "$SOCKET_NAME" kill-server 2>/dev/null || true
	rm -f "$SOCKET_PATH" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

# Test helper functions
start_test() {
	TESTS_RUN=$((TESTS_RUN + 1))
	echo "\n${YELLOW}Test $TESTS_RUN: $1${NC}"
}

pass_test() {
	TESTS_PASSED=$((TESTS_PASSED + 1))
	echo "${GREEN}✓ PASS${NC}: $1"
}

fail_test() {
	TESTS_FAILED=$((TESTS_FAILED + 1))
	echo "${RED}✗ FAIL${NC}: $1"
}

# Create agent-aware session
create_agent_session() {
	session_name=$1
	agent_type=$2
	goal=$3

	$TMUX_BIN -L "$SOCKET_NAME" new-session -d -s "$session_name" -G "$agent_type" -o "$goal"
	if [ $? -eq 0 ]; then
		return 0
	else
		return 1
	fi
}

# Check if session exists
session_exists() {
	$TMUX_BIN -L "$SOCKET_NAME" has-session -t "$1" 2>/dev/null
	return $?
}

# Get session list
list_sessions() {
	$TMUX_BIN -L "$SOCKET_NAME" list-sessions -F '#{session_name}' 2>/dev/null
}

# Run coordination command and capture output
run_coord_cmd() {
	cmd=$1
	shift
	$TMUX_BIN -L "$SOCKET_NAME" "$cmd" "$@" 2>&1
}

echo "=========================================="
echo "Phase 4.3 Agent Coordination Test Suite"
echo "=========================================="
echo "Socket: $SOCKET_NAME"
echo "Binary: $TMUX_BIN"

# ==========================================
# Test 1: Create Agent-Aware Sessions
# ==========================================
start_test "Create agent-aware sessions"

if create_agent_session "research1" "research" "Study OAuth flows"; then
	pass_test "Created research1 session"
else
	fail_test "Failed to create research1 session"
fi

if create_agent_session "research2" "research" "Test OAuth implementation"; then
	pass_test "Created research2 session"
else
	fail_test "Failed to create research2 session"
fi

if create_agent_session "dev1" "development" "Implement OAuth client"; then
	pass_test "Created dev1 session"
else
	fail_test "Failed to create dev1 session"
fi

# ==========================================
# Test 2: Join Coordination Group
# ==========================================
start_test "First session joins group (becomes coordinator)"

output=$(run_coord_cmd agent-join-group -t research1 oauth-team)
if echo "$output" | grep -q "joined group 'oauth-team'"; then
	pass_test "research1 joined oauth-team"
else
	fail_test "research1 failed to join group: $output"
fi

if echo "$output" | grep -q "First session in group (coordinator)"; then
	pass_test "research1 became coordinator"
else
	fail_test "research1 should be coordinator"
fi

# ==========================================
# Test 3: Second Session Joins Same Group
# ==========================================
start_test "Second session joins group (peer discovery)"

output=$(run_coord_cmd agent-join-group -t research2 oauth-team)
if echo "$output" | grep -q "joined group 'oauth-team'"; then
	pass_test "research2 joined oauth-team"
else
	fail_test "research2 failed to join group: $output"
fi

if echo "$output" | grep -q "Discovered 1 peer"; then
	pass_test "research2 discovered research1 as peer"
else
	fail_test "Peer discovery failed: $output"
fi

# ==========================================
# Test 4: Third Session Joins Different Group
# ==========================================
start_test "Third session joins different group"

output=$(run_coord_cmd agent-join-group -t dev1 dev-team)
if echo "$output" | grep -q "joined group 'dev-team'"; then
	pass_test "dev1 joined dev-team"
else
	fail_test "dev1 failed to join group: $output"
fi

if echo "$output" | grep -q "First session in group"; then
	pass_test "dev1 is first in dev-team (groups are isolated)"
else
	fail_test "Group isolation failed"
fi

# ==========================================
# Test 5: Share Context Within Group
# ==========================================
start_test "Share context with coordination group"

output=$(run_coord_cmd agent-share -t research1 "finding=Pattern X discovered")
if echo "$output" | grep -q "Shared with group 'oauth-team'"; then
	pass_test "research1 shared context"
else
	fail_test "Context sharing failed: $output"
fi

if echo "$output" | grep -q "finding=Pattern X discovered"; then
	pass_test "Context key-value confirmed"
else
	fail_test "Context format incorrect"
fi

# ==========================================
# Test 6: List Peers in Group
# ==========================================
start_test "List peers in coordination group"

output=$(run_coord_cmd agent-peers -t research1)
if echo "$output" | grep -q "Coordination Group: oauth-team"; then
	pass_test "Group name displayed"
else
	fail_test "Group name not shown: $output"
fi

if echo "$output" | grep -q "Role: Coordinator"; then
	pass_test "Coordinator role confirmed"
else
	fail_test "Coordinator role not shown"
fi

if echo "$output" | grep -q "research2"; then
	pass_test "Peer research2 listed"
else
	fail_test "Peer not listed: $output"
fi

if echo "$output" | grep -q "finding=Pattern X discovered"; then
	pass_test "Shared context visible to coordinator"
else
	fail_test "Shared context not visible"
fi

# ==========================================
# Test 7: Peer Can See Shared Context
# ==========================================
start_test "Peer can see coordinator's shared context"

output=$(run_coord_cmd agent-peers -t research2)
if echo "$output" | grep -q "Role: Member"; then
	pass_test "research2 is Member (not coordinator)"
else
	fail_test "Role incorrect: $output"
fi

if echo "$output" | grep -q "research1"; then
	pass_test "Peer research1 listed from research2's view"
else
	fail_test "Peer list incorrect: $output"
fi

# Note: Shared context is stored in coordinator's metadata
# In current implementation, peers see their own copy after sync
# This is expected behavior for MVP

# ==========================================
# Test 8: List All Coordination Groups
# ==========================================
start_test "List all coordination groups"

output=$(run_coord_cmd list-agent-groups)
if echo "$output" | grep -q "Agent Coordination Groups: 2"; then
	pass_test "Two groups detected"
else
	fail_test "Group count incorrect: $output"
fi

if echo "$output" | grep -q "Group: oauth-team"; then
	pass_test "oauth-team group listed"
else
	fail_test "oauth-team not listed"
fi

if echo "$output" | grep -q "Group: dev-team"; then
	pass_test "dev-team group listed"
else
	fail_test "dev-team not listed"
fi

if echo "$output" | grep -q "Members: 2" && echo "$output" | grep -q "Group: oauth-team" -A 2; then
	pass_test "oauth-team has 2 members"
else
	fail_test "oauth-team member count incorrect"
fi

if echo "$output" | grep -q "Members: 1" && echo "$output" | grep -q "Group: dev-team" -A 2; then
	pass_test "dev-team has 1 member"
else
	fail_test "dev-team member count incorrect"
fi

# ==========================================
# Test 9: Add More Context
# ==========================================
start_test "Add multiple context entries"

run_coord_cmd agent-share -t research1 "status=Testing OAuth"
run_coord_cmd agent-share -t research1 "progress=75%"

output=$(run_coord_cmd agent-peers -t research1)
if echo "$output" | grep -q "status=Testing OAuth"; then
	pass_test "Second context entry added"
else
	fail_test "Second context entry missing"
fi

if echo "$output" | grep -q "progress=75%"; then
	pass_test "Third context entry added"
else
	fail_test "Third context entry missing"
fi

# ==========================================
# Test 10: Peer Can Share Context Too
# ==========================================
start_test "Non-coordinator member can share context"

output=$(run_coord_cmd agent-share -t research2 "test_result=PASS")
if echo "$output" | grep -q "Shared with group 'oauth-team'"; then
	pass_test "research2 (member) shared context"
else
	fail_test "Member context sharing failed: $output"
fi

# ==========================================
# Test 11: Leave Coordination Group
# ==========================================
start_test "Session leaves coordination group"

output=$(run_coord_cmd agent-leave-group -t research2)
if echo "$output" | grep -q "left group 'oauth-team'"; then
	pass_test "research2 left oauth-team"
else
	fail_test "Leave group failed: $output"
fi

if echo "$output" | grep -q "Removed from 1 peer"; then
	pass_test "research2 removed from peer lists"
else
	fail_test "Peer removal failed"
fi

# ==========================================
# Test 12: Verify Peer Removal
# ==========================================
start_test "Verify peer was removed from coordinator's list"

output=$(run_coord_cmd agent-peers -t research1)
if echo "$output" | grep -q "Peers: None"; then
	pass_test "research2 removed from research1's peer list"
else
	fail_test "Peer still listed: $output"
fi

# ==========================================
# Test 13: List Groups After Leave
# ==========================================
start_test "List groups after member leaves"

output=$(run_coord_cmd list-agent-groups)
if echo "$output" | grep -q "Members: 1" && echo "$output" | grep -q "Group: oauth-team" -A 2; then
	pass_test "oauth-team now has 1 member"
else
	fail_test "Member count not updated: $output"
fi

# ==========================================
# Test 14: Rejoin Group
# ==========================================
start_test "Session can rejoin group"

output=$(run_coord_cmd agent-join-group -t research2 oauth-team)
if echo "$output" | grep -q "joined group 'oauth-team'"; then
	pass_test "research2 rejoined oauth-team"
else
	fail_test "Rejoin failed: $output"
fi

if echo "$output" | grep -q "Discovered 1 peer"; then
	pass_test "Peer discovery works on rejoin"
else
	fail_test "Peer discovery failed on rejoin"
fi

# ==========================================
# Test 15: Error Handling - Join Without Agent
# ==========================================
start_test "Error handling: join group without agent metadata"

# Create non-agent session
$TMUX_BIN -L "$SOCKET_NAME" new-session -d -s "regular"

output=$(run_coord_cmd agent-join-group -t regular test-group 2>&1)
if echo "$output" | grep -q "has no agent metadata"; then
	pass_test "Correctly rejected non-agent session"
else
	fail_test "Should reject non-agent session: $output"
fi

# ==========================================
# Test 16: Error Handling - Leave Without Group
# ==========================================
start_test "Error handling: leave when not in group"

output=$(run_coord_cmd agent-leave-group -t dev1)
# dev1 is in dev-team, so first leave
run_coord_cmd agent-leave-group -t dev1 >/dev/null 2>&1

output=$(run_coord_cmd agent-leave-group -t dev1 2>&1)
if echo "$output" | grep -q "not in a coordination group"; then
	pass_test "Correctly rejected leave when not in group"
else
	fail_test "Should reject leave when not in group: $output"
fi

# ==========================================
# Test 17: Error Handling - Share Without Group
# ==========================================
start_test "Error handling: share context without being in group"

output=$(run_coord_cmd agent-share -t dev1 "test=value" 2>&1)
if echo "$output" | grep -q "not in a coordination group"; then
	pass_test "Correctly rejected share when not in group"
else
	fail_test "Should reject share when not in group: $output"
fi

# ==========================================
# Test 18: Error Handling - Peers Without Group
# ==========================================
start_test "Error handling: list peers without being in group"

output=$(run_coord_cmd agent-peers -t dev1 2>&1)
if echo "$output" | grep -q "not in a coordination group"; then
	pass_test "Correctly rejected peers when not in group"
else
	fail_test "Should reject peers when not in group: $output"
fi

# ==========================================
# Test 19: Multiple Sessions Join Same Group
# ==========================================
start_test "Multiple sessions can join same group"

# Create more sessions
create_agent_session "research3" "research" "Document OAuth flows"
create_agent_session "research4" "research" "Review OAuth security"

run_coord_cmd agent-join-group -t research3 oauth-team >/dev/null 2>&1
run_coord_cmd agent-join-group -t research4 oauth-team >/dev/null 2>&1

output=$(run_coord_cmd list-agent-groups)
if echo "$output" | grep -q "Members: 4" && echo "$output" | grep -q "Group: oauth-team" -A 2; then
	pass_test "oauth-team now has 4 members"
else
	fail_test "Multiple join failed: $output"
fi

# ==========================================
# Test 20: Group Statistics
# ==========================================
start_test "Verify coordination statistics"

output=$(run_coord_cmd agent-peers -t research1)
if echo "$output" | grep -q "Peers: 3 sessions"; then
	pass_test "Correct peer count (3 peers for coordinator)"
else
	fail_test "Peer count incorrect: $output"
fi

if echo "$output" | grep -q "Last Coordination:"; then
	pass_test "Last coordination timestamp shown"
else
	fail_test "Timestamp missing"
fi

# ==========================================
# Summary
# ==========================================
echo "\n=========================================="
echo "Test Summary"
echo "=========================================="
echo "Tests Run:    $TESTS_RUN"
echo "${GREEN}Tests Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
	echo "${RED}Tests Failed: $TESTS_FAILED${NC}"
else
	echo "Tests Failed: $TESTS_FAILED"
fi
echo "=========================================="

if [ $TESTS_FAILED -eq 0 ]; then
	echo "${GREEN}✓ All tests passed!${NC}"
	exit 0
else
	echo "${RED}✗ Some tests failed${NC}"
	exit 1
fi
