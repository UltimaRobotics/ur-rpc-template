
# UR-RPC Template: Request-Response & Queued Messaging Documentation

## Overview

The UR-RPC template provides two primary messaging patterns for client communication:

1. **Request-Response Pattern**: Direct asynchronous RPC calls with callbacks
2. **Queued Messaging Pattern**: Sequential processing with ordered request handling

This documentation covers implementation details, integration guides, and best practices for both patterns.

## Table of Contents

1. [Request-Response Pattern](#request-response-pattern)
2. [Queued Messaging Pattern](#queued-messaging-pattern)
3. [Integration Guide](#integration-guide)
4. [Configuration](#configuration)
5. [Testing Examples](#testing-examples)
6. [Best Practices](#best-practices)
7. [Troubleshooting](#troubleshooting)

---

## Request-Response Pattern

### Architecture Overview

The request-response pattern implements asynchronous RPC calls where:
- **Requester**: Sends RPC requests and handles responses via callbacks
- **Responder**: Listens for requests, processes them, and sends responses
- **Topics**: Uses structured MQTT topics for routing messages

### Core Components

#### 1. Request Structure (`ur_rpc_request_t`)

```c
typedef struct {
    char* transaction_id;      // Unique identifier
    char* method;              // RPC method name  
    char* service;             // Target service name
    ur_rpc_authority_t authority; // Permission level
    cJSON* params;             // Request parameters
    char* response_topic;      // Response routing topic
    uint64_t timestamp;        // Request timestamp
    int timeout_ms;            // Request timeout
} ur_rpc_request_t;
```

#### 2. Response Structure (`ur_rpc_response_t`)

```c
typedef struct {
    char* transaction_id;      // Matches request ID
    bool success;              // Operation result
    cJSON* result;             // Response data
    char* error_message;       // Error description
    int error_code;            // Error code
    uint64_t timestamp;        // Response timestamp
    uint64_t processing_time_ms; // Processing duration
} ur_rpc_response_t;
```

### Implementation Example

#### Requester Implementation

```c
#include "../../ur-rpc-template/ur-rpc-template.h"

static ur_rpc_client_t* g_client = NULL;
static int g_request_counter = 0;

// Response callback function
void response_handler(const ur_rpc_response_t* response, void* user_data) {
    if (!response) return;
    
    printf("📋 [Requester] Response received: %s\n", 
           response->transaction_id ? response->transaction_id : "unknown");
    
    if (response->success) {
        printf("✅ Success: processing_time=%.2fms\n", 
               response->processing_time_ms);
        
        if (response->result) {
            char* result_str = cJSON_Print(response->result);
            if (result_str) {
                printf("📊 Response data: %s\n", result_str);
                free(result_str);
            }
        }
    } else {
        printf("❌ Failed: %s (code: %d)\n", 
               response->error_message ? response->error_message : "Unknown error",
               response->error_code);
    }
}

// Send RPC request
int send_rpc_request(const char* method, const char* service, cJSON* params) {
    ur_rpc_request_t* request = ur_rpc_request_create();
    if (!request) return UR_RPC_ERROR_MEMORY;
    
    // Configure request
    char transaction_id[64];
    snprintf(transaction_id, sizeof(transaction_id), "req_%d_%ld", 
             ++g_request_counter, time(NULL));
    
    ur_rpc_request_set_method(request, method, service);
    if (request->transaction_id) free(request->transaction_id);
    request->transaction_id = strdup(transaction_id);
    ur_rpc_request_set_timeout(request, 10000); // 10 seconds
    
    if (params) {
        ur_rpc_request_set_params(request, params);
    }
    
    printf("🔄 Sending request: method=%s, transaction=%s\n", 
           method, transaction_id);
    
    // Send asynchronous request
    int result = ur_rpc_call_async(g_client, request, response_handler, NULL);
    
    ur_rpc_request_destroy(request);
    return result;
}
```

#### Responder Implementation

```c
#include "../../ur-rpc-template/ur-rpc-template.h"

static ur_rpc_client_t* g_client = NULL;
static int g_response_counter = 0;

// Message handler for incoming requests
void message_handler(const char* topic, const char* payload, 
                    size_t payload_len, void* user_data) {
    if (!topic || !payload || !strstr(topic, "requests")) {
        return; // Ignore non-request messages
    }
    
    g_response_counter++;
    printf("📨 [Responder] Request #%d received on: %s\n", 
           g_response_counter, topic);
    
    // Parse JSON request
    cJSON* json = cJSON_Parse(payload);
    if (!json) {
        printf("❌ Failed to parse JSON request\n");
        return;
    }
    
    // Extract request details
    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* service = cJSON_GetObjectItem(json, "service");
    cJSON* transaction_id = cJSON_GetObjectItem(json, "transaction_id");
    cJSON* params = cJSON_GetObjectItem(json, "params");
    
    printf("🔍 Method: '%s', Service: '%s', Transaction: '%s'\n", 
           method && cJSON_IsString(method) ? cJSON_GetStringValue(method) : "unknown",
           service && cJSON_IsString(service) ? cJSON_GetStringValue(service) : "unknown",
           transaction_id && cJSON_IsString(transaction_id) ? cJSON_GetStringValue(transaction_id) : "unknown");
    
    // Simulate processing
    usleep(500000); // 500ms processing time
    
    // Create response
    cJSON* response = cJSON_CreateObject();
    if (transaction_id && cJSON_IsString(transaction_id)) {
        cJSON_AddStringToObject(response, "transaction_id", 
                               cJSON_GetStringValue(transaction_id));
    }
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddNumberToObject(response, "timestamp", ur_rpc_get_timestamp_ms());
    cJSON_AddNumberToObject(response, "processing_time_ms", 500);
    
    // Create result data
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "processed");
    cJSON_AddNumberToObject(result, "response_id", g_response_counter);
    if (method && cJSON_IsString(method)) {
        cJSON_AddStringToObject(result, "processed_method", 
                               cJSON_GetStringValue(method));
    }
    cJSON_AddItemToObject(response, "result", result);
    
    // Generate response topic
    char response_topic[512];
    strncpy(response_topic, topic, sizeof(response_topic) - 1);
    response_topic[sizeof(response_topic) - 1] = '\0';
    
    char* req_pos = strstr(response_topic, "requests");
    if (req_pos) {
        strcpy(req_pos, "responses");
    }
    
    // Send response
    char* response_str = cJSON_Print(response);
    if (response_str) {
        int send_result = ur_rpc_publish_message(g_client, response_topic, 
                                               response_str, strlen(response_str));
        if (send_result == UR_RPC_SUCCESS) {
            printf("📤 Response #%d sent successfully\n", g_response_counter);
        } else {
            printf("❌ Failed to send response #%d (error: %d)\n", 
                   g_response_counter, send_result);
        }
        free(response_str);
    }
    
    cJSON_Delete(response);
    cJSON_Delete(json);
}
```

---

## Queued Messaging Pattern

### Architecture Overview

The queued messaging pattern provides:
- **Sequential Processing**: Requests processed in order (1, 2, 3...)
- **Response Waiting**: Sender waits for each response before sending next request
- **Ordered Delivery**: Ensures message ordering and prevents overload

### Key Characteristics

1. **Client 1 (Sender)**: Sends sequential requests and waits for responses
2. **Client 2 (Processor)**: Processes requests in order with variable processing time
3. **Flow Control**: Built-in backpressure mechanism
4. **Error Handling**: Continues processing even if individual requests fail

### Implementation Example

#### Queued Client 1 (Sender)

```c
#include "../../ur-rpc-template/ur-rpc-template.h"

static atomic_bool g_running = true;
static ur_rpc_client_t* g_client = NULL;
static int g_current_request = 0;
static atomic_bool g_waiting_for_response = false;
static atomic_bool g_response_received = false;
static atomic_bool g_last_response_success = false;

void response_handler(const ur_rpc_response_t* response, void* user_data) {
    if (!response) return;
    
    printf("📋 [Queued Client 1] Response for request %d (transaction: %s)\n", 
           g_current_request, response->transaction_id ? response->transaction_id : "unknown");
    
    if (response->success) {
        printf("✅ Request %d SUCCESS: processing_time=%.2fms\n", 
               g_current_request, response->processing_time_ms);
        
        if (response->result) {
            char* result_str = cJSON_Print(response->result);
            if (result_str) {
                printf("📊 Response data: %s\n", result_str);
                free(result_str);
            }
        }
        atomic_store(&g_last_response_success, true);
    } else {
        printf("❌ Request %d FAILED: %s (code: %d)\n", 
               g_current_request,
               response->error_message ? response->error_message : "Unknown error",
               response->error_code);
        atomic_store(&g_last_response_success, false);
    }
    
    atomic_store(&g_response_received, true);
    atomic_store(&g_waiting_for_response, false);
}

void* queued_messaging_thread(void* arg) {
    printf("🚀 [Queued Client 1] Starting sequential request processing...\n");
    
    while (atomic_load(&g_running) && ur_rpc_client_is_connected(g_client)) {
        if (atomic_load(&g_waiting_for_response)) {
            usleep(100000); // 100ms wait
            continue;
        }
        
        g_current_request++;
        
        // Create request
        ur_rpc_request_t* request = ur_rpc_request_create();
        if (!request) {
            printf("❌ Failed to create request %d\n", g_current_request);
            sleep(1);
            continue;
        }
        
        // Configure request
        char method_name[64];
        char transaction_id[64];
        snprintf(method_name, sizeof(method_name), "sequential_process_%d", 
                 g_current_request);
        snprintf(transaction_id, sizeof(transaction_id), "seq_%d_%ld", 
                 g_current_request, time(NULL));
        
        ur_rpc_request_set_method(request, method_name, "queued_service");
        if (request->transaction_id) free(request->transaction_id);
        request->transaction_id = strdup(transaction_id);
        ur_rpc_request_set_timeout(request, 15000); // 15 seconds
        
        // Add parameters
        cJSON* params = cJSON_CreateObject();
        cJSON_AddNumberToObject(params, "sequence_number", g_current_request);
        cJSON_AddStringToObject(params, "data_type", "sequential");
        cJSON_AddNumberToObject(params, "timestamp", time(NULL));
        cJSON_AddStringToObject(params, "client_id", "queued_client_1");
        ur_rpc_request_set_params(request, params);
        
        printf("📤 Sending sequential request %d (waiting for response)\n", 
               g_current_request);
        
        // Reset response flags
        atomic_store(&g_response_received, false);
        atomic_store(&g_waiting_for_response, true);
        atomic_store(&g_last_response_success, false);
        
        // Send request
        int result = ur_rpc_call_async(g_client, request, response_handler, NULL);
        if (result != UR_RPC_SUCCESS) {
            printf("❌ Failed to send request %d (error: %d)\n", 
                   g_current_request, result);
            atomic_store(&g_waiting_for_response, false);
        } else {
            printf("🔄 Request %d sent, waiting for response...\n", g_current_request);
        }
        
        ur_rpc_request_destroy(request);
        
        // Wait for response with timeout
        int timeout_counter = 0;
        const int max_timeout = 200; // 20 seconds
        
        while (atomic_load(&g_waiting_for_response) && 
               timeout_counter < max_timeout && 
               atomic_load(&g_running)) {
            usleep(100000); // 100ms
            timeout_counter++;
        }
        
        if (timeout_counter >= max_timeout) {
            printf("⏰ Request %d timed out, continuing to next\n", g_current_request);
            atomic_store(&g_waiting_for_response, false);
        } else if (atomic_load(&g_response_received)) {
            if (atomic_load(&g_last_response_success)) {
                printf("🎯 Request %d completed successfully\n", g_current_request);
            } else {
                printf("⚠️ Request %d failed but continuing\n", g_current_request);
            }
        }
        
        sleep(1); // Brief pause before next request
        
        // Stop after demo limit
        if (g_current_request >= 10) {
            printf("🏁 Completed 10 sequential requests, stopping\n");
            break;
        }
    }
    
    return NULL;
}
```

#### Queued Client 2 (Processor)

```c
#include "../../ur-rpc-template/ur-rpc-template.h"

static atomic_bool g_running = true;
static ur_rpc_client_t* g_client = NULL;
static int g_processed_requests = 0;

void message_handler(const char* topic, const char* payload, 
                    size_t payload_len, void* user_data) {
    if (!topic || !payload || !strstr(topic, "requests")) {
        return;
    }
    
    g_processed_requests++;
    printf("📨 [Queued Client 2] Sequential request #%d on: %s\n", 
           g_processed_requests, topic);
    
    // Parse JSON request
    cJSON* json = cJSON_Parse(payload);
    if (!json) {
        printf("❌ Failed to parse JSON request\n");
        return;
    }
    
    // Extract request details
    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* service = cJSON_GetObjectItem(json, "service");
    cJSON* transaction_id = cJSON_GetObjectItem(json, "transaction_id");
    cJSON* params = cJSON_GetObjectItem(json, "params");
    
    printf("🔍 Method: '%s', Service: '%s', Transaction: '%s'\n", 
           method && cJSON_IsString(method) ? cJSON_GetStringValue(method) : "unknown",
           service && cJSON_IsString(service) ? cJSON_GetStringValue(service) : "unknown",
           transaction_id && cJSON_IsString(transaction_id) ? cJSON_GetStringValue(transaction_id) : "unknown");
    
    // Extract sequence information
    int sequence_number = -1;
    if (params) {
        cJSON* seq_num = cJSON_GetObjectItem(params, "sequence_number");
        if (cJSON_IsNumber(seq_num)) {
            sequence_number = seq_num->valueint;
            printf("🔢 Processing sequence number: %d\n", sequence_number);
        }
    }
    
    // Variable processing time based on sequence
    int processing_time_ms = 300 + (sequence_number * 100);
    printf("⏳ Processing request %d (simulated time: %dms)\n", 
           sequence_number, processing_time_ms);
    usleep(processing_time_ms * 1000);
    
    // Create response
    cJSON* response = cJSON_CreateObject();
    if (transaction_id && cJSON_IsString(transaction_id)) {
        cJSON_AddStringToObject(response, "transaction_id", 
                               cJSON_GetStringValue(transaction_id));
    }
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddNumberToObject(response, "timestamp", ur_rpc_get_timestamp_ms());
    cJSON_AddNumberToObject(response, "processing_time_ms", (double)processing_time_ms);
    
    // Create result data
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "processed_in_queue");
    cJSON_AddNumberToObject(result, "processed_sequence", sequence_number);
    cJSON_AddNumberToObject(result, "total_processed", g_processed_requests);
    cJSON_AddStringToObject(result, "queue_status", "sequential_processing");
    cJSON_AddBoolToObject(result, "ready_for_next", true);
    cJSON_AddItemToObject(response, "result", result);
    
    // Generate response topic
    char response_topic[512];
    strncpy(response_topic, topic, sizeof(response_topic) - 1);
    response_topic[sizeof(response_topic) - 1] = '\0';
    
    char* req_pos = strstr(response_topic, "requests");
    if (req_pos) {
        strcpy(req_pos, "responses");
    }
    
    printf("✅ Sending response for sequence %d\n", sequence_number);
    
    // Send response
    char* response_str = cJSON_Print(response);
    if (response_str) {
        int send_result = ur_rpc_publish_message(g_client, response_topic, 
                                               response_str, strlen(response_str));
        if (send_result == UR_RPC_SUCCESS) {
            printf("📤 Response sent for sequence %d - ready for next\n", 
                   sequence_number);
        } else {
            printf("❌ Failed to send response for sequence %d (error: %d)\n", 
                   sequence_number, send_result);
        }
        free(response_str);
    }
    
    cJSON_Delete(response);
    cJSON_Delete(json);
}
```

---

## Integration Guide

### Step 1: Basic Client Setup

```c
#include "../../ur-rpc-template/ur-rpc-template.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file.json>\n", argv[0]);
        return 1;
    }
    
    // Initialize UR-RPC framework
    if (ur_rpc_init() != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to initialize UR-RPC framework\n");
        return 1;
    }
    
    // Create and load configuration
    ur_rpc_client_config_t* config = ur_rpc_config_create();
    if (!config) {
        fprintf(stderr, "Failed to create client configuration\n");
        ur_rpc_cleanup();
        return 1;
    }
    
    if (ur_rpc_config_load_from_file(config, argv[1]) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to load configuration from %s\n", argv[1]);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Create topic configuration
    ur_rpc_topic_config_t* topic_config = ur_rpc_topic_config_create();
    if (!topic_config) {
        fprintf(stderr, "Failed to create topic configuration\n");
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Configure topics for your service
    ur_rpc_topic_config_set_prefixes(topic_config, "your_service", "target_service");
    ur_rpc_topic_config_set_suffixes(topic_config, "requests", "responses", "notifications");
    
    // Create and configure client
    ur_rpc_client_t* client = ur_rpc_client_create(config, topic_config);
    if (!client) {
        fprintf(stderr, "Failed to create UR-RPC client\n");
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Set message handler (for responders)
    ur_rpc_client_set_message_handler(client, your_message_handler, NULL);
    
    // Connect and start
    if (ur_rpc_client_connect(client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker\n");
        // cleanup...
        return 1;
    }
    
    if (ur_rpc_client_start(client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to start client loop\n");
        // cleanup...
        return 1;
    }
    
    // Wait for connection
    for (int i = 0; i < 50 && !ur_rpc_client_is_connected(client); i++) {
        usleep(100000); // 100ms
    }
    
    if (!ur_rpc_client_is_connected(client)) {
        fprintf(stderr, "Failed to establish MQTT connection\n");
        // cleanup...
        return 1;
    }
    
    // Your application logic here...
    
    // Cleanup
    ur_rpc_client_stop(client);
    ur_rpc_client_disconnect(client);
    ur_rpc_client_destroy(client);
    ur_rpc_topic_config_destroy(topic_config);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    return 0;
}
```

### Step 2: Configuration Files

#### Basic Configuration Template

```json
{
    "client_id": "your_client_id",
    "broker_host": "localhost",
    "broker_port": 1883,
    "username": null,
    "password": null,
    "clean_session": true,
    "qos": 1,
    "keepalive": 60,
    "use_tls": false,
    "connect_timeout": 10,
    "message_timeout": 30,
    "auto_reconnect": true,
    "reconnect_delay_min": 1,
    "reconnect_delay_max": 60
}
```

#### Queued Messaging Configuration

```json
{
    "client_id": "queued_client_1",
    "broker_host": "localhost",
    "broker_port": 1883,
    "clean_session": true,
    "qos": 1,
    "keepalive": 60,
    "connect_timeout": 10,
    "message_timeout": 15,
    "auto_reconnect": true,
    "reconnect_delay_min": 1,
    "reconnect_delay_max": 10
}
```

### Step 3: Topic Configuration

```c
// For direct messaging
ur_rpc_topic_config_set_prefixes(topic_config, "direct_messaging", "responder");
ur_rpc_topic_config_set_suffixes(topic_config, "requests", "responses", "notifications");

// For queued messaging
ur_rpc_topic_config_set_prefixes(topic_config, "queued_messaging", "client_2");
ur_rpc_topic_config_set_suffixes(topic_config, "requests", "responses", "notifications");
```

### Step 4: Message Subscription (for Responders)

```c
// Subscribe to request topics
char request_topic[256];
snprintf(request_topic, sizeof(request_topic), "your_service/target_service/+/requests");

int sub_result = ur_rpc_subscribe_topic(client, request_topic);
if (sub_result != UR_RPC_SUCCESS) {
    printf("❌ Failed to subscribe to request topics (error: %d)\n", sub_result);
    return -1;
}

printf("📡 Subscribed to: %s\n", request_topic);
```

---

## Configuration

### MQTT Broker Configuration

For testing, use the provided mosquitto configuration:

```
# clients/mosquitto.conf
listener 1883 0.0.0.0
allow_anonymous true
log_dest stdout
log_type all
```

### Client Configuration Options

| Parameter | Description | Default | Required |
|-----------|-------------|---------|----------|
| `client_id` | Unique MQTT client identifier | - | Yes |
| `broker_host` | MQTT broker hostname | "localhost" | Yes |
| `broker_port` | MQTT broker port | 1883 | Yes |
| `qos` | Quality of Service level | 1 | No |
| `keepalive` | Connection keepalive interval | 60 | No |
| `clean_session` | Start with clean session | true | No |
| `connect_timeout` | Connection timeout (seconds) | 10 | No |
| `message_timeout` | Message timeout (seconds) | 30 | No |
| `auto_reconnect` | Enable automatic reconnection | true | No |

---

## Testing Examples

### Building and Running Tests

#### Request-Response Test

```bash
# Build
cd clients/direct-messaging
make all

# Terminal 1: Start responder
./build/client_responder responder_config.json

# Terminal 2: Start requester
./build/client_requester requester_config.json
```

#### Queued Messaging Test

```bash
# Terminal 1: Start processor
./build/queued_client_2 queued_client_2_config.json

# Terminal 2: Start sender
./build/queued_client_1 queued_client_1_config.json
```

### Expected Output

#### Request-Response Pattern
```
[Requester] Connected to MQTT broker
🚀 Starting direct messaging requests...
🔄 Sending request #1: method=process_data_1, transaction=req_1_1703123456
✅ Request #1 sent successfully
📋 Response received: req_1_1703123456
✅ Response successful: processing_time=500.00ms
📊 Response data: {"status":"processed","response_id":1}
```

#### Queued Messaging Pattern
```
[Queued Client 1] Connected to MQTT broker
🚀 Starting sequential request processing...
📤 Sending sequential request 1 (waiting for response)
🔄 Request 1 sent, waiting for response...
📋 Response for request 1 (transaction: seq_1_1703123456)
✅ Request 1 SUCCESS: processing_time=400.00ms
🎯 Request 1 completed successfully, proceeding to next
```

---

## Best Practices

### 1. Error Handling

```c
// Always check return values
int result = ur_rpc_call_async(client, request, response_handler, NULL);
if (result != UR_RPC_SUCCESS) {
    printf("❌ Failed to send request (error: %d): %s\n", 
           result, ur_rpc_error_string(result));
    // Handle error appropriately
}
```

### 2. Memory Management

```c
// Always destroy created objects
ur_rpc_request_t* request = ur_rpc_request_create();
// ... use request ...
ur_rpc_request_destroy(request); // Don't forget!

// Free JSON objects
cJSON* params = cJSON_CreateObject();
// ... use params ...
cJSON_Delete(params); // Always cleanup
```

### 3. Timeout Configuration

```c
// Set appropriate timeouts based on expected processing time
ur_rpc_request_set_timeout(request, 15000); // 15 seconds for complex operations
```

### 4. Topic Naming Conventions

- Use hierarchical topic structure: `service/target/method/type`
- Keep topic names short but descriptive
- Use consistent naming patterns across services

### 5. Connection Management

```c
// Always check connection status before sending
if (!ur_rpc_client_is_connected(client)) {
    printf("❌ Client not connected, cannot send request\n");
    return UR_RPC_ERROR_NOT_CONNECTED;
}
```

### 6. Threading Considerations

```c
// Use atomic variables for thread-safe communication
static atomic_bool g_waiting_for_response = false;
static atomic_bool g_response_received = false;

// Protect shared resources with mutexes when needed
pthread_mutex_t response_mutex = PTHREAD_MUTEX_INITIALIZER;
```

---

## Troubleshooting

### Common Issues

#### 1. Connection Failures

**Problem**: Client fails to connect to MQTT broker
```
Failed to connect to MQTT broker
```

**Solutions**:
- Verify broker is running: `mosquitto -c clients/mosquitto.conf -v`
- Check host/port configuration
- Verify network connectivity
- Check firewall settings

#### 2. Message Not Received

**Problem**: Requests sent but no responses received

**Solutions**:
- Verify topic configuration matches between sender/receiver
- Check subscription patterns
- Enable MQTT broker logging for debugging
- Verify message handler is properly set

#### 3. JSON Parsing Errors

**Problem**: Failed to parse JSON request/response

**Solutions**:
- Validate JSON format using online validators
- Check for null terminators in strings
- Verify cJSON library is properly linked
- Add debug logging for raw payloads

#### 4. Timeout Issues

**Problem**: Requests timing out consistently

**Solutions**:
- Increase timeout values
- Check network latency
- Verify responder is processing messages
- Monitor broker performance

### Debug Logging

Enable detailed logging for troubleshooting:

```c
// In your main function, enable debug logging
logger_init(LOG_DEBUG, LOG_FLAG_CONSOLE | LOG_FLAG_TIMESTAMP | LOG_FLAG_COLOR, NULL);
```

### MQTT Broker Monitoring

Monitor MQTT traffic:

```bash
# Subscribe to all topics
mosquitto_sub -h localhost -t "#" -v

# Monitor specific service
mosquitto_sub -h localhost -t "your_service/+/+/+" -v
```

---

## Advanced Features

### 1. Custom Authority Levels

```c
// Set different authority levels for requests
ur_rpc_request_set_authority(request, UR_RPC_AUTHORITY_ADMIN);
ur_rpc_request_set_authority(request, UR_RPC_AUTHORITY_USER);
ur_rpc_request_set_authority(request, UR_RPC_AUTHORITY_GUEST);
```

### 2. Statistics Monitoring

```c
// Get client statistics
ur_rpc_statistics_t stats;
if (ur_rpc_client_get_statistics(client, &stats) == UR_RPC_SUCCESS) {
    printf("Messages sent: %lu\n", stats.messages_sent);
    printf("Messages received: %lu\n", stats.messages_received);
    printf("Requests sent: %lu\n", stats.requests_sent);
    printf("Responses received: %lu\n", stats.responses_received);
    printf("Errors: %lu\n", stats.errors_count);
}
```

### 3. Connection Status Monitoring

```c
void connection_callback(ur_rpc_connection_status_t status, void* user_data) {
    printf("Connection status changed: %s\n", 
           ur_rpc_connection_status_to_string(status));
}

// Set connection callback
ur_rpc_client_set_connection_callback(client, connection_callback, NULL);
```

This comprehensive documentation provides everything needed to implement and integrate request-response and queued messaging functionality using the UR-RPC template library.
