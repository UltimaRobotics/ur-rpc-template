# UR-RPC Normal Connection Clients - Setup Guide

## Quick Start

This guide provides step-by-step instructions to set up and test the bidirectional messaging clients.

### Prerequisites

1. **MQTT Broker**: Mosquitto must be installed and available
2. **Build Tools**: CMake 3.10+ and GCC compiler
3. **Development Environment**: Linux-based system (tested on Replit)

### Step 1: Build the Clients

```bash
# Navigate to the clients directory
cd "clients/normal connection clients"

# Clean and build everything
make clean && make all

# Verify build success
ls -la build/client_*
```

Expected output:
```
-rwxr-xr-x 1 user user 135376 client_a
-rwxr-xr-x 1 user user 139472 client_b
```

### Step 2: Start MQTT Broker

The system requires a local MQTT broker on port 1883:

```bash
# Start broker with provided configuration
mosquitto -c ../mosquitto.conf -v
```

You should see:
```
mosquitto version 2.0.18 starting
Config loaded from ../mosquitto.conf.
Opening ipv4 listen socket on port 1883.
mosquitto version 2.0.18 running
```

### Step 3: Test Individual Clients

**Test Client B (recommended first):**
```bash
cd build
timeout 10s ./client_b client_b_config.json
```

Expected output:
```
Client B starting with config: client_b_config.json
[INFO] Initializing UR-RPC framework
[INFO] UR-RPC framework initialized successfully
Client B connecting to broker...
[INFO] Connecting to MQTT broker localhost:1883
[INFO] Starting MQTT client loop
Client B is ready for bidirectional messaging
[Client B] Sent status update #0
[Client B] Sent status update #1
```

**Test Client A:**
```bash
timeout 10s ./client_a client_a_config.json
```

### Step 4: Monitor MQTT Traffic

In a separate terminal, monitor all client communication:

```bash
# Monitor all client topics
mosquitto_sub -h localhost -p 1883 -t "clients/+/+" -v
```

This will show all messages exchanged between clients in real-time.

### Step 5: Run Bidirectional Test

Use the provided test script for automated testing:

```bash
./test_bidirectional.sh
```

## Configuration Details

### Client A Configuration (`client_a_config.json`)
```json
{
  "client_id": "client_a_001",
  "broker_host": "localhost",
  "broker_port": 1883,
  "keepalive": 60,
  "clean_session": true,
  "qos": 1,
  "auto_reconnect": true
}
```

### Client B Configuration (`client_b_config.json`)
```json
{
  "client_id": "client_b_001", 
  "broker_host": "localhost",
  "broker_port": 1883,
  "keepalive": 60,
  "clean_session": true,
  "qos": 1,
  "auto_reconnect": true
}
```

## Expected Behavior

### Message Flow

1. **Client A → Client B:**
   - Ping messages every 5 seconds on `clients/client_a/messages`
   - RPC requests every 10 seconds on `clients/client_a/rpc/request/+`

2. **Client B → Client A:**
   - Pong responses on `clients/client_b/messages`
   - Status updates every 7.5 seconds on `clients/client_b/messages`
   - RPC responses on `clients/client_b/rpc/response/+`
   - RPC requests every 12.5 seconds on `clients/client_b/rpc/request/+`

### MQTT Topics Used

```
clients/client_a/messages         # Client A general messages
clients/client_a/rpc/request/+    # Client A RPC requests
clients/client_a/rpc/response/+   # Client A RPC responses

clients/client_b/messages         # Client B general messages  
clients/client_b/rpc/request/+    # Client B RPC requests
clients/client_b/rpc/response/+   # Client B RPC responses
```

## Troubleshooting

### Common Issues

1. **"Failed to create RPC client"**
   - Ensure MQTT broker is running on localhost:1883
   - Check configuration file format and location

2. **"Connection failed"**
   - Verify broker is accessible: `mosquitto_pub -h localhost -p 1883 -t test -m "hello"`
   - Check firewall settings

3. **No message exchange**
   - Use `mosquitto_sub -h localhost -t "clients/+/+"` to monitor traffic
   - Verify both clients are connecting to the same broker

4. **Build failures**
   - Ensure all dependencies are available
   - Check CMake version: `cmake --version` (requires 3.10+)

### Debug Commands

```bash
# Test broker connectivity
mosquitto_pub -h localhost -p 1883 -t "test/topic" -m "test message"

# Monitor specific client
mosquitto_sub -h localhost -t "clients/client_a/+"

# Check if clients are connecting
tail -f /var/log/mosquitto/mosquitto.log  # if logging enabled
```

## Architecture Overview

### Static Linking
Both clients are statically linked with:
- ur-rpc-template library
- cJSON for JSON parsing
- mqtt-client for MQTT operations
- ur-logger-api for logging
- pthread for threading

### Thread Model
Each client runs multiple threads:
- Main thread: Application logic
- MQTT thread: Message processing
- Optional heartbeat thread

### Message Patterns
1. **Ping/Pong**: Simple bidirectional messaging
2. **RPC Request/Response**: Structured RPC calls with transaction IDs
3. **Status Updates**: Periodic status broadcasts
4. **Heartbeat**: Keep-alive functionality (configurable)

## Integration Notes

### With Larger Systems
- Clients can be integrated into larger applications
- Configuration supports multiple brokers via relay functionality
- SSL/TLS can be enabled by modifying configuration
- Topic patterns can be customized via topic configuration

### Performance Considerations
- Default QoS level 1 ensures message delivery
- Auto-reconnect handles network interruptions
- Configurable timeouts for different scenarios
- Statistics available via ur-rpc API

This setup demonstrates the full capabilities of the ur-rpc-template library for building robust MQTT-based RPC systems.