#!/bin/bash
#
# Example workflow using tmux agentic features
# This demonstrates a complete development workflow with agent-aware sessions
#

set -e

echo "=== Tmux Agentic Workflow Example ==="
echo
echo "This script demonstrates a typical development workflow using"
echo "tmux's agentic features to track progress across different phases."
echo

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
NC='\033[0m'

step() {
    echo -e "${BLUE}[Step $1]${NC} $2"
}

done_step() {
    echo -e "${GREEN}✓${NC} $1"
    echo
}

# Clean slate
tmux kill-server 2>/dev/null || true
sleep 1

# Step 1: Research Phase
step 1 "Research Phase - Investigating requirements"
tmux new-session -d -G research -o "Research authentication patterns" -s auth-research

# Simulate some work
sleep 1
tmux send-keys -t auth-research "echo 'Researching OAuth 2.0...'" C-m
tmux send-keys -t auth-research "echo 'Reviewing JWT best practices...'" C-m
sleep 1

done_step "Research session created and active"
tmux show-agent -t auth-research | grep -E "(Session|Agent Type|Goal)" | sed 's/^/  /'
echo

# Step 2: Design Phase
step 2 "Design Phase - Creating architecture"
tmux new-session -d -G development -o "Design authentication system" -s auth-design

# Create a multi-pane layout
tmux split-window -h -t auth-design
tmux split-window -v -t auth-design

# Simulate design work
tmux send-keys -t auth-design:0.0 "echo 'Designing database schema...'" C-m
tmux send-keys -t auth-design:0.1 "echo 'Creating API endpoints...'" C-m
tmux send-keys -t auth-design:0.2 "echo 'Planning middleware...'" C-m
sleep 1

done_step "Design session with 3-pane layout"
tmux show-agent -t auth-design | grep -E "(Session|Agent Type|Goal)" | sed 's/^/  /'
echo

# Step 3: Implementation Phase
step 3 "Implementation Phase - Building the feature"
tmux new-session -d -G development -o "Implement OAuth integration" -s auth-impl

# Simulate implementation
sleep 1
tmux send-keys -t auth-impl "echo 'Implementing OAuth client...'" C-m
tmux send-keys -t auth-impl "echo 'Adding JWT validation...'" C-m
sleep 1

done_step "Implementation session active"
tmux show-agent -t auth-impl | grep -E "(Session|Agent Type|Goal)" | sed 's/^/  /'
echo

# Step 4: Testing Phase
step 4 "Testing Phase - Validating implementation"
tmux new-session -d -G testing -o "Test authentication flows" -s auth-test

# Simulate testing
sleep 1
tmux send-keys -t auth-test "echo 'Running unit tests...'" C-m
tmux send-keys -t auth-test "echo 'Testing OAuth flow...'" C-m
tmux send-keys -t auth-test "echo 'Validating JWT tokens...'" C-m
sleep 1

done_step "Testing session running"
tmux show-agent -t auth-test | grep -E "(Session|Agent Type|Goal)" | sed 's/^/  /'
echo

# Step 5: Debugging Phase
step 5 "Debugging Phase - Fixing issues found in testing"
tmux new-session -d -G debugging -o "Fix token expiration bug" -s auth-debug

# Simulate debugging
sleep 1
tmux send-keys -t auth-debug "echo 'Analyzing token expiration issue...'" C-m
tmux send-keys -t auth-debug "echo 'Found bug in refresh logic...'" C-m
sleep 1

done_step "Debugging session investigating issue"
tmux show-agent -t auth-debug | grep -E "(Session|Agent Type|Goal)" | sed 's/^/  /'
echo

# Step 6: Documentation Phase
step 6 "Documentation Phase - Writing docs"
tmux new-session -d -G writing -o "Document authentication API" -s auth-docs

# Simulate documentation
sleep 1
tmux send-keys -t auth-docs "echo 'Writing API documentation...'" C-m
tmux send-keys -t auth-docs "echo 'Creating usage examples...'" C-m
sleep 1

done_step "Documentation session active"
tmux show-agent -t auth-docs | grep -E "(Session|Agent Type|Goal)" | sed 's/^/  /'
echo

# Summary
echo "=== Workflow Summary ==="
echo
echo "Active sessions with agent tracking:"
echo
tmux list-sessions | sed 's/^/  /'
echo
echo "You now have 6 sessions representing different phases of development:"
echo "  1. auth-research - Research phase"
echo "  2. auth-design - Design/architecture phase"
echo "  3. auth-impl - Implementation phase"
echo "  4. auth-test - Testing phase"
echo "  5. auth-debug - Debugging phase"
echo "  6. auth-docs - Documentation phase"
echo
echo "Each session tracks its purpose, goal, and progress."
echo
echo "To view agent info for any session:"
echo "  tmux show-agent -t <session-name>"
echo
echo "To switch between sessions:"
echo "  tmux switch-client -t <session-name>"
echo
echo "To attach and work:"
echo "  tmux attach -t <session-name>"
echo
echo "To clean up all test sessions:"
echo "  tmux kill-server"
echo
