#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include "../../ur-rpc-template/ur-rpc-template.h"

static atomic_bool g_running = true;
static ur_rpc_client_t* g_client = NULL;
static int g_response_counter = 0;

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    atomic_store(&g_running, false);
}

void message_handler(const char* topic, const char* payload, size_t payload_len, void* user_data) {
    if (!topic || !payload) return;
    
    // Check if this is a request topic (should contain "requests")
    if (!strstr(topic, "requests")) {
        return; // Ignore non-request messages
    }
    
    g_response_counter++;
    printf("📨 [Responder] Received request #%d on topic: %s\n", g_response_counter, topic);
    
    // Parse the JSON request
    cJSON* json = cJSON_Parse(payload);
    if (!json) {
        printf("❌ [Responder] Failed to parse JSON request\n");
        return;
    }
    
    // Extract request details
    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* service = cJSON_GetObjectItem(json, "service");
    cJSON* transaction_id = cJSON_GetObjectItem(json, "transaction_id");
    cJSON* params = cJSON_GetObjectItem(json, "params");
    
    printf("🔍 [Responder] Method: '%s', Service: '%s', Transaction: '%s'\n", 
           method && cJSON_IsString(method) ? cJSON_GetStringValue(method) : "unknown",
           service && cJSON_IsString(service) ? cJSON_GetStringValue(service) : "unknown",
           transaction_id && cJSON_IsString(transaction_id) ? cJSON_GetStringValue(transaction_id) : "unknown");
    
    if (params) {
        char* params_str = cJSON_Print(params);
        if (params_str) {
            printf("📊 [Responder] Request params: %s\n", params_str);
            free(params_str);
        }
    }
    
    // Simulate processing time
    usleep(500000); // 500ms processing
    
    // Create response JSON
    cJSON* response = cJSON_CreateObject();
    if (transaction_id && cJSON_IsString(transaction_id)) {
        cJSON_AddStringToObject(response, "transaction_id", cJSON_GetStringValue(transaction_id));
    }
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddNumberToObject(response, "timestamp", time(NULL) * 1000);
    cJSON_AddNumberToObject(response, "processing_time_ms", 500);
    
    // Create result data
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "processed");
    cJSON_AddNumberToObject(result, "response_id", g_response_counter);
    if (method && cJSON_IsString(method)) {
        cJSON_AddStringToObject(result, "processed_method", cJSON_GetStringValue(method));
    }
    cJSON_AddNumberToObject(result, "processing_timestamp", time(NULL));
    
    // Echo back some request data if available
    if (params) {
        cJSON* req_number = cJSON_GetObjectItem(params, "request_number");
        if (cJSON_IsNumber(req_number)) {
            cJSON_AddNumberToObject(result, "processed_request_number", cJSON_GetNumberValue(req_number));
        }
    }
    
    cJSON_AddItemToObject(response, "result", result);
    
    // Generate response topic (replace "requests" with "responses")
    char response_topic[512];
    strncpy(response_topic, topic, sizeof(response_topic) - 1);
    response_topic[sizeof(response_topic) - 1] = '\0';
    
    char* req_pos = strstr(response_topic, "requests");
    if (req_pos) {
        strcpy(req_pos, "responses");
    }
    
    printf("✅ [Responder] Sending response #%d for transaction '%s'\n", 
           g_response_counter, 
           transaction_id && cJSON_IsString(transaction_id) ? cJSON_GetStringValue(transaction_id) : "unknown");
    
    // Send response
    char* response_str = cJSON_Print(response);
    if (response_str) {
        int send_result = ur_rpc_publish_message(g_client, response_topic, response_str, strlen(response_str));
        if (send_result != UR_RPC_SUCCESS) {
            printf("❌ [Responder] Failed to send response #%d (error: %d)\n", g_response_counter, send_result);
        } else {
            printf("📤 [Responder] Response #%d sent successfully to %s\n", g_response_counter, response_topic);
        }
        free(response_str);
    }
    
    // Cleanup
    cJSON_Delete(response);
    cJSON_Delete(json);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file.json>\n", argv[0]);
        return 1;
    }
    
    printf("Direct Messaging Responder starting with config: %s\n", argv[1]);
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize UR-RPC framework
    if (ur_rpc_init() != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to initialize UR-RPC framework\n");
        return 1;
    }
    
    // Create client configuration
    ur_rpc_client_config_t* config = ur_rpc_config_create();
    if (!config) {
        fprintf(stderr, "Failed to create client configuration\n");
        ur_rpc_cleanup();
        return 1;
    }
    
    // Load configuration from JSON file
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
    
    // Set topic configuration for receiving requests
    ur_rpc_topic_config_set_prefixes(topic_config, "direct_messaging", "responder");
    ur_rpc_topic_config_set_suffixes(topic_config, "requests", "responses", "notifications");
    
    // Create UR-RPC client
    g_client = ur_rpc_client_create(config, topic_config);
    if (!g_client) {
        fprintf(stderr, "Failed to create UR-RPC client\n");
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Set request handler
    ur_rpc_client_set_message_handler(g_client, message_handler, NULL);
    
    printf("Responder connecting to MQTT broker...\n");
    
    // Connect to MQTT broker
    if (ur_rpc_client_connect(g_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker\n");
        ur_rpc_client_destroy(g_client);
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Start client loop
    if (ur_rpc_client_start(g_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to start client loop\n");
        ur_rpc_client_destroy(g_client);
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Wait for connection
    printf("Waiting for MQTT connection...\n");
    for (int i = 0; i < 50 && !ur_rpc_client_is_connected(g_client); i++) {
        usleep(100000); // 100ms
    }
    
    if (!ur_rpc_client_is_connected(g_client)) {
        fprintf(stderr, "Failed to establish MQTT connection\n");
        ur_rpc_client_destroy(g_client);
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Subscribe to request topics
    char request_topic[256];
    snprintf(request_topic, sizeof(request_topic), "direct_messaging/responder/+/requests");
    
    int sub_result = ur_rpc_subscribe_topic(g_client, request_topic);
    if (sub_result != UR_RPC_SUCCESS) {
        printf("❌ [Responder] Failed to subscribe to request topics (error: %d)\n", sub_result);
        ur_rpc_client_destroy(g_client);
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    printf("🔗 [Responder] Connected to MQTT broker\n");
    printf("📡 [Responder] Subscribed to: %s\n", request_topic);
    printf("👂 [Responder] Waiting for direct messaging requests...\n");
    
    // Main loop - just wait for requests
    while (atomic_load(&g_running)) {
        sleep(1);
    }
    
    // Cleanup
    printf("Responder shutting down...\n");
    
    ur_rpc_client_stop(g_client);
    ur_rpc_client_disconnect(g_client);
    ur_rpc_client_destroy(g_client);
    ur_rpc_topic_config_destroy(topic_config);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("🔗 Direct Messaging Responder session completed\n");
    return 0;
}