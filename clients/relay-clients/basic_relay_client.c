/*
 * Basic Relay Client
 * 
 * Relays messages between two MQTT brokers on different ports.
 * Subscribes to topics on broker 1 and forwards messages to broker 2.
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

// Message handler for relayed messages
void relay_message_handler(const char* topic, const char* payload, size_t payload_len, void* user_data) {
    if (!topic || !payload) return;
    
    printf("🔄 [Basic Relay] Received message on topic: %s\n", topic);
    printf("📨 [Basic Relay] Message content: %.*s\n", (int)payload_len, payload);
    
    // The ur-rpc-template library handles the relay automatically based on configuration
    // Just log the relay activity
    printf("➡️  [Basic Relay] Message forwarded to destination broker\n\n");
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file.json>\n", argv[0]);
        fprintf(stderr, "Example: %s basic_relay_config.json\n", argv[0]);
        return 1;
    }

    printf("======================================\n");
    printf("  Basic MQTT Broker Relay Client     \n");
    printf("======================================\n");
    printf("Basic Relay Client starting with config: %s\n", argv[1]);
    
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
    
    // Load configuration from JSON file
    if (ur_rpc_config_load_from_file(config, argv[1]) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to load configuration from %s\n", argv[1]);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Create UR-RPC relay client
    g_relay_client = ur_rpc_relay_client_create(config);
    if (!g_relay_client) {
        fprintf(stderr, "Failed to create UR-RPC relay client\n");
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    printf("Basic Relay connecting to MQTT brokers...\n");
    
    // Start relay client (handles connection and relay automatically)
    if (ur_rpc_relay_client_start(g_relay_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to start relay client\n");
        ur_rpc_relay_client_destroy(g_relay_client);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    printf("🚀 [Basic Relay] Relay functionality started successfully\n");
    printf("📡 [Basic Relay] Monitoring message forwarding between brokers...\n");
    printf("🔄 [Basic Relay] Relaying messages from broker 1 to broker 2\n\n");
    
    // Main relay loop
    while (g_running) {
        sleep(1);
        
        // Check relay statistics periodically
        static int stats_counter = 0;
        if (++stats_counter >= 30) { // Every 30 seconds
            printf("📊 [Basic Relay] Stats - Relay running for %d seconds\n", stats_counter * 30);
            stats_counter = 0;
        }
    }
    
    printf("\nBasic Relay shutting down...\n");
    
    // Stop relay functionality
    ur_rpc_relay_client_stop(g_relay_client);
    
    // Cleanup
    ur_rpc_relay_client_destroy(g_relay_client);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("✅ [Basic Relay] Shutdown complete\n");
    return 0;
}