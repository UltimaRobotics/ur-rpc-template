
# UR-RPC Template Relay Functionality Documentation

## Overview

The UR-RPC Template library provides comprehensive relay functionality that enables message forwarding between multiple MQTT brokers. This feature allows you to create complex messaging topologies, bridge networks, implement message filtering, and create fault-tolerant distributed systems.

## Table of Contents

1. [Core Relay Concepts](#core-relay-concepts)
2. [Relay Architecture](#relay-architecture)
3. [Configuration Structure](#configuration-structure)
4. [Implementation Guide](#implementation-guide)
5. [Integration Steps](#integration-steps)
6. [Relay Types](#relay-types)
7. [Configuration Examples](#configuration-examples)
8. [API Reference](#api-reference)
9. [Best Practices](#best-practices)
10. [Troubleshooting](#troubleshooting)

## Core Relay Concepts

### What is Relay Functionality?

Relay functionality in UR-RPC allows you to:
- **Forward messages** between different MQTT brokers
- **Bridge networks** across different ports, protocols, or security domains
- **Filter and transform** messages during relay
- **Create redundant paths** for message delivery
- **Implement conditional forwarding** based on content or system state

### Key Components

1. **Relay Client** (`ur_rpc_relay_client_t`) - Multi-broker client manager
2. **Broker Configuration** (`ur_rpc_broker_config_t`) - Individual broker settings
3. **Relay Rules** (`ur_rpc_relay_rule_t`) - Message forwarding rules
4. **Conditional Logic** - Smart forwarding based on conditions

## Relay Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Source MQTT   │    │   Relay Client  │    │ Destination MQTT│
│   Broker        │◄──►│                 │◄──►│   Broker        │
│   (Port 1883)   │    │  - Rule Engine  │    │   (Port 1885)   │
│                 │    │  - Filtering    │    │                 │
└─────────────────┘    │  - Transform    │    └─────────────────┘
                       │  - Conditional  │
                       └─────────────────┘
```

### Message Flow

1. **Subscribe** to source topics on source broker
2. **Receive** messages from source broker
3. **Apply** relay rules and filtering logic
4. **Transform** topic and payload if needed
5. **Publish** to destination broker
6. **Handle** bidirectional forwarding if configured

## Configuration Structure

### Relay Configuration

```c
typedef struct {
    bool enabled;                                    // Enable/disable relay functionality
    bool conditional_relay;                         // Enable conditional relay
    ur_rpc_broker_config_t brokers[UR_RPC_MAX_BROKERS]; // Broker configurations
    int broker_count;                               // Number of configured brokers
    ur_rpc_relay_rule_t rules[UR_RPC_MAX_RELAY_RULES]; // Relay rules
    int rule_count;                                 // Number of relay rules
    char* relay_prefix;                            // Default prefix for relayed messages
} ur_rpc_relay_config_t;
```

### Broker Configuration

```c
typedef struct {
    char* host;                // Broker hostname
    int port;                  // Broker port
    char* username;            // Optional username
    char* password;            // Optional password
    char* client_id;           // Client ID for this broker
    bool use_tls;              // Enable TLS/SSL
    char* ca_file;             // CA certificate file
    bool is_primary;           // Primary broker flag
} ur_rpc_broker_config_t;
```

### Relay Rule Configuration

```c
typedef struct {
    char* source_topic;        // Source topic pattern (supports wildcards)
    char* destination_topic;   // Destination topic pattern
    char* topic_prefix;        // Prefix to add to destination topic
    int source_broker_index;   // Index of source broker
    int dest_broker_index;     // Index of destination broker
    bool bidirectional;        // Enable bidirectional forwarding
} ur_rpc_relay_rule_t;
```

## Implementation Guide

### Step 1: Include Headers

```c
#include "ur-rpc-template.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
```

### Step 2: Initialize UR-RPC Framework

```c
if (ur_rpc_init() != UR_RPC_SUCCESS) {
    fprintf(stderr, "Failed to initialize UR-RPC framework\n");
    return 1;
}
```

### Step 3: Create and Configure Client

```c
// Create client configuration
ur_rpc_client_config_t* config = ur_rpc_config_create();
if (!config) {
    fprintf(stderr, "Failed to create client configuration\n");
    return 1;
}

// Load configuration from JSON file
if (ur_rpc_config_load_from_file(config, "relay_config.json") != UR_RPC_SUCCESS) {
    fprintf(stderr, "Failed to load configuration\n");
    return 1;
}
```

### Step 4: Create Relay Client

```c
// Create relay client
ur_rpc_relay_client_t* relay_client = ur_rpc_relay_client_create(config);
if (!relay_client) {
    fprintf(stderr, "Failed to create relay client\n");
    return 1;
}
```

### Step 5: Start Relay Functionality

```c
// Start relay client
if (ur_rpc_relay_client_start(relay_client) != UR_RPC_SUCCESS) {
    fprintf(stderr, "Failed to start relay client\n");
    return 1;
}
```

### Step 6: Main Relay Loop

```c
// Main relay loop
while (running) {
    sleep(1);
    // Relay operates automatically in background
    // Add monitoring or statistics here if needed
}
```

### Step 7: Cleanup

```c
// Stop and cleanup
ur_rpc_relay_client_stop(relay_client);
ur_rpc_relay_client_destroy(relay_client);
ur_rpc_config_destroy(config);
ur_rpc_cleanup();
```

## Integration Steps

### 1. Update Your CMakeLists.txt

```cmake
# Add relay support to your CMakeLists.txt
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Link with UR-RPC template library
target_link_libraries(your_relay_client 
    ur-rpc-template 
    ${MOSQUITTO_LIBRARIES}
    cjson
    pthread
    ssl
    crypto
)
```

### 2. Create Configuration File

Create a JSON configuration file for your relay client:

```json
{
  "relay": {
    "enabled": true,
    "conditional_relay": false,
    "relay_prefix": "relayed/",
    "brokers": [
      {
        "host": "localhost",
        "port": 1883,
        "client_id": "source_broker_client",
        "is_primary": true,
        "username": null,
        "password": null,
        "use_tls": false
      },
      {
        "host": "localhost", 
        "port": 1885,
        "client_id": "dest_broker_client",
        "is_primary": false,
        "username": null,
        "password": null,
        "use_tls": false
      }
    ],
    "rules": [
      {
        "source_topic": "data/sensors/+",
        "destination_topic": "forwarded/sensors/+",
        "topic_prefix": "relayed/",
        "source_broker_index": 0,
        "dest_broker_index": 1,
        "bidirectional": false
      }
    ]
  }
}
```

### 3. Compile Your Relay Client

```bash
# Build your relay client
cd your_project_directory
make setup
make your_relay_client
```

## Relay Types

### 1. Basic Relay

**Purpose**: Simple message forwarding between brokers

**Features**:
- Direct message forwarding
- Topic pattern matching
- Configurable prefixes
- Bidirectional support

**Configuration Example**:
```json
{
  "relay": {
    "enabled": true,
    "conditional_relay": false,
    "rules": [
      {
        "source_topic": "data/+",
        "destination_topic": "forwarded/+",
        "source_broker_index": 0,
        "dest_broker_index": 1,
        "bidirectional": false
      }
    ]
  }
}
```

### 2. SSL Relay

**Purpose**: Secure message forwarding between SSL-enabled brokers

**Features**:
- TLS/SSL encryption
- Certificate validation
- Secure handshake
- End-to-end encryption

**Configuration Example**:
```json
{
  "relay": {
    "enabled": true,
    "brokers": [
      {
        "host": "localhost",
        "port": 1884,
        "use_tls": true,
        "ca_file": "ssl_certs/ca.crt",
        "client_cert": "ssl_certs/client.crt",
        "client_key": "ssl_certs/client.key"
      }
    ]
  }
}
```

### 3. Conditional Relay

**Purpose**: Smart message forwarding with filtering logic

**Features**:
- Message content filtering
- Priority-based forwarding
- Time-based conditions
- Custom filtering logic

**Implementation Example**:
```c
// Custom message filter function
bool should_relay_message(const char* topic, const char* message) {
    cJSON* json = cJSON_Parse(message);
    if (!json) return true;
    
    // Filter based on priority
    cJSON* priority = cJSON_GetObjectItemCaseSensitive(json, "priority");
    if (priority && cJSON_IsString(priority)) {
        const char* priority_str = cJSON_GetStringValue(priority);
        if (strcmp(priority_str, "low") == 0) {
            cJSON_Delete(json);
            return false; // Don't relay low priority messages
        }
    }
    
    cJSON_Delete(json);
    return true;
}
```

## Configuration Examples

### Multi-Broker Relay Configuration

```json
{
  "client_id": "multi_broker_relay",
  "broker_host": "localhost",
  "broker_port": 1883,
  "relay": {
    "enabled": true,
    "conditional_relay": true,
    "relay_prefix": "multi_relay/",
    "brokers": [
      {
        "host": "localhost",
        "port": 1883,
        "client_id": "primary_broker",
        "is_primary": true
      },
      {
        "host": "localhost",
        "port": 1885,
        "client_id": "secondary_broker_1",
        "is_primary": false
      },
      {
        "host": "localhost",
        "port": 1887,
        "client_id": "secondary_broker_2", 
        "is_primary": false
      }
    ],
    "rules": [
      {
        "source_topic": "sensors/+/data",
        "destination_topic": "processed/sensors/+/data",
        "topic_prefix": "relay1/",
        "source_broker_index": 0,
        "dest_broker_index": 1,
        "bidirectional": false
      },
      {
        "source_topic": "events/+",
        "destination_topic": "archived/events/+",
        "topic_prefix": "relay2/",
        "source_broker_index": 0,
        "dest_broker_index": 2,
        "bidirectional": true
      }
    ]
  }
}
```

### SSL Relay Configuration

```json
{
  "client_id": "ssl_relay_client",
  "relay": {
    "enabled": true,
    "brokers": [
      {
        "host": "localhost",
        "port": 1884,
        "client_id": "ssl_source",
        "is_primary": true,
        "use_tls": true,
        "ca_file": "ssl_certs/ca.crt",
        "client_cert": "ssl_certs/client.crt",
        "client_key": "ssl_certs/client.key"
      },
      {
        "host": "localhost",
        "port": 1886,
        "client_id": "ssl_dest",
        "is_primary": false,
        "use_tls": true,
        "ca_file": "ssl_certs/ca.crt",
        "client_cert": "ssl_certs/client.crt",
        "client_key": "ssl_certs/client.key"
      }
    ],
    "rules": [
      {
        "source_topic": "secure/+",
        "destination_topic": "encrypted/+",
        "source_broker_index": 0,
        "dest_broker_index": 1,
        "bidirectional": false
      }
    ]
  }
}
```

## API Reference

### Core Relay Functions

#### `ur_rpc_relay_client_t* ur_rpc_relay_client_create(const ur_rpc_client_config_t* config)`
Creates a new relay client with specified configuration.

**Parameters**:
- `config`: Client configuration with relay settings

**Returns**: Relay client instance or NULL on failure

#### `int ur_rpc_relay_client_start(ur_rpc_relay_client_t* relay_client)`
Starts relay functionality, connecting to all configured brokers.

**Parameters**:
- `relay_client`: Relay client instance

**Returns**: `UR_RPC_SUCCESS` on success, error code on failure

#### `int ur_rpc_relay_client_stop(ur_rpc_relay_client_t* relay_client)`
Stops relay functionality and disconnects from brokers.

**Parameters**:
- `relay_client`: Relay client instance

**Returns**: `UR_RPC_SUCCESS` on success, error code on failure

#### `void ur_rpc_relay_client_destroy(ur_rpc_relay_client_t* relay_client)`
Destroys relay client and frees all resources.

**Parameters**:
- `relay_client`: Relay client instance

### Configuration Functions

#### `int ur_rpc_relay_config_add_broker(ur_rpc_relay_config_t* config, const char* host, int port, const char* client_id, bool is_primary)`
Adds a broker to relay configuration.

**Parameters**:
- `config`: Relay configuration
- `host`: Broker hostname
- `port`: Broker port
- `client_id`: Client identifier
- `is_primary`: Primary broker flag

#### `int ur_rpc_relay_config_add_rule(ur_rpc_relay_config_t* config, const char* source_topic, const char* dest_topic, const char* prefix, int source_broker, int dest_broker, bool bidirectional)`
Adds a message forwarding rule.

**Parameters**:
- `config`: Relay configuration
- `source_topic`: Source topic pattern
- `dest_topic`: Destination topic pattern
- `prefix`: Topic prefix to add
- `source_broker`: Source broker index
- `dest_broker`: Destination broker index
- `bidirectional`: Enable bidirectional forwarding

#### `int ur_rpc_relay_config_set_prefix(ur_rpc_relay_config_t* config, const char* prefix)`
Sets default relay prefix.

**Parameters**:
- `config`: Relay configuration
- `prefix`: Default prefix string

### Conditional Relay Functions

#### `int ur_rpc_relay_set_secondary_connection_ready(bool ready)`
Sets global flag for secondary connection readiness.

**Parameters**:
- `ready`: Ready state flag

#### `bool ur_rpc_relay_is_secondary_connection_ready(void)`
Checks if secondary connections are ready.

**Returns**: True if secondary connections are ready

#### `int ur_rpc_relay_connect_secondary_brokers(ur_rpc_relay_client_t* relay_client)`
Connects to secondary brokers when ready flag is set.

**Parameters**:
- `relay_client`: Relay client instance

## Best Practices

### 1. Configuration Management

- **Use JSON configuration files** for complex relay setups
- **Separate configurations** for different environments (dev/prod)
- **Validate configurations** before starting relay clients
- **Use meaningful client IDs** for debugging

### 2. Topic Design

- **Use consistent topic hierarchies** across relayed systems
- **Apply topic prefixes** to avoid conflicts
- **Design for scalability** with wildcard patterns
- **Document topic mappings** for maintenance

### 3. Security Considerations

- **Use SSL/TLS** for production deployments
- **Validate certificates** in SSL configurations
- **Implement authentication** where required
- **Monitor relay traffic** for security issues

### 4. Performance Optimization

- **Limit relay rules** to essential forwarding only
- **Use efficient topic patterns** to reduce processing
- **Monitor relay performance** and adjust as needed
- **Implement connection pooling** for high-throughput scenarios

### 5. Error Handling

- **Implement robust error handling** in relay logic
- **Add retry mechanisms** for failed connections
- **Log relay activities** for debugging
- **Monitor relay health** continuously

### 6. Testing and Validation

- **Test relay configurations** in staging environment
- **Validate message forwarding** end-to-end
- **Test failover scenarios** with broker outages
- **Monitor relay metrics** in production

## Troubleshooting

### Common Issues

#### 1. Connection Failures

**Problem**: Relay client cannot connect to brokers

**Solutions**:
- Check broker availability and network connectivity
- Verify broker configuration (host, port, credentials)
- Check SSL certificate validity for TLS connections
- Review firewall and network security settings

#### 2. Messages Not Forwarded

**Problem**: Messages received but not relayed

**Solutions**:
- Verify relay rules and topic patterns
- Check broker connection status
- Review conditional relay logic
- Validate message format and content

#### 3. SSL/TLS Issues

**Problem**: SSL connections fail or disconnect

**Solutions**:
- Verify SSL certificate chain
- Check certificate expiration dates
- Validate CA certificate configuration
- Review TLS version compatibility

#### 4. Performance Issues

**Problem**: High latency or message loss

**Solutions**:
- Monitor relay client resource usage
- Optimize topic patterns and rules
- Check network bandwidth and latency
- Review broker performance metrics

### Debugging Tips

#### 1. Enable Verbose Logging

```c
// Enable detailed logging for debugging
ur_rpc_client_set_log_level(client, UR_RPC_LOG_DEBUG);
```

#### 2. Monitor Relay Statistics

```c
// Check relay statistics periodically
ur_rpc_statistics_t stats;
ur_rpc_client_get_statistics(relay_client, &stats);
printf("Messages relayed: %llu\n", stats.messages_sent);
printf("Relay errors: %llu\n", stats.errors_count);
```

#### 3. Test Configuration

```bash
# Test JSON configuration validity
python3 -m json.tool your_config.json
```

#### 4. Monitor MQTT Traffic

```bash
# Monitor MQTT traffic on source broker
mosquitto_sub -h localhost -p 1883 -t "#" -v

# Monitor MQTT traffic on destination broker  
mosquitto_sub -h localhost -p 1885 -t "#" -v
```

### Error Codes

- `UR_RPC_SUCCESS` (0): Operation successful
- `UR_RPC_ERROR_INVALID_PARAM` (-1): Invalid parameter
- `UR_RPC_ERROR_MEMORY` (-2): Memory allocation error
- `UR_RPC_ERROR_MQTT` (-3): MQTT operation error
- `UR_RPC_ERROR_JSON` (-4): JSON parsing error
- `UR_RPC_ERROR_TIMEOUT` (-5): Operation timeout
- `UR_RPC_ERROR_NOT_CONNECTED` (-6): Not connected to broker
- `UR_RPC_ERROR_CONFIG` (-7): Configuration error
- `UR_RPC_ERROR_THREAD` (-8): Thread operation error

## Example Applications

### Network Bridge

Use relay functionality to bridge separate network segments:

```c
// Bridge corporate network to IoT network
// Forward only specific sensor data
// Apply security filtering during relay
```

### Load Balancing

Distribute messages across multiple brokers:

```c
// Round-robin message distribution
// Failover to backup brokers
// Load monitoring and adjustment
```

### Message Archiving

Archive messages to separate brokers:

```c
// Forward all messages to archive broker
// Add timestamps and metadata
// Implement retention policies
```

### Content Filtering

Filter and transform messages during relay:

```c
// Filter by message priority
// Transform message format
// Add routing information
```

---

For complete examples and working implementations, see the relay client examples in the `clients/relay-clients/` directory.

For additional information about the UR-RPC template system, refer to the main documentation: `UR_RPC_TEMPLATE_DOCUMENTATION.md`
