#!/bin/sh
# Test Phase 4.4A: Agent Performance Analytics Integration
# This tests that analytics are properly recorded during session lifecycle

TMUX_BIN="./tmux"
TMUX_SOCKET="test_analytics"
TEST_COUNT=0
PASS_COUNT=0
FAIL_COUNT=0

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

test_result() {
	TEST_COUNT=$((TEST_COUNT + 1))
	if [ $1 -eq 0 ]; then
		printf "${GREEN}✓${NC} Test $TEST_COUNT: $2\n"
		PASS_COUNT=$((PASS_COUNT + 1))
	else
		printf "${RED}✗${NC} Test $TEST_COUNT: $2\n"
		FAIL_COUNT=$((FAIL_COUNT + 1))
	fi
}

# Clean up any existing test server
$TMUX_BIN -L $TMUX_SOCKET kill-server 2>/dev/null || true
sleep 1

echo "Testing Phase 4.4A: Agent Performance Analytics"
echo "================================================"
echo ""

#
# Test 1: Analytics initialization (should return zeros)
#
echo "Test 1: Check initial analytics state"
$TMUX_BIN -f /dev/null -L $TMUX_SOCKET new-session -d -s init -G general -o "Init session"
sleep 1
OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -s 2>&1)
echo "$OUTPUT"

if echo "$OUTPUT" | grep -q "Sessions: 1 total"; then
	test_result 0 "Initial session recorded"
else
	test_result 1 "Initial session not recorded"
fi

#
# Test 2: Create session with agent metadata
#
echo ""
echo "Test 2: Create session with agent type"
$TMUX_BIN -L $TMUX_SOCKET new-session -d -s research -G research -o "Research project"
sleep 1

# Check that session has agent metadata
OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET show-agent 2>&1)
echo "$OUTPUT"

if echo "$OUTPUT" | grep -q "Agent Type: research"; then
	test_result 0 "Session created with agent metadata"
else
	test_result 1 "Session missing agent metadata"
fi

#
# Test 3: Check analytics recorded session start
#
echo ""
echo "Test 3: Verify analytics recorded session start"
OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -s 2>&1)
echo "$OUTPUT"

if echo "$OUTPUT" | grep -q "Sessions: 2 total"; then
	test_result 0 "Analytics recorded both sessions"
else
	test_result 1 "Analytics session count incorrect (got: $OUTPUT)"
fi

if echo "$OUTPUT" | grep -q "2 active"; then
	test_result 0 "Active session count correct"
else
	test_result 1 "Active session count incorrect"
fi

#
# Test 4: Check per-type analytics
#
echo ""
echo "Test 4: Check per-type analytics for 'research' type"
OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -t research 2>&1)
echo "$OUTPUT"

if echo "$OUTPUT" | grep -q "Agent Type: research"; then
	test_result 0 "Per-type analytics available"
else
	test_result 1 "Per-type analytics missing"
fi

if echo "$OUTPUT" | grep -q "Sessions: 1"; then
	test_result 0 "Per-type session count correct"
else
	test_result 1 "Per-type session count incorrect"
fi

#
# Test 5: Create multiple sessions with different types
#
echo ""
echo "Test 5: Create multiple sessions with different agent types"
$TMUX_BIN -L $TMUX_SOCKET new-session -d -s dev1 -G development -o "Development work"
$TMUX_BIN -L $TMUX_SOCKET new-session -d -s dev2 -G development -o "More dev work"
$TMUX_BIN -L $TMUX_SOCKET new-session -d -s testing -G qa -o "Quality assurance"
sleep 1

OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -s 2>&1)
echo "$OUTPUT"

if echo "$OUTPUT" | grep -q "Sessions: 5 total"; then
	test_result 0 "Multiple sessions tracked"
else
	test_result 1 "Session count incorrect after multiple creates"
fi

#
# Test 6: Full analytics report
#
echo ""
echo "Test 6: Generate full analytics report"
OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics 2>&1)
echo "$OUTPUT" | head -30

if echo "$OUTPUT" | grep -q "Agent Analytics Report"; then
	test_result 0 "Full analytics report generated"
else
	test_result 1 "Full analytics report failed"
fi

if echo "$OUTPUT" | grep -q "Per-Type Analytics"; then
	test_result 0 "Per-type analytics included in full report"
else
	test_result 1 "Per-type analytics missing from full report"
fi

#
# Test 7: Kill one session and check analytics
#
echo ""
echo "Test 7: Kill session and verify analytics update"
$TMUX_BIN -L $TMUX_SOCKET kill-session -t research
sleep 1

OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -s 2>&1)
echo "$OUTPUT"

ACTIVE=$(echo "$OUTPUT" | grep "active" | sed 's/.*(\([0-9]*\) active).*/\1/')
if [ "$ACTIVE" -eq 4 ]; then
	test_result 0 "Active session count decreased after kill (4 active)"
else
	test_result 1 "Active session count not updated after kill (got $ACTIVE active)"
fi

TOTAL=$( echo "$OUTPUT" | grep "Sessions:" | sed 's/.*Sessions: \([0-9]*\).*/\1/')
ACTIVE=$(echo "$OUTPUT" | grep "active" | sed 's/.*(\([0-9]*\) active).*/\1/')
ENDED=$((TOTAL - ACTIVE))
if [ "$ENDED" -ge 1 ]; then
	test_result 0 "Session end recorded ($ENDED sessions ended)"
else
	test_result 1 "Session end not recorded (total=$TOTAL active=$ACTIVE)"
fi

#
# Test 8: Analytics persistence (across command invocations)
#
echo ""
echo "Test 8: Verify analytics persist across command calls"
OUTPUT1=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -s 2>&1)
OUTPUT2=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -s 2>&1)

if [ "$OUTPUT1" = "$OUTPUT2" ]; then
	test_result 0 "Analytics data persists across calls"
else
	test_result 1 "Analytics data not consistent"
fi

#
# Test 9: Kill all but one session (so server stays alive)
#
echo ""
echo "Test 9: Kill most sessions and check final state"
$TMUX_BIN -L $TMUX_SOCKET kill-session -t dev1
$TMUX_BIN -L $TMUX_SOCKET kill-session -t dev2
sleep 1

OUTPUT=$($TMUX_BIN -L $TMUX_SOCKET agent-analytics -s 2>&1)
echo "$OUTPUT"

ACTIVE=$(echo "$OUTPUT" | grep "active" | sed 's/.*(\([0-9]*\) active).*/\1/')
if [ "$ACTIVE" -le 2 ]; then
	test_result 0 "Sessions marked as ended ($ACTIVE still active)"
else
	test_result 1 "Active session count not decreased ($ACTIVE still active)"
fi

if echo "$OUTPUT" | grep -q "Sessions: 5 total"; then
	test_result 0 "Total session count preserved"
else
	test_result 1 "Total session count lost"
fi

# Kill last session (testing) and server
$TMUX_BIN -L $TMUX_SOCKET kill-session -t testing

# Clean up
echo ""
$TMUX_BIN -L $TMUX_SOCKET kill-server 2>/dev/null || true

# Summary
echo ""
echo "Test Summary"
echo "============"
echo "Total Tests: $TEST_COUNT"
echo "Passed: $PASS_COUNT"
echo "Failed: $FAIL_COUNT"
echo ""

if [ $FAIL_COUNT -eq 0 ]; then
	echo "${GREEN}All tests passed!${NC}"
	exit 0
else
	echo "${RED}Some tests failed.${NC}"
	exit 1
fi
