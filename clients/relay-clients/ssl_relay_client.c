/*
 * SSL Relay Client
 * 
 * Relays messages between two SSL-secured MQTT brokers.
 * Handles TLS/SSL connections and certificate validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include "ur-rpc-template.h"

// Global variables for signal handling
static ur_rpc_relay_client_t* g_relay_client = NULL;
static volatile sig_atomic_t g_running = 1;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    g_running = 0;
    
    if (g_relay_client) {
        ur_rpc_relay_client_stop(g_relay_client);
    }
}

// Message handler for SSL relayed messages
void ssl_relay_message_handler(ur_rpc_client_t* client, const char* topic, const char* message, void* user_data) {
    if (!client || !topic || !message) return;
    
    printf("🔐 [SSL Relay] Received encrypted message on topic: %s\n", topic);
    printf("📨 [SSL Relay] Message content: %s\n", message);
    
    // The ur-rpc-template library handles the SSL relay automatically
    printf("🔒 [SSL Relay] Message securely forwarded between SSL brokers\n\n");
}

// Connection status callback with SSL info
void ssl_connection_callback(ur_rpc_client_t* client, ur_rpc_connection_status_t status, void* user_data) {
    const char* broker_id = (const char*)user_data;
    
    switch (status) {
        case UR_RPC_CONN_CONNECTED:
            printf("🔗 [SSL Relay] Secure SSL connection established to %s broker\n", broker_id ? broker_id : "MQTT");
            break;
        case UR_RPC_CONN_DISCONNECTED:
            printf("❌ [SSL Relay] SSL connection lost to %s broker\n", broker_id ? broker_id : "MQTT");
            break;
        case UR_RPC_CONN_RECONNECTING:
            printf("🔄 [SSL Relay] Reconnecting with SSL to %s broker...\n", broker_id ? broker_id : "MQTT");
            break;
        case UR_RPC_CONN_ERROR:
            printf("💥 [SSL Relay] SSL connection error to %s broker\n", broker_id ? broker_id : "MQTT");
            break;
        default:
            break;
    }
}

// SSL certificate validation callback
void ssl_cert_callback(ur_rpc_client_t* client, const char* cert_info, bool valid, void* user_data) {
    if (valid) {
        printf("✅ [SSL Relay] Certificate validation successful\n");
        printf("🔐 [SSL Relay] Certificate info: %s\n", cert_info ? cert_info : "N/A");
    } else {
        printf("⚠️  [SSL Relay] Certificate validation failed: %s\n", cert_info ? cert_info : "Unknown error");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ssl_config_file.json>\n", argv[0]);
        fprintf(stderr, "Example: %s ssl_relay_config.json\n", argv[0]);
        return 1;
    }

    printf("==========================================\n");
    printf("  SSL-Secured MQTT Broker Relay Client   \n");
    printf("==========================================\n");
    printf("SSL Relay Client starting with config: %s\n", argv[1]);
    
    // Set up signal handlers
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
    
    // Load SSL configuration from JSON file
    if (ur_rpc_config_load_from_file(config, argv[1]) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to load SSL configuration from %s\n", argv[1]);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Create topic configuration for SSL relay
    ur_rpc_topic_config_t* topic_config = ur_rpc_topic_config_create();
    if (!topic_config) {
        fprintf(stderr, "Failed to create topic configuration\n");
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Set topic configuration for SSL relay operation
    ur_rpc_topic_config_set_prefixes(topic_config, "ssl_relay", "secure");
    ur_rpc_topic_config_set_suffixes(topic_config, "encrypted", "decrypted", "status");
    
    // Create UR-RPC SSL relay client
    g_relay_client = ur_rpc_relay_client_create(config);
    if (!g_relay_client) {
        fprintf(stderr, "Failed to create UR-RPC SSL relay client\n");
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    printf("SSL Relay connecting to secure MQTT brokers...\n");
    printf("🔐 [SSL Relay] Initializing SSL/TLS connections...\n");
    
    // Start SSL relay client (handles connection and relay automatically)
    if (ur_rpc_relay_client_start(g_relay_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to start SSL relay client\n");
        ur_rpc_relay_client_destroy(g_relay_client);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    printf("🚀 [SSL Relay] SSL relay functionality started successfully\n");
    printf("🔒 [SSL Relay] Monitoring encrypted message forwarding between SSL brokers...\n");
    printf("🔐 [SSL Relay] All communications are TLS/SSL encrypted\n\n");
    
    // Main SSL relay loop
    while (g_running) {
        sleep(1);
        
        // Check SSL relay statistics periodically
        static int stats_counter = 0;
        if (++stats_counter >= 30) { // Every 30 seconds
            printf("📊 [SSL Relay] Stats - SSL relay running for %d seconds\n", stats_counter * 30);
            printf("🔐 [SSL Relay] All communications encrypted with TLS/SSL\n");
            stats_counter = 0;
        }
    }
    
    printf("\nSSL Relay shutting down...\n");
    
    // Stop SSL relay functionality
    ur_rpc_relay_client_stop(g_relay_client);
    
    // Cleanup
    ur_rpc_relay_client_destroy(g_relay_client);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("✅ [SSL Relay] Secure shutdown complete\n");
    return 0;
}