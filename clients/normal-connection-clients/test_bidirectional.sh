#!/bin/bash

# Test script for bidirectional messaging between clients
echo "Starting bidirectional messaging test..."
echo "======================================="

# Function to cleanup background processes
cleanup() {
    echo "Cleaning up background processes..."
    jobs -p | xargs -r kill
    wait
    echo "Cleanup complete"
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM EXIT

# Change to build directory
cd "$(dirname "$0")/build" || exit 1

# Check if executables exist
if [[ ! -f "client_a" || ! -f "client_b" ]]; then
    echo "Error: Client executables not found. Please run 'make all' first."
    exit 1
fi

# Check if config files exist
if [[ ! -f "client_a_config.json" || ! -f "client_b_config.json" ]]; then
    echo "Error: Configuration files not found."
    exit 1
fi

echo "Starting Client A in background..."
./client_a client_a_config.json &
CLIENT_A_PID=$!

echo "Waiting 2 seconds for Client A to initialize..."
sleep 2

echo "Starting Client B..."
./client_b client_b_config.json &
CLIENT_B_PID=$!

echo "Both clients started. Monitoring for 15 seconds..."
echo "Watch for bidirectional messaging between clients..."
echo ""

# Monitor for 15 seconds
sleep 15

echo ""
echo "Stopping clients..."
kill $CLIENT_A_PID $CLIENT_B_PID 2>/dev/null
wait $CLIENT_A_PID $CLIENT_B_PID 2>/dev/null

echo "Test completed!"
echo ""
echo "Expected behavior:"
echo "- Client A sends ping messages every 5 seconds"
echo "- Client B responds with pong messages"
echo "- Client A sends RPC requests every 10 seconds"
echo "- Client B sends status updates every 7.5 seconds"
echo "- Both clients should connect and exchange messages"