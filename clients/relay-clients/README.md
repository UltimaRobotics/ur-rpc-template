# Relay Clients Directory

This directory contains three specialized relay client implementations that extend the UR-RPC messaging system with multi-broker relay functionality.

## Overview

The relay clients enable message forwarding between different MQTT brokers with various features:

- **Basic Relay**: Simple message forwarding between brokers on different ports
- **SSL Relay**: Encrypted message forwarding between SSL-secured brokers
- **Conditional Relay**: Intelligent message filtering with conditional forwarding logic

## Components

### Relay Client Executables

- `basic_relay_client` - Forwards messages between regular MQTT brokers
- `ssl_relay_client` - Forwards messages between SSL/TLS secured brokers  
- `conditional_relay_client` - Applies filtering logic before forwarding messages

### Configuration Files

- `basic_relay_config.json` - Configuration for basic relay (ports 1883 → 1885)
- `ssl_relay_config.json` - Configuration for SSL relay (ports 1884 → 1886)
- `conditional_relay_config.json` - Configuration for conditional relay with filtering rules

### Python Verification Scripts

- `test_basic_relay.py` - Comprehensive test for basic relay functionality
- `test_ssl_relay.py` - SSL/TLS relay verification with encryption testing
- `test_conditional_relay.py` - Conditional logic and filtering verification
- `run_all_relay_tests.py` - Complete test suite for all relay types
- `quick_relay_demo.py` - Quick demonstration of relay client functionality

### Build System

- `CMakeLists.txt` - CMake configuration with OpenSSL support
- `Makefile` - Build wrapper with relay-specific targets
- `build/` - Build directory containing compiled executables and configs

## Quick Start

### 1. Build All Relay Clients

```bash
cd clients/relay-clients
make setup  # Setup build environment
make all    # Build all relay clients
```

### 2. Run Quick Demo

```bash
python3 quick_relay_demo.py
```

### 3. Run Comprehensive Tests

```bash
# Run individual tests
python3 test_basic_relay.py
python3 test_ssl_relay.py
python3 test_conditional_relay.py

# Or run complete test suite
python3 run_all_relay_tests.py
```

## Relay Types Explained

### Basic Relay Client

**Purpose**: Forward messages between brokers on different ports

**Features**:
- Message forwarding from port 1883 → 1885
- Topic pattern matching with wildcards
- Bidirectional relay support
- Simple configuration

**Use Case**: Connect separate broker networks or create backup message paths

**Example Topics**:
- `data/sensors/+` → `forwarded/sensors/+`
- `events/system/+` → `forwarded/system/+`
- `commands/+` ↔ `relayed/commands/+`

### SSL Relay Client

**Purpose**: Forward messages between SSL-secured brokers with encryption

**Features**:
- SSL/TLS encrypted connections (ports 1884 → 1886)
- Certificate validation
- Secure handshake verification
- Encrypted message transmission

**Use Case**: Bridge secure networks while maintaining encryption end-to-end

**Example Topics**:
- `secure/sensors/+` → `encrypted/sensors/+`
- `secure/events/+` → `encrypted/events/+`
- `secure/commands/+` → `encrypted/commands/+`

### Conditional Relay Client

**Purpose**: Apply intelligent filtering before forwarding messages

**Features**:
- Priority-based filtering (blocks low priority)
- Message type filtering (blocks debug messages)
- Timestamp validation (blocks old messages)
- Business hours logic
- Custom conditional rules

**Use Case**: Filter and route messages based on content, priority, or business rules

**Example Logic**:
- High priority messages → always forwarded
- Low priority messages → filtered out
- Debug messages → blocked
- Old messages (>5 minutes) → filtered
- Normal messages during business hours → forwarded

## Configuration Format

All relay clients use JSON configuration files with the following structure:

```json
{
  "brokers": [
    {
      "id": "source_broker",
      "host": "localhost", 
      "port": 1883,
      "tls": {
        "enabled": false
      }
    },
    {
      "id": "destination_broker",
      "host": "localhost",
      "port": 1885,
      "tls": {
        "enabled": false
      }
    }
  ],
  "relay_rules": [
    {
      "from_broker": "source_broker",
      "to_broker": "destination_broker",
      "source_topic_pattern": "data/sensors/+",
      "destination_topic_pattern": "forwarded/sensors/+",
      "qos": 1
    }
  ]
}
```

### SSL Configuration

For SSL relay clients, add TLS configuration:

```json
{
  "tls": {
    "enabled": true,
    "ca_cert": "../ssl_certs/ca.crt",
    "client_cert": "../ssl_certs/client.crt", 
    "client_key": "../ssl_certs/client.key",
    "insecure": false
  }
}
```

### Conditional Rules

For conditional relay, add filtering rules:

```json
{
  "conditional_rules": [
    {
      "condition_type": "priority_filter",
      "blocked_priorities": ["low"],
      "allowed_priorities": ["normal", "high"]
    },
    {
      "condition_type": "message_type_filter",
      "blocked_types": ["debug"],
      "allowed_types": ["info", "warning", "error"]
    },
    {
      "condition_type": "timestamp_filter",
      "max_age_seconds": 300
    }
  ]
}
```

## Testing and Verification

### Python Test Scripts

The Python verification scripts provide comprehensive testing:

1. **Automated broker setup** - Scripts start destination brokers automatically
2. **Message publishing** - Publishes test messages to source brokers
3. **Message verification** - Confirms messages are received at destinations
4. **Filtering validation** - Tests conditional logic for conditional relay
5. **SSL verification** - Tests encryption and certificate handling
6. **Results reporting** - Detailed test results and recommendations

### Test Requirements

- Python 3.7+
- paho-mqtt library (`pip install paho-mqtt` or use packager tool)
- mosquitto broker installed
- SSL certificates (for SSL relay testing)

### Test Execution

```bash
# Check dependencies
python3 run_all_relay_tests.py

# Individual tests with detailed output
python3 test_basic_relay.py
python3 test_ssl_relay.py  
python3 test_conditional_relay.py
```

## Troubleshooting

### Common Issues

1. **Connection refused**: Check if MQTT brokers are running on expected ports
2. **SSL handshake failed**: Verify SSL certificates are valid and accessible
3. **Messages not forwarded**: Check relay configuration and topic patterns
4. **Python test failures**: Ensure paho-mqtt library is installed correctly

### Debugging Steps

1. **Check broker logs**: Look for connection and message logs
2. **Verify configurations**: Ensure JSON files are valid and paths are correct
3. **Test connectivity**: Use mosquitto_pub/sub to test broker connections
4. **Review relay logs**: Check relay client output for error messages

### Log Files

- `mosquitto_1885.log` - Basic relay destination broker
- `ssl_mosquitto_1886.log` - SSL relay destination broker  
- `mosquitto_1887.log` - Conditional relay destination broker
- Test output files with timestamps for detailed debugging

## Integration with UR-RPC System

The relay clients extend the existing UR-RPC messaging capabilities:

- **Compatible with existing clients**: Direct messaging clients can use relay networks
- **Preserves message format**: JSON RPC messages remain intact through relay
- **Extends topic hierarchies**: Adds relay-specific topic prefixes and patterns
- **Maintains security**: SSL relay preserves encryption end-to-end

## Performance Considerations

- **Basic Relay**: Low latency, minimal processing overhead
- **SSL Relay**: Higher latency due to encryption, increased CPU usage
- **Conditional Relay**: Moderate latency due to message inspection and filtering

## Future Enhancements

- Multiple relay hops (relay chains)
- Load balancing across multiple destination brokers
- Message transformation during relay
- Advanced filtering with regex patterns
- Relay health monitoring and automatic failover

---

For more information about the UR-RPC system, see the main project documentation in `../../UR_RPC_TEMPLATE_DOCUMENTATION.md`.