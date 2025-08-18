#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include "../../ur-rpc-template/ur-rpc-template.h"

// Global variables
static ur_rpc_client_t* g_client = NULL;
static _Atomic bool g_running = true;
static _Atomic int g_status_count = 0;
static _Atomic int g_rpc_count = 0;
static pthread_mutex_t g_print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Safe printing function
void safe_print(const char* format, ...) {
    pthread_mutex_lock(&g_print_mutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&g_print_mutex);
}

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    safe_print("Received signal %d, shutting down gracefully...\n", sig);
    atomic_store(&g_running, false);
    exit(EXIT_FAILURE);
}

// Message handler for incoming messages
void on_message_handler(const char* topic, const char* payload, size_t payload_len, void* user_data) {
    (void)user_data;
    
    if (strstr(topic, "enhanced_client_a_ssl/messages")) {
        safe_print("📨 [Enhanced Client B] Received from Client A: %.*s\n", (int)payload_len, payload);
        
        // Check if it's a ping and respond with pong
        if (strstr(payload, "ping")) {
            time_t now = time(NULL);
            char pong_msg[256];
            snprintf(pong_msg, sizeof(pong_msg), 
                    "{\"type\":\"pong\",\"from\":\"enhanced_client_b_ssl\",\"in_response_to\":\"%.*s\",\"timestamp\":%ld,\"ssl\":true}", 
                    (int)payload_len, payload, now);
            
            if (ur_rpc_publish_message(g_client, "clients/enhanced_client_b_ssl/messages", pong_msg, strlen(pong_msg)) == UR_RPC_SUCCESS) {
                safe_print("🏓 [Enhanced Client B] Sent SSL pong response\n");
            }
        }
    } else if (strstr(topic, "enhanced_client_a_ssl/rpc/")) {
        safe_print("🔧 [Enhanced Client B] RPC message from Client A: %.*s\n", (int)payload_len, payload);
    } else if (strstr(topic, "heartbeat")) {
        safe_print("💓 [Enhanced Client B] Client A heartbeat: %.*s\n", (int)payload_len, payload);
    }
}

// Connection status callback
void on_connection_status(ur_rpc_connection_status_t status, void* user_data) {
    (void)user_data;
    
    switch (status) {
        case UR_RPC_CONN_CONNECTED:
            safe_print("🔐 [Enhanced Client B] Connected to SSL broker on port 1884\n");
            break;
        case UR_RPC_CONN_DISCONNECTED:
            safe_print("🔐 [Enhanced Client B] Disconnected from SSL broker\n");
            break;
        case UR_RPC_CONN_CONNECTING:
            safe_print("🔐 [Enhanced Client B] Connecting to SSL broker...\n");
            break;
        case UR_RPC_CONN_RECONNECTING:
            safe_print("🔐 [Enhanced Client B] Reconnecting to SSL broker...\n");
            break;
        case UR_RPC_CONN_ERROR:
            safe_print("🔐 [Enhanced Client B] Connection error\n");
            break;
        default:
            safe_print("🔐 [Enhanced Client B] Connection status: %d\n", status);
    }
}

// Application logic thread
void* application_thread(void* arg) {
    (void)arg;
    
    time_t last_status = 0;
    time_t last_rpc = 0;
    
    while (atomic_load(&g_running)) {
        time_t now = time(NULL);
        
        // Send status update every 7.5 seconds
        if (now - last_status >= 7) {
            int count = atomic_fetch_add(&g_status_count, 1);
            char status_msg[512];
            snprintf(status_msg, sizeof(status_msg), 
                    "{\"type\":\"status_update\",\"from\":\"enhanced_client_b_ssl\",\"count\":%d,\"timestamp\":%ld,\"ssl\":true,\"broker_port\":1884,\"encryption\":\"TLS\"}", 
                    count, now);
            
            if (ur_rpc_publish_message(g_client, "clients/enhanced_client_b_ssl/messages", status_msg, strlen(status_msg)) == UR_RPC_SUCCESS) {
                safe_print("📊 [Enhanced Client B] Sent SSL status update #%d\n", count);
            } else {
                safe_print("❌ [Enhanced Client B] Failed to send status update #%d\n", count);
            }
            last_status = now;
        }
        
        // Send RPC notification every 12.5 seconds
        if (now - last_rpc >= 12) {
            int count = atomic_fetch_add(&g_rpc_count, 1);
            char rpc_msg[256];
            snprintf(rpc_msg, sizeof(rpc_msg), 
                    "{\"type\":\"rpc_notification\",\"from\":\"enhanced_client_b_ssl\",\"count\":%d,\"timestamp\":%ld,\"ssl\":true}", 
                    count, now);
            
            if (ur_rpc_publish_message(g_client, "clients/enhanced_client_b_ssl/rpc/notifications", rpc_msg, strlen(rpc_msg)) == UR_RPC_SUCCESS) {
                safe_print("🔧 [Enhanced Client B] Sent SSL RPC notification #%d\n", count);
            } else {
                safe_print("❌ [Enhanced Client B] Failed to send RPC notification #%d\n", count);
            }
            last_rpc = now;
        }
        
        usleep(100000); // 100ms
    }
    
    safe_print("🏁 [Enhanced Client B] Application thread terminating\n");
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return 1;
    }
    
    safe_print("Enhanced Client B starting with SSL config: %s\n", argv[1]);
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize UR-RPC framework
    if (ur_rpc_init() != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to initialize UR-RPC framework\n");
        return 1;
    }
    
    // Create configuration
    ur_rpc_client_config_t* config = ur_rpc_config_create();
    if (!config) {
        fprintf(stderr, "Failed to create configuration\n");
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
    
    // Create client
    g_client = ur_rpc_client_create(config, topic_config);
    if (!g_client) {
        fprintf(stderr, "Failed to create RPC client\n");
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Set up callbacks
    ur_rpc_client_set_message_handler(g_client, on_message_handler, NULL);
    ur_rpc_client_set_connection_callback(g_client, on_connection_status, NULL);
    
    safe_print("Enhanced Client B connecting to SSL broker...\n");
    
    // Connect to broker
    if (ur_rpc_client_connect(g_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker\n");
        ur_rpc_client_destroy(g_client);
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Start the client
    if (ur_rpc_client_start(g_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to start RPC client\n");
        ur_rpc_client_disconnect(g_client);
        ur_rpc_client_destroy(g_client);
        ur_rpc_topic_config_destroy(topic_config);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Subscribe to topics
    ur_rpc_subscribe_topic(g_client, "clients/enhanced_client_a_ssl/messages");
    ur_rpc_subscribe_topic(g_client, "clients/enhanced_client_a_ssl/rpc/+");
    ur_rpc_subscribe_topic(g_client, "clients/enhanced_client_a_ssl/heartbeat");
    
    safe_print("Waiting for SSL connection...\n");
    sleep(2);
    
    safe_print("Enhanced Client B is ready for SSL bidirectional messaging\n");
    safe_print("Waiting for messages from Enhanced Client A...\n");
    
    // Start application thread
    pthread_t app_thread;
    if (pthread_create(&app_thread, NULL, application_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create application thread\n");
        atomic_store(&g_running, false);
    }
    
    // Main loop
    while (atomic_load(&g_running)) {
        usleep(100000); // 100ms
    }
    
    // Wait for application thread to finish
    if (app_thread) {
        pthread_join(app_thread, NULL);
    }
    
    safe_print("Enhanced Client B shutting down...\n");
    
    // Cleanup
    ur_rpc_client_stop(g_client);
    ur_rpc_client_disconnect(g_client);
    ur_rpc_client_destroy(g_client);
    ur_rpc_topic_config_destroy(topic_config);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    safe_print("🔐 Enhanced Client B SSL session completed\n");
    return 0;
}
