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
static int g_current_request = 0;
static atomic_bool g_waiting_for_response = false;
static atomic_bool g_response_received = false;
static atomic_bool g_last_response_success = false;

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    atomic_store(&g_running, false);
}

void response_handler(const ur_rpc_response_t* response, void* user_data) {
    if (!response) return;
    
    printf("📋 [Queued Client 1] Received response for request %d (transaction: %s)\n", 
           g_current_request, response->transaction_id ? response->transaction_id : "unknown");
    
    if (response->success) {
        printf("✅ [Queued Client 1] Request %d SUCCESS: processing_time=%.2fms\n", 
               g_current_request, response->processing_time_ms);
        
        if (response->result) {
            char* result_str = cJSON_Print(response->result);
            if (result_str) {
                printf("📊 [Queued Client 1] Response data: %s\n", result_str);
                free(result_str);
            }
        }
        atomic_store(&g_last_response_success, true);
    } else {
        printf("❌ [Queued Client 1] Request %d FAILED: %s (code: %d)\n", 
               g_current_request,
               response->error_message ? response->error_message : "Unknown error",
               response->error_code);
        atomic_store(&g_last_response_success, false);
    }
    
    // Signal that response was received
    atomic_store(&g_response_received, true);
    atomic_store(&g_waiting_for_response, false);
}

void* queued_messaging_thread(void* arg) {
    printf("🚀 [Queued Client 1] Starting sequential request processing...\n");
    
    while (atomic_load(&g_running) && ur_rpc_client_is_connected(g_client)) {
        if (atomic_load(&g_waiting_for_response)) {
            // Wait for response before sending next request
            usleep(100000); // 100ms
            continue;
        }
        
        g_current_request++;
        
        // Create request
        ur_rpc_request_t* request = ur_rpc_request_create();
        if (!request) {
            printf("❌ [Queued Client 1] Failed to create request %d\n", g_current_request);
            sleep(1);
            continue;
        }
        
        // Set request parameters
        char method_name[64];
        char transaction_id[64];
        snprintf(method_name, sizeof(method_name), "sequential_process_%d", g_current_request);
        snprintf(transaction_id, sizeof(transaction_id), "seq_%d_%ld", g_current_request, time(NULL));
        
        ur_rpc_request_set_method(request, method_name, "queued_service");
        if (request->transaction_id) free(request->transaction_id);
        request->transaction_id = strdup(transaction_id);
        ur_rpc_request_set_timeout(request, 15000); // 15 seconds
        
        // Add parameters with sequence information
        cJSON* params = cJSON_CreateObject();
        cJSON_AddNumberToObject(params, "sequence_number", g_current_request);
        cJSON_AddStringToObject(params, "data_type", "sequential");
        cJSON_AddNumberToObject(params, "timestamp", time(NULL));
        cJSON_AddStringToObject(params, "client_id", "queued_client_1");
        ur_rpc_request_set_params(request, params);
        
        printf("📤 [Queued Client 1] Sending sequential request %d (waiting for response before next)\n", 
               g_current_request);
        
        // Reset response flags
        atomic_store(&g_response_received, false);
        atomic_store(&g_waiting_for_response, true);
        atomic_store(&g_last_response_success, false);
        
        // Send the request
        int result = ur_rpc_call_async(g_client, request, response_handler, NULL);
        if (result != UR_RPC_SUCCESS) {
            printf("❌ [Queued Client 1] Failed to send request %d (error: %d)\n", g_current_request, result);
            atomic_store(&g_waiting_for_response, false);
        } else {
            printf("🔄 [Queued Client 1] Request %d sent, waiting for response...\n", g_current_request);
        }
        
        ur_rpc_request_destroy(request);
        
        // Wait for response with timeout
        int timeout_counter = 0;
        const int max_timeout = 200; // 20 seconds (100ms * 200)
        
        while (atomic_load(&g_waiting_for_response) && timeout_counter < max_timeout && atomic_load(&g_running)) {
            usleep(100000); // 100ms
            timeout_counter++;
        }
        
        if (timeout_counter >= max_timeout) {
            printf("⏰ [Queued Client 1] Request %d timed out, continuing to next request\n", g_current_request);
            atomic_store(&g_waiting_for_response, false);
        } else if (atomic_load(&g_response_received)) {
            if (atomic_load(&g_last_response_success)) {
                printf("🎯 [Queued Client 1] Request %d completed successfully, proceeding to next\n", g_current_request);
            } else {
                printf("⚠️ [Queued Client 1] Request %d failed but continuing to next\n", g_current_request);
            }
        }
        
        // Brief pause before next request
        sleep(1);
        
        // Stop after 10 requests for demo purposes
        if (g_current_request >= 10) {
            printf("🏁 [Queued Client 1] Completed 10 sequential requests, stopping\n");
            break;
        }
    }
    
    printf("🏁 [Queued Client 1] Sequential messaging thread terminating\n");
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file.json>\n", argv[0]);
        return 1;
    }
    
    printf("Queued Direct Messaging Client 1 starting with config: %s\n", argv[1]);
    
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
    
    // Set topic configuration for communication with client 2
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
    
    printf("Queued Client 1 connecting to MQTT broker...\n");
    
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
    
    printf("🔗 [Queued Client 1] Connected to MQTT broker\n");
    printf("🔢 [Queued Client 1] Starting queued direct messaging (sequential requests)...\n");
    
    // Start queued messaging thread
    pthread_t messaging_thread_id;
    if (pthread_create(&messaging_thread_id, NULL, queued_messaging_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create messaging thread\n");
        atomic_store(&g_running, false);
    }
    
    // Main loop
    while (atomic_load(&g_running)) {
        sleep(1);
    }
    
    // Cleanup
    printf("Queued Client 1 shutting down...\n");
    
    if (messaging_thread_id) {
        pthread_join(messaging_thread_id, NULL);
    }
    
    ur_rpc_client_stop(g_client);
    ur_rpc_client_disconnect(g_client);
    ur_rpc_client_destroy(g_client);
    ur_rpc_topic_config_destroy(topic_config);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("🔗 Queued Direct Messaging Client 1 session completed\n");
    return 0;
}