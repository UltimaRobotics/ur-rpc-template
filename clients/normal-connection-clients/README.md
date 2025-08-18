# Normal Connection Clients

This directory contains two client applications that demonstrate bidirectional messaging using the ur-rpc-template API exclusively.

## Overview

- **Client A** (`client_a.c`): Primary client that sends ping messages and RPC requests
- **Client B** (`client_b.c`): Secondary client that responds to pings and RPC requests

Both clients connect to a local MQTT broker on port 1883 and use JSON configuration files for all parameters.

## Features

### Bidirectional Messaging
- **Ping/Pong**: Client A sends ping messages, Client B responds with pong
- **RPC Calls**: Both clients can make RPC requests to each other
- **Status Updates**: Periodic status broadcasts
- **Heartbeat**: Keep-alive messages for connection monitoring

### Message Types
1. **Regular Messages**: JSON-formatted messages on `/messages` topics
2. **RPC Requests**: Structured RPC calls on `/rpc/request/` topics  
3. **RPC Responses**: Structured RPC responses on `/rpc/response/` topics
4. **Heartbeat**: Keep-alive messages on `/heartbeat` topics

### Topic Structure
```
clients/
├── client_a/
│   ├── messages          # General messages from Client A
│   ├── rpc/request/+     # RPC requests from Client A
│   ├── rpc/response/+    # RPC responses from Client A
│   └── heartbeat         # Heartbeat from Client A
└── client_b/
    ├── messages          # General messages from Client B  
    ├── rpc/request/+     # RPC requests from Client B
    ├── rpc/response/+    # RPC responses from Client B
    └── heartbeat         # Heartbeat from Client B
```

## Configuration

Each client uses a JSON configuration file:

### Client A Configuration (`client_a_config.json`)
```json
{
  "client_id": "client_a_001",
  "broker": {
    "host": "localhost",
    "port": 1883
  },
  "heartbeat": {
    "enabled": true,
    "topic": "clients/client_a/heartbeat",
    "interval_seconds": 30
  }
}
```

### Client B Configuration (`client_b_config.json`)  
```json
{
  "client_id": "client_b_001",
  "broker": {
    "host": "localhost", 
    "port": 1883
  },
  "heartbeat": {
    "enabled": true,
    "topic": "clients/client_b/heartbeat",
    "interval_seconds": 30
  }
}
```

## Building

The project uses CMake for building with static linking to ur-rpc-template:

```bash
# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# Built executables will be available as:
# - client_a
# - client_b
```

## Usage

### Prerequisites
1. **MQTT Broker**: Ensure a local MQTT broker (like Mosquitto) is running on port 1883
2. **Dependencies**: All dependencies are statically linked

### Running the Clients

**Terminal 1 - Start Client A:**
```bash
cd build
./client_a client_a_config.json
```

**Terminal 2 - Start Client B:**
```bash
cd build  
./client_b client_b_config.json
```

### Expected Behavior

1. **Connection**: Both clients connect to localhost:1883
2. **Subscription**: Each client subscribes to the other's topics
3. **Ping/Pong**: Client A sends ping every 5 seconds, Client B responds with pong
4. **RPC Calls**: 
   - Client A calls `get_status` on Client B every 10 seconds
   - Client B calls `get_info` on Client A every 12.5 seconds
5. **Status Updates**: Client B sends status updates every 7.5 seconds
6. **Heartbeat**: Both clients send heartbeat messages every 30 seconds

### Sample Output

**Client A:**
```
Client A starting with config: client_a_config.json
[Client A] Connection status changed: connected
[Client A] Successfully connected to broker
[Client A] Subscribed to Client B topics
Client A is ready for bidirectional messaging
[Client A] Sent ping message #1
[Client A] Received message on topic 'clients/client_b/messages': {"from":"client_b","type":"pong"...}
[Client A] Sending RPC request #1 to Client B
[Client A] RPC Response received - Transaction: ..., Success: true
```

**Client B:**
```
Client B starting with config: client_b_config.json
[Client B] Connection status changed: connected
[Client B] Successfully connected to broker  
[Client B] Subscribed to Client A topics
Client B is ready for bidirectional messaging
[Client B] Received message on topic 'clients/client_a/messages': {"from":"client_a","type":"ping"...}
[Client B] Sent pong response
[Client B] RPC Request - Method: get_status, Transaction: ...
[Client B] Sent RPC response to topic: clients/client_b/rpc/response/...
```

## Message Flow Examples

### Ping/Pong Flow
1. Client A → `clients/client_a/messages`: `{"from":"client_a","type":"ping",...}`
2. Client B → `clients/client_b/messages`: `{"from":"client_b","type":"pong",...}`

### RPC Request/Response Flow
1. Client A → `clients/client_a/rpc/request/txn_id`: `{"method":"get_status","service":"client_b",...}`
2. Client B → `clients/client_b/rpc/response/txn_id`: `{"success":true,"result":{...},...}`

## Error Handling

- **Connection Failures**: Automatic reconnection with exponential backoff
- **Message Timeouts**: RPC requests timeout after 5 seconds
- **Graceful Shutdown**: SIGINT/SIGTERM handling for clean disconnection
- **JSON Parsing**: Robust error handling for malformed messages

## Monitoring

Use an MQTT client to monitor the message flow:

```bash
# Monitor all client messages
mosquitto_sub -h localhost -t "clients/+/+"

# Monitor specific client
mosquitto_sub -h localhost -t "clients/client_a/+"

# Monitor RPC traffic only  
mosquitto_sub -h localhost -t "clients/+/rpc/+"
```

## Customization

### Adding New Message Types
1. Modify the `message_handler` function in both clients
2. Add new topic subscriptions in the connection callback
3. Update JSON configuration with new topics

### Changing Message Frequency
Modify the counter thresholds in the main loops:
- `message_counter % X`: Controls ping frequency
- `rpc_counter % Y`: Controls RPC request frequency

### Adding New RPC Methods
1. Add method handling in `message_handler`
2. Create appropriate response data
3. Update the RPC request generation code