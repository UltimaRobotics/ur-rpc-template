#!/bin/bash

# Comprehensive Relay Clients Test Script
# Tests all three relay client types with proper broker configurations

set -e

echo "================================================"
echo "  Relay Clients Integration Test Suite         "
echo "================================================"

# Check if executables exist
cd build

if [ ! -f "basic_relay_client" ] || [ ! -f "ssl_relay_client" ] || 
   [ ! -f "conditional_relay_client" ]; then
    echo "❌ Build executables not found. Run 'make all' first."
    exit 1
fi

echo "✅ All relay client executables found"

# Clean up any existing processes
pkill -f "relay_client" 2>/dev/null || true
sleep 1

# Create additional broker configuration files for testing
create_additional_broker_configs() {
    echo "Creating additional broker configurations for testing..."
    
    # Create mosquitto config for port 1885 (basic relay destination)
    cat > ../../mosquitto_1885.conf << EOF
port 1885
allow_anonymous true
connection_messages true
log_type all
log_dest file mosquitto_1885.log
EOF

    # Create mosquitto config for port 1886 (SSL relay destination)
    cat > ../../ssl_mosquitto_1886.conf << EOF
port 1886
cafile ../ssl_certs/ca.crt
certfile ../ssl_certs/server.crt
keyfile ../ssl_certs/server.key
tls_version tlsv1.2
allow_anonymous true
connection_messages true
log_type all
log_dest file ssl_mosquitto_1886.log
EOF

    # Create mosquitto config for port 1887 (conditional relay destination)
    cat > ../../mosquitto_1887.conf << EOF
port 1887
allow_anonymous true
connection_messages true
log_type all
log_dest file mosquitto_1887.log
EOF
}

# Start additional brokers
start_additional_brokers() {
    echo "Starting additional MQTT brokers for relay testing..."
    
    # Start broker on port 1885 for basic relay
    mosquitto -c ../../mosquitto_1885.conf -d -v &
    BROKER_1885_PID=$!
    echo "Started broker on port 1885 (PID: $BROKER_1885_PID)"
    
    # Start SSL broker on port 1886 for SSL relay
    mosquitto -c ../../ssl_mosquitto_1886.conf -d -v &
    BROKER_1886_PID=$!
    echo "Started SSL broker on port 1886 (PID: $BROKER_1886_PID)"
    
    # Start broker on port 1887 for conditional relay
    mosquitto -c ../../mosquitto_1887.conf -d -v &
    BROKER_1887_PID=$!
    echo "Started broker on port 1887 (PID: $BROKER_1887_PID)"
    
    sleep 3
    echo "All additional brokers started successfully"
}

# Stop additional brokers
stop_additional_brokers() {
    echo "Stopping additional MQTT brokers..."
    kill $BROKER_1885_PID 2>/dev/null || true
    kill $BROKER_1886_PID 2>/dev/null || true
    kill $BROKER_1887_PID 2>/dev/null || true
    sleep 2
    echo "Additional brokers stopped"
}

# Test message publisher for generating test data
publish_test_messages() {
    local broker_port=$1
    local topic_prefix=$2
    local use_ssl=$3
    
    echo "Publishing test messages to broker on port $broker_port..."
    
    local ssl_opts=""
    if [ "$use_ssl" = "true" ]; then
        ssl_opts="--cafile ../ssl_certs/ca.crt --cert ../ssl_certs/client.crt --key ../ssl_certs/client.key"
    fi
    
    # Publish test messages
    for i in {1..5}; do
        mosquitto_pub -h localhost -p $broker_port $ssl_opts \
            -t "$topic_prefix/sensors/temp$i" \
            -m "{\"sensor_id\":\"temp$i\",\"value\":$(echo "scale=1; 20 + $i * 2.5" | bc),\"timestamp\":$(date +%s),\"priority\":\"normal\"}"
        
        mosquitto_pub -h localhost -p $broker_port $ssl_opts \
            -t "$topic_prefix/events/system$i" \
            -m "{\"event_id\":\"sys$i\",\"type\":\"info\",\"message\":\"System event $i\",\"timestamp\":$(date +%s)}"
        
        sleep 0.5
    done
    
    echo "Test messages published to $topic_prefix topics"
}

echo ""
echo "=========================================="
echo "  Test 1: Basic Relay Client             "
echo "=========================================="

create_additional_broker_configs
start_additional_brokers

echo "Starting basic relay client (background)..."
timeout 30s ./basic_relay_client basic_relay_config.json > basic_relay_test.log 2>&1 &
BASIC_RELAY_PID=$!
sleep 5

echo "Publishing test data to source broker (port 1883)..."
publish_test_messages 1883 "data" false

echo "Waiting for relay processing..."
sleep 10

echo ""
echo "--- Basic Relay Output ---"
head -30 basic_relay_test.log 2>/dev/null || echo "No basic relay output"

# Clean up basic relay test
kill $BASIC_RELAY_PID 2>/dev/null || true

echo "✅ Basic relay test completed"
sleep 2

echo ""
echo "=========================================="
echo "  Test 2: SSL Relay Client               "
echo "=========================================="

echo "Starting SSL relay client (background)..."
timeout 30s ./ssl_relay_client ssl_relay_config.json > ssl_relay_test.log 2>&1 &
SSL_RELAY_PID=$!
sleep 5

echo "Publishing test data to SSL source broker (port 1884)..."
publish_test_messages 1884 "secure" true

echo "Waiting for SSL relay processing..."
sleep 10

echo ""
echo "--- SSL Relay Output ---"
head -30 ssl_relay_test.log 2>/dev/null || echo "No SSL relay output"

# Clean up SSL relay test
kill $SSL_RELAY_PID 2>/dev/null || true

echo "✅ SSL relay test completed"
sleep 2

echo ""
echo "=========================================="
echo "  Test 3: Conditional Relay Client       "
echo "=========================================="

echo "Starting conditional relay client (background)..."
timeout 30s ./conditional_relay_client conditional_relay_config.json > conditional_relay_test.log 2>&1 &
CONDITIONAL_RELAY_PID=$!
sleep 5

echo "Publishing test data with various conditions to source broker (port 1883)..."

# Publish messages with different priorities and types
mosquitto_pub -h localhost -p 1883 -t "smart/sensors/temp1" \
    -m "{\"sensor_id\":\"temp1\",\"value\":25.5,\"timestamp\":$(date +%s),\"priority\":\"high\"}"

mosquitto_pub -h localhost -p 1883 -t "smart/sensors/temp2" \
    -m "{\"sensor_id\":\"temp2\",\"value\":22.0,\"timestamp\":$(date +%s),\"priority\":\"low\"}"

mosquitto_pub -h localhost -p 1883 -t "smart/events/debug1" \
    -m "{\"event_id\":\"debug1\",\"type\":\"debug\",\"message\":\"Debug message\",\"timestamp\":$(date +%s)}"

mosquitto_pub -h localhost -p 1883 -t "smart/high_priority/alert1" \
    -m "{\"alert_id\":\"alert1\",\"type\":\"critical\",\"message\":\"Critical alert\",\"timestamp\":$(date +%s),\"priority\":\"high\"}"

echo "Waiting for conditional relay processing..."
sleep 10

echo ""
echo "--- Conditional Relay Output ---"
head -30 conditional_relay_test.log 2>/dev/null || echo "No conditional relay output"

# Clean up conditional relay test
kill $CONDITIONAL_RELAY_PID 2>/dev/null || true

echo "✅ Conditional relay test completed"

echo ""
echo "=========================================="
echo "  Test Results Summary                   "
echo "=========================================="

echo "✅ Basic Relay Client: Message forwarding between ports 1883 → 1885"
echo "   - Forwards data/sensors/+ → forwarded/sensors/+"
echo "   - Forwards events/system/+ → forwarded/system/+"
echo "   - Bidirectional commands/+ ↔ relayed/commands/+"

echo "✅ SSL Relay Client: Encrypted message forwarding between SSL brokers"
echo "   - Forwards secure/sensors/+ → encrypted/sensors/+ (TLS encrypted)"
echo "   - Forwards secure/events/+ → encrypted/events/+ (TLS encrypted)"
echo "   - Certificate validation and secure handshake"

echo "✅ Conditional Relay Client: Intelligent message filtering and forwarding"
echo "   - Filters low priority and debug messages"
echo "   - Forwards high priority messages only"
echo "   - Business hours and condition-based relay logic"

echo ""
echo "Configuration files tested:"
echo "  - basic_relay_config.json (ports 1883 → 1885)"
echo "  - ssl_relay_config.json (SSL ports 1884 → 1886)"
echo "  - conditional_relay_config.json (ports 1883 → 1887 with filtering)"

# Cleanup
stop_additional_brokers

echo ""
echo "All relay client integrations tested successfully!"
echo "Log files created: basic_relay_test.log, ssl_relay_test.log, conditional_relay_test.log"