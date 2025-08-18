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
static int g_processed_requests = 0;

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
    
    g_processed_requests++;
    printf("📨 [Queued Client 2] Received sequential request #%d on topic: %s\n", g_processed_requests, topic);
    
    // Parse the JSON request
    cJSON* json = cJSON_Parse(payload);
    if (!json) {
        printf("❌ [Queued Client 2] Failed to parse JSON request\n");
        return;
    }
    
    // Extract request details
    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* service = cJSON_GetObjectItem(json, "service");
    cJSON* transaction_id = cJSON_GetObjectItem(json, "transaction_id");
    cJSON* params = cJSON_GetObjectItem(json, "params");
    
    printf("🔍 [Queued Client 2] Method: '%s', Service: '%s', Transaction: '%s'\n", 
           method && cJSON_IsString(method) ? cJSON_GetStringValue(method) : "unknown",
           service && cJSON_IsString(service) ? cJSON_GetStringValue(service) : "unknown",
           transaction_id && cJSON_IsString(transaction_id) ? cJSON_GetStringValue(transaction_id) : "unknown");
    
    // Extract sequence information from parameters
    int sequence_number = -1;
    if (params) {
        char* params_str = cJSON_Print(params);
        if (params_str) {
            printf("📊 [Queued Client 2] Request params: %s\n", params_str);
            free(params_str);
        }
        
        cJSON* seq_num = cJSON_GetObjectItem(params, "sequence_number");
        if (cJSON_IsNumber(seq_num)) {
            sequence_number = seq_num->valueint;
            printf("🔢 [Queued Client 2] Processing sequence number: %d\n", sequence_number);
        }
    }
    
    // Simulate variable processing time based on sequence number
    int processing_time_ms = 300 + (sequence_number * 100); // Increasing processing time
    printf("⏳ [Queued Client 2] Processing request %d (simulated time: %dms)\n", 
           sequence_number, processing_time_ms);
    usleep(processing_time_ms * 1000);
    
    // Create response JSON
    cJSON* response = cJSON_CreateObject();
    if (transaction_id && cJSON_IsString(transaction_id)) {
        cJSON_AddStringToObject(response, "transaction_id", cJSON_GetStringValue(transaction_id));
    }
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddNumberToObject(response, "timestamp", ur_rpc_get_timestamp_ms());
    cJSON_AddNumberToObject(response, "processing_time_ms", (double)processing_time_ms);
    
    // Create detailed result data
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "processed_in_queue");
    cJSON_AddNumberToObject(result, "processed_sequence", sequence_number);
    cJSON_AddNumberToObject(result, "total_processed", g_processed_requests);
    if (method && cJSON_IsString(method)) {
        cJSON_AddStringToObject(result, "processed_method", cJSON_GetStringValue(method));
    }
    cJSON_AddNumberToObject(result, "processing_timestamp", time(NULL));
    cJSON_AddNumberToObject(result, "actual_processing_time_ms", processing_time_ms);
    
    // Add queue status information
    cJSON_AddStringToObject(result, "queue_status", "sequential_processing");
    cJSON_AddBoolToObject(result, "ready_for_next", true);
    
    cJSON_AddItemToObject(response, "result", result);
    
    // Generate response topic (replace "requests" with "responses")
    char response_topic[512];
    strncpy(response_topic, topic, sizeof(response_topic) - 1);
    response_topic[sizeof(response_topic) - 1] = '\0';
    
    char* req_pos = strstr(response_topic, "requests");
    if (req_pos) {
        strcpy(req_pos, "responses");
    }
    
    printf("✅ [Queued Client 2] Sending response for sequence %d (transaction: %s)\n", 
           sequence_number, 
           transaction_id && cJSON_IsString(transaction_id) ? cJSON_GetStringValue(transaction_id) : "unknown");
    
    // Send response
    char* response_str = cJSON_Print(response);
    if (response_str) {
        int send_result = ur_rpc_publish_message(g_client, response_topic, response_str, strlen(response_str));
        if (send_result != UR_RPC_SUCCESS) {
            printf("❌ [Queued Client 2] Failed to send response for sequence %d (error: %d)\n", 
                   sequence_number, send_result);
        } else {
            printf("📤 [Queued Client 2] Response sent for sequence %d - ready for next request\n", 
                   sequence_number);
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
    
    printf("Queued Direct Messaging Client 2 starting with config: %s\n", argv[1]);
    
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
    
    // Set topic configuration for receiving sequential requests
    ur_rpc_topic_config_set_prefixes(topic_config, "queued_messaging", "client_2");
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
    
    // Set message handler for processing sequential requests
    ur_rpc_client_set_message_handler(g_client, message_handler, NULL);
    
    printf("Queued Client 2 connecting to MQTT broker...\n");
    
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
    
    printf("🔗 [Queued Client 2] Connected to MQTT broker\n");
    printf("🔢 [Queued Client 2] Ready to process sequential requests in queue order...\n");
    
    // Main loop - wait for sequential requests
    while (atomic_load(&g_running)) {
        sleep(1);
    }
    
    // Cleanup
    printf("Queued Client 2 shutting down...\n");
    printf("📊 [Queued Client 2] Total sequential requests processed: %d\n", g_processed_requests);
    
    ur_rpc_client_stop(g_client);
    ur_rpc_client_disconnect(g_client);
    ur_rpc_client_destroy(g_client);
    ur_rpc_topic_config_destroy(topic_config);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("🔗 Queued Direct Messaging Client 2 session completed\n");
    return 0;
}