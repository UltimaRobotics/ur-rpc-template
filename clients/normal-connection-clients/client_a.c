#include "../../ur-rpc-template/ur-rpc-template.h"
#include <signal.h>
#include <unistd.h>

// Global variables for clean shutdown
static volatile bool running = true;
static ur_rpc_client_t* g_client = NULL;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    running = false;
}

// Connection status callback
void connection_callback(ur_rpc_connection_status_t status, void* user_data) {
    const char* client_name = (const char*)user_data;
    printf("[%s] Connection status changed: %s\n", 
           client_name, ur_rpc_connection_status_to_string(status));
    
    if (status == UR_RPC_CONN_CONNECTED) {
        printf("[%s] Successfully connected to broker\n", client_name);
        
        // Subscribe to messages from Client B
        ur_rpc_subscribe_topic(g_client, "clients/client_b/messages");
        ur_rpc_subscribe_topic(g_client, "clients/client_b/rpc/+/+");
        printf("[%s] Subscribed to Client B topics\n", client_name);
    }
}

// Message handler for incoming messages
void message_handler(const char* topic, const char* payload, size_t payload_len, void* user_data) {
    const char* client_name = (const char*)user_data;
    
    printf("[%s] Received message on topic '%s': %.*s\n", 
           client_name, topic, (int)payload_len, payload);
    
    // Parse JSON payload if it's an RPC request
    if (strstr(topic, "/rpc/") != NULL) {
        cJSON* json = cJSON_Parse(payload);
        if (json) {
            cJSON* method = cJSON_GetObjectItem(json, "method");
            cJSON* transaction_id = cJSON_GetObjectItem(json, "transaction_id");
            cJSON* params = cJSON_GetObjectItem(json, "params");
            
            if (method && transaction_id) {
                printf("[%s] RPC Request - Method: %s, Transaction: %s\n", 
                       client_name, method->valuestring, transaction_id->valuestring);
                
                // Create response
                ur_rpc_response_t* response = ur_rpc_response_create();
                response->transaction_id = strdup(transaction_id->valuestring);
                response->success = true;
                response->timestamp = ur_rpc_get_timestamp_ms();
                
                // Create result data
                cJSON* result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "message", "Hello from Client A!");
                cJSON_AddStringToObject(result, "processed_by", "client_a");
                cJSON_AddNumberToObject(result, "timestamp", (double)response->timestamp);
                response->result = result;
                
                // Send response
                char* response_json = ur_rpc_response_to_json(response);
                if (response_json) {
                    char response_topic[256];
                    snprintf(response_topic, sizeof(response_topic), 
                            "clients/client_a/rpc/response/%s", transaction_id->valuestring);
                    
                    ur_rpc_publish_message(g_client, response_topic, response_json, strlen(response_json));
                    printf("[%s] Sent RPC response to topic: %s\n", client_name, response_topic);
                    
                    free(response_json);
                }
                
                ur_rpc_response_destroy(response);
            }
            cJSON_Delete(json);
        }
    }
}

// RPC response handler
void response_handler(const ur_rpc_response_t* response, void* user_data) {
    const char* client_name = (const char*)user_data;
    
    printf("[%s] RPC Response received - Transaction: %s, Success: %s\n",
           client_name, response->transaction_id, response->success ? "true" : "false");
    
    if (response->success && response->result) {
        char* result_str = cJSON_Print(response->result);
        if (result_str) {
            printf("[%s] Response data: %s\n", client_name, result_str);
            free(result_str);
        }
    } else if (response->error_message) {
        printf("[%s] Error: %s (Code: %d)\n", 
               client_name, response->error_message, response->error_code);
    }
}

int main(int argc, char* argv[]) {
    const char* config_file = "client_a_config.json";
    
    // Parse command line arguments
    if (argc > 1) {
        config_file = argv[1];
    }
    
    printf("Client A starting with config: %s\n", config_file);
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize UR-RPC framework
    if (ur_rpc_init() != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to initialize UR-RPC framework\n");
        return 1;
    }
    
    // Create and load configuration
    ur_rpc_client_config_t* config = ur_rpc_config_create();
    if (!config) {
        fprintf(stderr, "Failed to create configuration\n");
        ur_rpc_cleanup();
        return 1;
    }
    
    // Load configuration from JSON file
    if (ur_rpc_config_load_from_file(config, config_file) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to load configuration from %s\n", config_file);
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
    
    // Create client
    g_client = ur_rpc_client_create(config, topic_config);
    if (!g_client) {
        fprintf(stderr, "Failed to create RPC client\n");
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Set callbacks
    ur_rpc_client_set_connection_callback(g_client, connection_callback, (void*)"Client A");
    ur_rpc_client_set_message_handler(g_client, message_handler, (void*)"Client A");
    
    // Connect to broker
    printf("Client A connecting to broker...\n");
    if (ur_rpc_client_connect(g_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to connect to broker\n");
        ur_rpc_client_destroy(g_client);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Start client
    if (ur_rpc_client_start(g_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to start client\n");
        ur_rpc_client_disconnect(g_client);
        ur_rpc_client_destroy(g_client);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Wait for connection
    printf("Waiting for connection...\n");
    for (int i = 0; i < 50 && !ur_rpc_client_is_connected(g_client); i++) {
        usleep(100000); // 100ms
    }
    
    if (!ur_rpc_client_is_connected(g_client)) {
        fprintf(stderr, "Failed to establish connection within timeout\n");
        running = false;
    }
    
    printf("Client A is ready for bidirectional messaging\n");
    printf("Commands: 'send' to send message, 'rpc' to make RPC call, 'quit' to exit\n");
    
    // Main message loop
    int message_counter = 0;
    int rpc_counter = 0;
    
    while (running && ur_rpc_client_is_connected(g_client)) {
        // Send periodic messages to Client B
        if (message_counter % 50 == 0) {  // Every 5 seconds (100ms * 50)
            cJSON* message = cJSON_CreateObject();
            cJSON_AddStringToObject(message, "from", "client_a");
            cJSON_AddStringToObject(message, "type", "ping");
            cJSON_AddNumberToObject(message, "counter", message_counter / 50);
            cJSON_AddNumberToObject(message, "timestamp", (double)ur_rpc_get_timestamp_ms());
            
            char* message_str = cJSON_Print(message);
            if (message_str) {
                ur_rpc_publish_message(g_client, "clients/client_a/messages", message_str, strlen(message_str));
                printf("[Client A] Sent ping message #%d\n", message_counter / 50);
                free(message_str);
            }
            cJSON_Delete(message);
        }
        
        // Send periodic RPC calls to Client B
        if (rpc_counter % 100 == 0 && rpc_counter > 0) {  // Every 10 seconds
            ur_rpc_request_t* request = ur_rpc_request_create();
            ur_rpc_request_set_method(request, "get_status", "client_b");
            ur_rpc_request_set_authority(request, UR_RPC_AUTHORITY_USER);
            ur_rpc_request_set_timeout(request, 5000);
            
            cJSON* params = cJSON_CreateObject();
            cJSON_AddStringToObject(params, "requester", "client_a");
            cJSON_AddNumberToObject(params, "request_id", rpc_counter / 100);
            ur_rpc_request_set_params(request, params);
            
            printf("[Client A] Sending RPC request #%d to Client B\n", rpc_counter / 100);
            ur_rpc_call_async(g_client, request, response_handler, (void*)"Client A");
            
            ur_rpc_request_destroy(request);
            cJSON_Delete(params);
        }
        
        message_counter++;
        rpc_counter++;
        usleep(100000); // 100ms
    }
    
    printf("Client A shutting down...\n");
    
    // Cleanup
    ur_rpc_client_stop(g_client);
    ur_rpc_client_disconnect(g_client);
    ur_rpc_client_destroy(g_client);
    ur_rpc_topic_config_destroy(topic_config);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("Client A shutdown complete\n");
    return 0;
}