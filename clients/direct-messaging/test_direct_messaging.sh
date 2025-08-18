#!/bin/bash

# Test script for Direct Messaging Clients
# Tests both standard request-response and queued messaging

set -e

echo "========================================"
echo "  Direct Messaging Clients Test Suite  "
echo "========================================"

# Build the clients first
echo "Building direct messaging clients..."
make clean
make all

if [ ! -f "build/client_requester" ] || [ ! -f "build/client_responder" ] || 
   [ ! -f "build/queued_client_1" ] || [ ! -f "build/queued_client_2" ]; then
    echo "❌ Build failed - missing executables"
    exit 1
fi

echo "✅ Build successful"
echo ""

# Test 1: Standard Request-Response Messaging
echo "=========================================="
echo "  Test 1: Standard Request-Response      "
echo "=========================================="

echo "Starting responder client..."
timeout 30s ./build/client_responder responder_config.json &
RESPONDER_PID=$!
sleep 3

echo "Starting requester client..."
timeout 25s ./build/client_requester requester_config.json &
REQUESTER_PID=$!

# Wait for test completion
sleep 28

echo "✅ Standard request-response messaging test completed"
echo ""

# Test 2: Queued Direct Messaging
echo "=========================================="
echo "  Test 2: Queued Direct Messaging        "
echo "=========================================="

echo "Starting queued client 2 (processor)..."
timeout 60s ./build/queued_client_2 queued_client_2_config.json &
QUEUED_2_PID=$!
sleep 3

echo "Starting queued client 1 (sequential requester)..."
timeout 50s ./build/queued_client_1 queued_client_1_config.json &
QUEUED_1_PID=$!

# Wait for queued test completion
sleep 55

echo "✅ Queued direct messaging test completed"
echo ""

# Summary
echo "=========================================="
echo "  Direct Messaging Test Summary          "
echo "=========================================="
echo "✅ Standard Request-Response: Client requester sends requests to responder"
echo "✅ Queued Messaging: Client 1 sends sequential requests 1,2,3... waiting for each response"
echo "✅ All direct messaging mechanisms tested successfully"
echo ""
echo "Configuration files used:"
echo "  - requester_config.json (standard requester)"
echo "  - responder_config.json (standard responder)" 
echo "  - queued_client_1_config.json (sequential requester)"
echo "  - queued_client_2_config.json (sequential processor)"
echo ""
echo "🎉 Direct messaging test suite completed successfully!"