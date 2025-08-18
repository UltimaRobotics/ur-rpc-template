# UR-RPC-Template Library Documentation

## Overview

The UR-RPC-Template is a comprehensive C library that provides MQTT-based Remote Procedure Call (RPC) functionality with advanced features including multi-broker relay support, heartbeat monitoring, and robust configuration management. The library is designed for distributed systems requiring reliable message passing between multiple MQTT brokers.

## Project Structure

```
ur-rpc-template/
├── ur-rpc-template.h        # Main header file with API definitions
├── ur-rpc-template.c        # Implementation file (2070 lines)
├── CMakeLists.txt          # CMake build configuration
├── Makefile               # Simple Makefile wrapper
└── deps/                  # Dependencies directory
    ├── cJSON/             # JSON parsing library
    ├── mqtt-client/       # MQTT client implementation
    └── ur-logger-api/     # Logging functionality
```

## Core Features

- **MQTT RPC Communication**: Request-response and notification patterns
- **Multi-Broker Relay**: Message forwarding between multiple MQTT brokers
- **Conditional Relay**: Connect to secondary brokers only when ready
- **Heartbeat Monitoring**: Keep-alive functionality with custom payloads
- **Thread-Safe Operations**: Full thread safety with atomic operations
- **SSL/TLS Support**: Secure communication with certificate validation
- **Statistics & Monitoring**: Comprehensive metrics collection
- **Configuration Management**: JSON-based configuration loading
- **Auto-Reconnection**: Robust connection management with backoff

---

## API Reference

### 1. Library Initialization

#### `int ur_rpc_init(void)`
Initializes the UR-RPC framework and underlying MQTT library.

**Returns:**
- `UR_RPC_SUCCESS` (0): Success
- `UR_RPC_ERROR_MQTT` (-3): MQTT library initialization failed

**Example:**
```c
if (ur_rpc_init() != UR_RPC_SUCCESS) {
    fprintf(stderr, "Failed to initialize UR-RPC framework\n");
    return -1;
}
```

#### `void ur_rpc_cleanup(void)`
Cleans up and releases all global resources.

**Example:**
```c
ur_rpc_cleanup();
```

#### `const char* ur_rpc_error_string(ur_rpc_error_t error)`
Converts error codes to human-readable strings.

---

### 2. Configuration Management

#### `ur_rpc_client_config_t* ur_rpc_config_create(void)`
Creates a new client configuration with default values.

**Returns:**
- Pointer to new configuration or `NULL` on failure

**Default Values:**
- Port: 1883
- Keepalive: 60 seconds
- QoS: 1
- Clean session: true
- Auto-reconnect: true

#### `void ur_rpc_config_destroy(ur_rpc_client_config_t* config)`
Destroys configuration and frees all associated memory.

#### `int ur_rpc_config_set_broker(ur_rpc_client_config_t* config, const char* host, int port)`
Sets the MQTT broker connection details.

**Parameters:**
- `config`: Configuration object
- `host`: Broker hostname/IP address
- `port`: Broker port (1-65535)

#### `int ur_rpc_config_set_credentials(ur_rpc_client_config_t* config, const char* username, const char* password)`
Sets MQTT authentication credentials.

#### `int ur_rpc_config_set_client_id(ur_rpc_client_config_t* config, const char* client_id)`
Sets the MQTT client identifier.

#### `int ur_rpc_config_set_tls(ur_rpc_client_config_t* config, const char* ca_file, const char* cert_file, const char* key_file)`
Configures SSL/TLS encryption.

**Parameters:**
- `ca_file`: CA certificate file path (required)
- `cert_file`: Client certificate file path (optional)
- `key_file`: Client private key file path (optional)

#### `int ur_rpc_config_load_from_file(ur_rpc_client_config_t* config, const char* filename)`
Loads configuration from JSON file.

**JSON Configuration Structure:**
```json
{
  "client_id": "example_client",
  "broker": {
    "host": "localhost",
    "port": 1883,
    "username": "user",
    "password": "pass"
  },
  "tls": {
    "enabled": true,
    "ca_file": "/path/to/ca.crt"
  },
  "heartbeat": {
    "enabled": true,
    "topic": "heartbeat/client",
    "interval_seconds": 30,
    "payload": "{\"status\":\"alive\"}"
  },
  "relay": {
    "enabled": true,
    "conditional_relay": true,
    "brokers": [
      {
        "host": "broker1.example.com",
        "port": 1883,
        "client_id": "relay_client_1",
        "is_primary": true
      }
    ],
    "rules": [
      {
        "source_topic": "sensor/+",
        "destination_topic": "relayed/sensor/+",
        "source_broker_index": 0,
        "dest_broker_index": 1,
        "bidirectional": false
      }
    ]
  }
}
```

---

### 3. Client Management

#### `ur_rpc_client_t* ur_rpc_client_create(const ur_rpc_client_config_t* config, const ur_rpc_topic_config_t* topic_config)`
Creates a new RPC client instance.

**Parameters:**
- `config`: Client configuration
- `topic_config`: Topic naming configuration (can be NULL for defaults)

#### `void ur_rpc_client_destroy(ur_rpc_client_t* client)`
Destroys client and frees all resources.

#### `int ur_rpc_client_connect(ur_rpc_client_t* client)`
Establishes connection to MQTT broker.

#### `int ur_rpc_client_disconnect(ur_rpc_client_t* client)`
Disconnects from MQTT broker.

#### `int ur_rpc_client_start(ur_rpc_client_t* client)`
Starts the client's message processing threads.

#### `int ur_rpc_client_stop(ur_rpc_client_t* client)`
Stops all client threads.

#### `bool ur_rpc_client_is_connected(const ur_rpc_client_t* client)`
Checks if client is currently connected.

#### `ur_rpc_connection_status_t ur_rpc_client_get_status(const ur_rpc_client_t* client)`
Gets current connection status.

**Status Values:**
- `UR_RPC_CONN_DISCONNECTED`: Not connected
- `UR_RPC_CONN_CONNECTING`: Connection in progress
- `UR_RPC_CONN_CONNECTED`: Successfully connected
- `UR_RPC_CONN_RECONNECTING`: Attempting to reconnect
- `UR_RPC_CONN_ERROR`: Connection error

---

### 4. Callback Management

#### `void ur_rpc_client_set_connection_callback(ur_rpc_client_t* client, ur_rpc_connection_callback_t callback, void* user_data)`
Sets callback for connection status changes.

**Callback Signature:**
```c
typedef void (*ur_rpc_connection_callback_t)(ur_rpc_connection_status_t status, void* user_data);
```

#### `void ur_rpc_client_set_message_handler(ur_rpc_client_t* client, ur_rpc_message_handler_t handler, void* user_data)`
Sets callback for incoming messages.

**Callback Signature:**
```c
typedef void (*ur_rpc_message_handler_t)(const char* topic, const char* payload, size_t payload_len, void* user_data);
```

---

### 5. RPC Operations

#### `ur_rpc_request_t* ur_rpc_request_create(void)`
Creates a new RPC request object.

#### `void ur_rpc_request_destroy(ur_rpc_request_t* request)`
Destroys RPC request and frees memory.

#### `int ur_rpc_request_set_method(ur_rpc_request_t* request, const char* method, const char* service)`
Sets the RPC method and target service.

#### `int ur_rpc_request_set_authority(ur_rpc_request_t* request, ur_rpc_authority_t authority)`
Sets the request authority level.

**Authority Levels:**
- `UR_RPC_AUTHORITY_ADMIN`: Administrator privileges
- `UR_RPC_AUTHORITY_USER`: Regular user privileges
- `UR_RPC_AUTHORITY_GUEST`: Guest privileges
- `UR_RPC_AUTHORITY_SYSTEM`: System-level privileges

#### `int ur_rpc_request_set_params(ur_rpc_request_t* request, const cJSON* params)`
Sets request parameters as JSON object.

#### `int ur_rpc_call_async(ur_rpc_client_t* client, const ur_rpc_request_t* request, ur_rpc_response_handler_t callback, void* user_data)`
Makes asynchronous RPC call.

**Callback Signature:**
```c
typedef void (*ur_rpc_response_handler_t)(const ur_rpc_response_t* response, void* user_data);
```

#### `int ur_rpc_call_sync(ur_rpc_client_t* client, const ur_rpc_request_t* request, ur_rpc_response_t** response, int timeout_ms)`
Makes synchronous RPC call with timeout.

#### `int ur_rpc_send_notification(ur_rpc_client_t* client, const char* method, const char* service, ur_rpc_authority_t authority, const cJSON* params)`
Sends fire-and-forget notification.

---

### 6. Topic Management

#### `int ur_rpc_publish_message(ur_rpc_client_t* client, const char* topic, const char* payload, size_t payload_len)`
Publishes raw message to topic.

#### `int ur_rpc_subscribe_topic(ur_rpc_client_t* client, const char* topic)`
Subscribes to MQTT topic.

#### `int ur_rpc_unsubscribe_topic(ur_rpc_client_t* client, const char* topic)`
Unsubscribes from MQTT topic.

#### `char* ur_rpc_generate_request_topic(const ur_rpc_client_t* client, const char* method, const char* service, const char* transaction_id)`
Generates standardized request topic.

#### `char* ur_rpc_generate_response_topic(const ur_rpc_client_t* client, const char* method, const char* service, const char* transaction_id)`
Generates standardized response topic.

#### `char* ur_rpc_generate_notification_topic(const ur_rpc_client_t* client, const char* method, const char* service)`
Generates standardized notification topic.

---

### 7. Heartbeat Management

#### `int ur_rpc_config_set_heartbeat(ur_rpc_client_config_t* config, const char* topic, int interval_seconds, const char* payload)`
Configures heartbeat functionality.

#### `int ur_rpc_heartbeat_start(ur_rpc_client_t* client)`
Starts heartbeat thread.

#### `int ur_rpc_heartbeat_stop(ur_rpc_client_t* client)`
Stops heartbeat thread.

---

### 8. Relay Functionality

#### `ur_rpc_relay_client_t* ur_rpc_relay_client_create(const ur_rpc_client_config_t* config)`
Creates multi-broker relay client.

#### `void ur_rpc_relay_client_destroy(ur_rpc_relay_client_t* relay_client)`
Destroys relay client.

#### `int ur_rpc_relay_client_start(ur_rpc_relay_client_t* relay_client)`
Starts relay functionality.

#### `int ur_rpc_relay_client_stop(ur_rpc_relay_client_t* relay_client)`
Stops relay functionality.

#### `int ur_rpc_relay_config_add_broker(ur_rpc_relay_config_t* config, const char* host, int port, const char* client_id, bool is_primary)`
Adds broker to relay configuration.

#### `int ur_rpc_relay_config_add_rule(ur_rpc_relay_config_t* config, const char* source_topic, const char* dest_topic, const char* prefix, int source_broker, int dest_broker, bool bidirectional)`
Adds message forwarding rule.

---

### 9. Conditional Relay Control

#### `int ur_rpc_relay_set_secondary_connection_ready(bool ready)`
Sets the global flag for secondary connection readiness.

#### `bool ur_rpc_relay_is_secondary_connection_ready(void)`
Checks if secondary connections are ready.

#### `int ur_rpc_relay_connect_secondary_brokers(ur_rpc_relay_client_t* relay_client)`
Connects to secondary brokers when ready flag is set.

---

### 10. Utilities and Transaction Management

#### `char* ur_rpc_generate_transaction_id(void)`
Generates unique UUID-style transaction ID.

**Format:** `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`

#### `bool ur_rpc_validate_transaction_id(const char* transaction_id)`
Validates transaction ID format.

#### `uint64_t ur_rpc_get_timestamp_ms(void)`
Gets current timestamp in milliseconds.

#### `void ur_rpc_free_string(char* str)`
Safely frees library-allocated strings.

---

### 11. JSON Utilities

#### `char* ur_rpc_request_to_json(const ur_rpc_request_t* request)`
Converts request object to JSON string.

#### `ur_rpc_request_t* ur_rpc_request_from_json(const char* json_str)`
Creates request object from JSON string.

#### `char* ur_rpc_response_to_json(const ur_rpc_response_t* response)`
Converts response object to JSON string.

#### `ur_rpc_response_t* ur_rpc_response_from_json(const char* json_str)`
Creates response object from JSON string.

---

### 12. Statistics and Monitoring

#### `int ur_rpc_client_get_statistics(const ur_rpc_client_t* client, ur_rpc_statistics_t* stats)`
Retrieves client statistics.

**Statistics Structure:**
```c
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t requests_sent;
    uint64_t responses_received;
    uint64_t notifications_sent;
    uint64_t errors_count;
    uint64_t connection_count;
    uint64_t uptime_seconds;
    time_t last_activity;
} ur_rpc_statistics_t;
```

#### `int ur_rpc_client_reset_statistics(ur_rpc_client_t* client)`
Resets all client statistics to zero.

---

## Data Structures

### Core Configuration Structures

#### `ur_rpc_client_config_t`
Main client configuration containing:
- Broker connection details
- Authentication credentials
- SSL/TLS settings
- Timeout configurations
- Auto-reconnect settings
- Heartbeat configuration
- Relay configuration
- Topic lists from JSON config

#### `ur_rpc_topic_config_t`
Topic naming convention configuration:
- Base prefix (e.g., "ur_rpc")
- Service-specific prefix
- Request/response/notification suffixes
- Transaction ID inclusion flag

#### `ur_rpc_relay_config_t`
Relay configuration containing:
- Multiple broker configurations
- Relay rules for message forwarding
- Conditional relay settings
- Default relay prefix

### Request/Response Structures

#### `ur_rpc_request_t`
RPC request containing:
- Transaction ID
- Method and service names
- Authority level
- Parameters (JSON object)
- Response topic
- Timestamp and timeout

#### `ur_rpc_response_t`
RPC response containing:
- Transaction ID (matching request)
- Success/failure flag
- Result data (JSON object)
- Error message and code
- Timestamps and processing time

### Client Structures

#### `ur_rpc_client_t`
Main client instance containing:
- MQTT connection handle
- Configuration and topic settings
- Thread management
- Connection status
- Message handlers
- Pending request tracking
- Statistics counters

#### `ur_rpc_relay_client_t`
Multi-broker relay client containing:
- Array of client connections
- Relay configuration
- Threading for relay operations
- Relay statistics
- Message handlers

---

## Constants and Limits

```c
#define UR_RPC_MAX_TOPIC_LENGTH 256
#define UR_RPC_MAX_PAYLOAD_LENGTH 4096
#define UR_RPC_MAX_CLIENT_ID_LENGTH 64
#define UR_RPC_MAX_TRANSACTION_ID_LENGTH 37
#define UR_RPC_DEFAULT_KEEPALIVE 60
#define UR_RPC_DEFAULT_QOS 1
#define UR_RPC_DEFAULT_TIMEOUT_MS 30000
#define UR_RPC_MAX_BROKERS 16
#define UR_RPC_MAX_PREFIX_LENGTH 128
#define UR_RPC_MAX_RELAY_RULES 32
```

---

## Error Handling

### Error Codes

```c
typedef enum {
    UR_RPC_SUCCESS = 0,                 // Operation successful
    UR_RPC_ERROR_INVALID_PARAM = -1,    // Invalid parameter passed
    UR_RPC_ERROR_MEMORY = -2,           // Memory allocation failed
    UR_RPC_ERROR_MQTT = -3,             // MQTT library error
    UR_RPC_ERROR_JSON = -4,             // JSON parsing error
    UR_RPC_ERROR_TIMEOUT = -5,          // Operation timed out
    UR_RPC_ERROR_NOT_CONNECTED = -6,    // Client not connected
    UR_RPC_ERROR_CONFIG = -7,           // Configuration error
    UR_RPC_ERROR_THREAD = -8            // Thread operation error
} ur_rpc_error_t;
```

### Best Practices

1. **Always check return values** for error codes
2. **Use `ur_rpc_error_string()`** to get human-readable error messages
3. **Ensure proper cleanup** by calling destroy functions
4. **Handle connection callbacks** for robust error recovery
5. **Set appropriate timeouts** for operations
6. **Use thread-safe operations** when accessing shared data

---

## Threading Model

The library uses multiple threads for different operations:

1. **Main Thread**: Application logic and API calls
2. **MQTT Thread**: Message processing and network I/O
3. **Heartbeat Thread**: Periodic keep-alive messages
4. **Relay Thread**: Message forwarding between brokers

All operations are thread-safe using atomic operations and mutexes.

---

## Dependencies

### External Libraries

1. **libmosquitto**: MQTT client library
2. **pthread**: POSIX threads for multi-threading
3. **Standard C libraries**: stdio, stdlib, string, unistd, time

### Included Dependencies

1. **cJSON**: JSON parsing and generation
2. **mqtt-client**: Custom MQTT client implementation
3. **ur-logger-api**: Logging functionality with levels and formatting

---

## Build System

### CMake Configuration
- C99 standard compliance
- Debug and Release configurations
- Static library output
- Automatic dependency linking

### Makefile Wrapper
- Simple build commands
- Clean and install targets
- Build directory management

---

## Usage Examples

### Basic Client Setup

```c
#include "ur-rpc-template.h"

int main() {
    // Initialize library
    if (ur_rpc_init() != UR_RPC_SUCCESS) {
        return -1;
    }
    
    // Create configuration
    ur_rpc_client_config_t* config = ur_rpc_config_create();
    ur_rpc_config_set_broker(config, "localhost", 1883);
    ur_rpc_config_set_client_id(config, "example_client");
    
    // Create client
    ur_rpc_client_t* client = ur_rpc_client_create(config, NULL);
    
    // Connect and start
    ur_rpc_client_connect(client);
    ur_rpc_client_start(client);
    
    // ... application logic ...
    
    // Cleanup
    ur_rpc_client_stop(client);
    ur_rpc_client_disconnect(client);
    ur_rpc_client_destroy(client);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    return 0;
}
```

### Making RPC Calls

```c
// Create request
ur_rpc_request_t* request = ur_rpc_request_create();
ur_rpc_request_set_method(request, "get_data", "sensor_service");
ur_rpc_request_set_authority(request, UR_RPC_AUTHORITY_USER);

// Set parameters
cJSON* params = cJSON_CreateObject();
cJSON_AddStringToObject(params, "sensor_id", "temp_01");
ur_rpc_request_set_params(request, params);

// Make asynchronous call
ur_rpc_call_async(client, request, response_handler, user_data);

// Cleanup
ur_rpc_request_destroy(request);
cJSON_Delete(params);
```

### Configuration from JSON

```c
ur_rpc_client_config_t* config = ur_rpc_config_create();
if (ur_rpc_config_load_from_file(config, "config.json") != UR_RPC_SUCCESS) {
    fprintf(stderr, "Failed to load configuration\n");
    return -1;
}
```

This comprehensive documentation covers all aspects of the UR-RPC-Template library, providing developers with the information needed to effectively use the library in their applications.