# C++ Wrapper for UR-RPC Template

This directory contains a comprehensive C++ wrapper for the UR-RPC Template library, providing modern C++ interfaces for MQTT-based RPC communication.

## Overview

The C++ wrapper (`ur-rpc-template.hpp`) provides:
- **Modern C++ API** with RAII, smart pointers, and exception handling
- **Type-safe interfaces** using enums and strong typing
- **STL integration** with std::string, std::function, and containers
- **Exception-based error handling** instead of error codes
- **Automatic resource management** with destructors and smart pointers

## Features

### Core Wrapper Classes

- **`UrRpc::Library`** - RAII library initialization
- **`UrRpc::ClientConfig`** - Fluent configuration builder
- **`UrRpc::TopicConfig`** - Topic structure configuration  
- **`UrRpc::Client`** - Main RPC client with modern callbacks
- **`UrRpc::RelayClient`** - Multi-broker relay functionality
- **`UrRpc::JsonValue`** - Smart JSON wrapper with automatic cleanup

### Advanced Features

- **Bidirectional messaging** with ping/pong patterns
- **SSL/TLS support** with certificate management
- **Relay functionality** for multi-broker communication
- **Heartbeat monitoring** for connection health
- **Statistics collection** and performance monitoring
- **Async/sync RPC calls** with timeout handling
- **Service discovery** and capability announcement
- **Batch request processing** for efficiency

## Client Examples

### 1. Basic Client (`cpp_basic_client`)
Demonstrates fundamental RPC operations:
- Connection management with reconnection
- Message publishing and subscription  
- Synchronous and asynchronous RPC calls
- Notification sending
- Heartbeat monitoring
- Statistics reporting

```bash
cd build && ./cpp_basic_client cpp_basic_config.json
```

### 2. Bidirectional Client (`cpp_bidirectional_client`)
Shows two-way communication patterns:
- Ping/pong messaging between clients
- RPC request/response handling
- Message type routing
- Real-time data exchange
- Partner discovery and communication

```bash
# Run two instances for full bidirectional demo
cd build && ./cpp_bidirectional_client cpp_bidirectional_config.json cpp_client_a &
cd build && ./cpp_bidirectional_client cpp_bidirectional_config.json cpp_client_b &
```

### 3. SSL Client (`cpp_ssl_client`)
Demonstrates secure communication:
- SSL/TLS encrypted connections
- Certificate-based authentication
- Secure message exchange
- Encrypted RPC calls
- SSL connection health monitoring

```bash
cd build && ./cpp_ssl_client cpp_ssl_config.json
```

### 4. Relay Client (`cpp_relay_client`)
Multi-broker message forwarding:
- Primary and secondary broker connections
- Conditional relay activation
- Message routing between brokers
- Relay status monitoring
- Dynamic connection control

```bash
cd build && ./cpp_relay_client cpp_relay_config.json
```

### 5. Advanced Features (`cpp_advanced_features`)
Comprehensive demonstration:
- Service discovery and announcement
- Batch request processing
- Advanced notification system
- Method registration and handling
- Transaction management
- Performance metrics

```bash
cd build && ./cpp_advanced_features cpp_advanced_config.json
```

## Building

### Requirements
- C++17 compatible compiler
- CMake 3.16+
- libmosquitto development library
- OpenSSL development library
- ur-rpc-template library

### Build Steps

```bash
# Configure and build
make configure
make build

# Or build specific clients
make cpp_basic_client
make cpp_ssl_client

# Run tests
make test

# Clean build
make clean
```

### Manual CMake Build

```bash
mkdir build && cd build
cmake ..
make -j4
```

## Configuration

Each client uses JSON configuration files with these key settings:

```json
{
  "client_id": "unique_client_identifier",
  "broker_host": "localhost",
  "broker_port": 1883,
  "use_tls": false,
  "auto_reconnect": true,
  "heartbeat": {
    "enabled": true,
    "topic": "heartbeat_topic",
    "interval_seconds": 30
  }
}
```

### SSL Configuration

For SSL clients, add TLS settings:

```json
{
  "broker_port": 1884,
  "use_tls": true,
  "ca_file": "../../ssl_certs/ca.crt",
  "cert_file": "../../ssl_certs/client.crt", 
  "key_file": "../../ssl_certs/client.key",
  "tls_version": "tlsv1.2",
  "tls_insecure": false
}
```

## API Usage Examples

### Basic Client Setup

```cpp
#include "ur-rpc-template.hpp"
using namespace UrRpc;

// Initialize library
Library library;

// Configure client
ClientConfig config;
config.setBroker("localhost", 1883)
      .setClientId("my_client")
      .setReconnect(true, 5, 30);

// Configure topics
TopicConfig topicConfig;
topicConfig.setPrefixes("my_app", "service")
           .setSuffixes("req", "resp", "notify");

// Create client
Client client(config, topicConfig);

// Set message handler
client.setMessageHandler([](const std::string& topic, const std::string& payload) {
    std::cout << "Message: " << topic << " -> " << payload << std::endl;
});

// Connect and start
client.connect();
client.start();
```

### JSON Message Handling

```cpp
// Create JSON message
JsonValue message;
message.addString("type", "status");
message.addNumber("value", 42.5);
message.addBool("active", true);

// Publish message
client.publishMessage("my/topic", message.toString());

// Parse received message
JsonValue received(payload);
auto status = received.getString("type");
auto value = received.getNumber("value");
```

### RPC Request/Response

```cpp
// Create request
Request request;
request.setMethod("calculate", "math_service")
       .setAuthority(Authority::User)
       .setTimeout(5000);

JsonValue params;
params.addNumber("a", 10);
params.addNumber("b", 20);
request.setParams(params);

// Synchronous call
Response response = client.callSync(request, 10000);
if (response.isSuccess()) {
    std::cout << "Result: " << response.getResult().toString() << std::endl;
}

// Asynchronous call
client.callAsync(request, [](bool success, const JsonValue& result, 
                            const std::string& error, int code) {
    if (success) {
        std::cout << "Async result: " << result.toString() << std::endl;
    }
});
```

## Error Handling

The wrapper uses exceptions for error handling:

```cpp
try {
    Client client(config, topicConfig);
    client.connect();
} catch (const ConfigException& e) {
    std::cerr << "Configuration error: " << e.what() << std::endl;
} catch (const ConnectionException& e) {
    std::cerr << "Connection error: " << e.what() << std::endl;
} catch (const UrRpc::Exception& e) {
    std::cerr << "RPC error: " << e.what() << std::endl;
}
```

## Integration with Existing Code

The wrapper is designed for easy integration:

1. **Include single header**: Only `#include "ur-rpc-template.hpp"` needed
2. **No manual cleanup**: RAII handles all resource management
3. **STL compatibility**: Works with standard containers and algorithms
4. **Exception safety**: Strong exception safety guarantees
5. **Thread safety**: Underlying library thread safety preserved

## Performance Considerations

- **Zero-copy where possible**: Minimal string copying in hot paths
- **Efficient JSON handling**: Smart pointers avoid unnecessary allocations
- **Connection pooling**: Reuse connections for multiple operations
- **Batch operations**: Group related operations for efficiency

## Testing

The provided client examples serve as comprehensive tests:

1. **Functional testing**: Each client tests specific feature sets
2. **Integration testing**: Clients interact with multiple brokers
3. **Performance testing**: Statistics and timing measurements
4. **Error testing**: Graceful handling of connection failures

Run the test suite:

```bash
make test
```

## Troubleshooting

### Common Issues

1. **Compilation errors**: Ensure C++17 support and all dependencies installed
2. **Connection failures**: Check broker availability and network connectivity
3. **SSL errors**: Verify certificate paths and permissions
4. **Memory issues**: Use memory tools like valgrind for debugging

### Debug Mode

Enable debug output by setting environment variables:

```bash
export UR_RPC_DEBUG=1
export MQTT_DEBUG=1
```

## Contributing

The C++ wrapper follows modern C++ best practices:

- **RAII everywhere**: No manual resource management
- **const correctness**: Proper const usage throughout
- **Exception safety**: Strong and basic guarantees
- **STL conventions**: Familiar interfaces for C++ developers

For extending the wrapper:

1. Follow existing patterns for new functionality
2. Add appropriate exception handling
3. Update documentation and examples
4. Test with multiple compiler versions

## License

Same license as the UR-RPC Template project.