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
static int g_request_counter = 0;

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    atomic_store(&g_running, false);
}

void response_handler(const ur_rpc_response_t* response, void* user_data) {
    if (!response) return;
    
    printf("📋 [Requester] Received response to transaction '%s'\n", 
           response->transaction_id ? response->transaction_id : "unknown");
    
    if (response->success) {
        printf("✅ [Requester] Response successful: error_code=%d, processing_time=%.2fms\n", 
               response->error_code, response->processing_time_ms);
        
        if (response->result) {
            char* result_str = cJSON_Print(response->result);
            if (result_str) {
                printf("📊 [Requester] Response data: %s\n", result_str);
                free(result_str);
            }
        }
    } else {
        printf("❌ [Requester] Response failed: %s (code: %d)\n", 
               response->error_message ? response->error_message : "Unknown error",
               response->error_code);
    }
}

void* request_thread(void* arg) {
    printf("🚀 [Requester] Request thread starting...\n");
    
    while (atomic_load(&g_running) && ur_rpc_client_is_connected(g_client)) {
        // Create a request
        ur_rpc_request_t* request = ur_rpc_request_create();
        if (!request) {
            printf("❌ [Requester] Failed to create request\n");
            sleep(2);
            continue;
        }
        
        // Set request parameters
        char method_name[64];
        char transaction_id[64];
        snprintf(method_name, sizeof(method_name), "process_data_%d", ++g_request_counter);
        snprintf(transaction_id, sizeof(transaction_id), "req_%d_%ld", g_request_counter, time(NULL));
        
        ur_rpc_request_set_method(request, method_name, "data_service");
        if (request->transaction_id) free(request->transaction_id);
        request->transaction_id = strdup(transaction_id);
        ur_rpc_request_set_timeout(request, 10000); // 10 seconds
        
        // Add some parameters
        cJSON* params = cJSON_CreateObject();
        cJSON_AddNumberToObject(params, "request_number", g_request_counter);
        cJSON_AddStringToObject(params, "data", "sample_data");
        cJSON_AddNumberToObject(params, "timestamp", time(NULL));
        ur_rpc_request_set_params(request, params);
        
        printf("🔄 [Requester] Sending request #%d: method=%s, transaction=%s\n", 
               g_request_counter, method_name, transaction_id);
        
        // Send the request to the responder
        int result = ur_rpc_call_async(g_client, request, response_handler, NULL);
        if (result != UR_RPC_SUCCESS) {
            printf("❌ [Requester] Failed to send request #%d (error: %d)\n", g_request_counter, result);
        } else {
            printf("✅ [Requester] Request #%d sent successfully\n", g_request_counter);
        }
        
        ur_rpc_request_destroy(request);
        
        // Wait before sending next request
        sleep(3);
    }
    
    printf("🏁 [Requester] Request thread terminating\n");
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file.json>\n", argv[0]);
        return 1;
    }
    
    printf("Direct Messaging Requester starting with config: %s\n", argv[1]);
    
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
    
    // Set topic configuration for communication with responder
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
    
    printf("Requester connecting to MQTT broker...\n");
    
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
    
    printf("🔗 [Requester] Connected to MQTT broker\n");
    printf("🚀 [Requester] Starting direct messaging requests...\n");
    
    // Start request thread
    pthread_t request_thread_id;
    if (pthread_create(&request_thread_id, NULL, request_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create request thread\n");
        atomic_store(&g_running, false);
    }
    
    // Main loop
    while (atomic_load(&g_running)) {
        sleep(1);
    }
    
    // Cleanup
    printf("Requester shutting down...\n");
    
    if (request_thread_id) {
        pthread_join(request_thread_id, NULL);
    }
    
    ur_rpc_client_stop(g_client);
    ur_rpc_client_disconnect(g_client);
    ur_rpc_client_destroy(g_client);
    ur_rpc_topic_config_destroy(topic_config);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("🔗 Direct Messaging Requester session completed\n");
    return 0;
}