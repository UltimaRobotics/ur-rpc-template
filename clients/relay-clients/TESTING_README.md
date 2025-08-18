# MQTT Relay Testing Framework

This directory contains a comprehensive testing framework for verifying the functionality of C relay binary implementations.

## Overview

The testing system is redesigned to properly test C relay binaries by separating publisher and subscriber concerns. Each relay type has dedicated Python scripts that work together to verify message forwarding functionality.

## Test Scripts Architecture

### Basic Relay Testing (Port 1883 → 1885)
- **`basic_relay_publisher.py`** - Publishes test messages to source broker (port 1883)
- **`basic_relay_subscriber.py`** - Subscribes to destination broker (port 1885) and verifies forwarding

### SSL Relay Testing (Port 1884 → 1886)  
- **`ssl_relay_publisher.py`** - Publishes encrypted messages to SSL source broker (port 1884)
- **`ssl_relay_subscriber.py`** - Subscribes to SSL destination broker (port 1886) and verifies encrypted forwarding

### Conditional Relay Testing (Port 1883 → 1887)
- **`conditional_relay_publisher.py`** - Publishes messages with various priorities/types to test filtering
- **`conditional_relay_subscriber.py`** - Subscribes to filtered destination broker (port 1887) and verifies filtering logic

### Test Coordination
- **`run_relay_tests.py`** - Comprehensive test runner that coordinates all relay tests

## Message Flow Verification

Each relay type verifies specific forwarding patterns based on configuration:

### Basic Relay (`basic_relay_config.json`)
```
data/sensors/+ → forwarded/sensors/+
events/system/+ → forwarded/system/+  
commands/+ → relayed/commands/+ (bidirectional)
```

### SSL Relay (`ssl_relay_config.json`)
```
secure/sensors/+ → encrypted/sensors/+
secure/events/+ → encrypted/events/+
secure/commands/+ → encrypted/commands/+ (bidirectional)
```

### Conditional Relay (`conditional_relay_config.json`)
```
smart/sensors/+ → filtered/sensors/+ (with filtering)
smart/events/+ → filtered/events/+ (with filtering)
smart/high_priority/+ → filtered/priority/+
smart/commands/+ → filtered/commands/+ (with filtering)
```

## Usage Examples

### Individual Relay Testing

**Basic Relay:**
```bash
# Terminal 1: Start the relay binary
./build/basic_relay_client build/basic_relay_config.json

# Terminal 2: Start subscriber
python3 basic_relay_subscriber.py --duration 30

# Terminal 3: Run publisher
python3 basic_relay_publisher.py
```

**SSL Relay:**
```bash
# Terminal 1: Start the SSL relay binary
./build/ssl_relay_client build/ssl_relay_config.json

# Terminal 2: Start SSL subscriber  
python3 ssl_relay_subscriber.py --duration 30

# Terminal 3: Run SSL publisher
python3 ssl_relay_publisher.py
```

**Conditional Relay:**
```bash
# Terminal 1: Start the conditional relay binary
./build/conditional_relay_client build/conditional_relay_config.json

# Terminal 2: Start conditional subscriber
python3 conditional_relay_subscriber.py --duration 40

# Terminal 3: Run conditional publisher
python3 conditional_relay_publisher.py
```

### Comprehensive Testing

**Run all tests automatically:**
```bash
python3 run_relay_tests.py --duration 30
```

**Run specific relay type:**
```bash
python3 run_relay_tests.py --test-type basic --duration 20
python3 run_relay_tests.py --test-type ssl --duration 25  
python3 run_relay_tests.py --test-type conditional --duration 40
```

## Publisher Script Features

Each publisher script includes:
- **Test message generation** based on relay configuration rules
- **Message tracking** with unique test IDs
- **Expected destination mapping** for verification
- **Continuous publishing mode** for stress testing
- **Verbose output** for debugging

Publisher Options:
```bash
python3 *_relay_publisher.py [OPTIONS]
  --host HOST          MQTT broker host (default: localhost)
  --port PORT          MQTT broker port  
  --verbose, -v        Verbose output
  --continuous, -c     Publish messages continuously
  --interval SECONDS   Interval between message sets
```

## Subscriber Script Features

Each subscriber script includes:
- **Topic pattern verification** based on relay rules
- **Message analysis** and correctness checking
- **Filtering logic validation** (conditional relay)
- **Test result tracking** and summary reporting
- **Real-time message display** with detailed analysis

Subscriber Options:
```bash
python3 *_relay_subscriber.py [OPTIONS]
  --host HOST          MQTT broker host (default: localhost)
  --port PORT          MQTT broker port
  --verbose, -v        Verbose output  
  --duration, -d SECS  Listen duration in seconds
```

## Test Validation Criteria

### Basic Relay Validation
- ✅ Messages forwarded with correct topic transformation
- ✅ Bidirectional relay working for commands
- ✅ Message content preserved during forwarding
- ✅ No message loss or duplication

### SSL Relay Validation  
- ✅ SSL/TLS connections established successfully
- ✅ Certificate validation working
- ✅ Encrypted messages forwarded correctly
- ✅ Message content preserved through encryption
- ✅ SSL topic transformations applied correctly

### Conditional Relay Validation
- ✅ High priority messages pass through filtering
- ❌ Low priority messages filtered out
- ❌ Debug type messages filtered out  
- ❌ Old messages (>5 min) filtered out
- ✅ Normal priority current messages pass through
- ✅ Filtering statistics accurate

## Prerequisites

### For Basic Relay Testing
- Local MQTT broker running on port 1883
- Destination broker will be started automatically on port 1885

### For SSL Relay Testing
- SSL certificates available at `../ssl_certs/`:
  - `ca.crt` (Certificate Authority)
  - `client.crt` (Client Certificate)
  - `client.key` (Client Private Key)
  - `server.crt` (Server Certificate)  
  - `server.key` (Server Private Key)
- SSL source broker on port 1884
- SSL destination broker will be started automatically on port 1886

### For Conditional Relay Testing
- Local MQTT broker running on port 1883
- Conditional destination broker will be started automatically on port 1887

## C Relay Binary Requirements

Ensure the C relay binaries are built and available:
```bash
make clean
make all
```

This should create:
- `build/basic_relay_client`
- `build/ssl_relay_client` 
- `build/conditional_relay_client`

## Configuration Files

The test scripts expect these configuration files to exist:
- `build/basic_relay_config.json`
- `build/ssl_relay_config.json`
- `build/conditional_relay_config.json`

## Test Output Analysis

### Success Indicators
- **Publisher**: "✅ Published successfully" for each message
- **Subscriber**: "📥 RELAY MESSAGE RECEIVED" for forwarded messages
- **Analysis**: "✅ CORRECT" relay status for proper forwarding
- **Summary**: "✅ ALL TESTS PASSED" in final results

### Failure Indicators  
- **Connection errors**: Check broker availability and configuration
- **SSL errors**: Verify certificate files and paths
- **No messages received**: Check if relay binary is running
- **Filtering violations**: Review conditional relay logic

## Troubleshooting

### Common Issues

**No messages received:**
1. Verify the C relay binary is running
2. Check source and destination brokers are accessible
3. Verify relay configuration files are correct
4. Check network connectivity between brokers

**SSL connection failures:**
1. Verify SSL certificate files exist and are readable
2. Check certificate validity and paths
3. Ensure SSL brokers are configured correctly
4. Verify TLS version compatibility

**Filtering not working:**
1. Check conditional relay binary implementation
2. Verify filtering logic in C code
3. Review conditional relay configuration
4. Test with various message priorities and types

### Debug Mode

Run any script with `--verbose` flag for detailed debugging:
```bash
python3 basic_relay_publisher.py --verbose
python3 ssl_relay_subscriber.py --verbose --duration 60
```

## Legacy Files

The following files are deprecated in favor of the new modular system:
- `test_basic_relay.py` → Use `basic_relay_publisher.py` + `basic_relay_subscriber.py`
- `test_ssl_relay.py` → Use `ssl_relay_publisher.py` + `ssl_relay_subscriber.py`
- `test_conditional_relay.py` → Use `conditional_relay_publisher.py` + `conditional_relay_subscriber.py`
- `run_all_relay_tests.py` → Use `run_relay_tests.py`

The new system provides better separation of concerns, more focused testing, and easier debugging of relay functionality.