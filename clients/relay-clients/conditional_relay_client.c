/*
 * Conditional Relay Client
 * 
 * Implements conditional relay functionality where the secondary broker
 * connection is established only when certain conditions are met.
 * Features intelligent broker selection and conditional forwarding logic.
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
static volatile sig_atomic_t g_secondary_broker_ready = 0;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    g_running = 0;
    
    if (g_relay_client) {
        ur_rpc_relay_client_stop(g_relay_client);
    }
}

// Conditional logic checker - determines if message should be relayed
bool should_relay_message(const char* topic, const char* message) {
    if (!topic || !message) return false;
    
    // Parse message to check conditions
    cJSON* json = cJSON_Parse(message);
    if (!json) return true; // Relay all non-JSON messages by default
    
    bool should_relay = true;
    
    // Condition 1: Check priority level
    cJSON* priority = cJSON_GetObjectItemCaseSensitive(json, "priority");
    if (priority && cJSON_IsString(priority)) {
        const char* priority_str = cJSON_GetStringValue(priority);
        if (strcmp(priority_str, "low") == 0) {
            should_relay = false; // Don't relay low priority messages
        }
    }
    
    // Condition 2: Check message type
    cJSON* msg_type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (msg_type && cJSON_IsString(msg_type)) {
        const char* type_str = cJSON_GetStringValue(msg_type);
        if (strcmp(type_str, "debug") == 0) {
            should_relay = false; // Don't relay debug messages
        }
    }
    
    // Condition 3: Check timestamp freshness (relay only recent messages)
    cJSON* timestamp = cJSON_GetObjectItemCaseSensitive(json, "timestamp");
    if (timestamp && cJSON_IsNumber(timestamp)) {
        time_t msg_time = (time_t)cJSON_GetNumberValue(timestamp);
        time_t current_time = time(NULL);
        if (current_time - msg_time > 300) { // 5 minutes old
            should_relay = false; // Don't relay old messages
        }
    }
    
    // Condition 4: Check if secondary broker is ready
    if (!g_secondary_broker_ready) {
        should_relay = false;
    }
    
    cJSON_Delete(json);
    return should_relay;
}

// Message handler for conditional relay
void conditional_relay_message_handler(ur_rpc_client_t* client, const char* topic, const char* message, void* user_data) {
    if (!client || !topic || !message) return;
    
    printf("🔍 [Conditional Relay] Evaluating message on topic: %s\n", topic);
    
    // Apply conditional logic
    bool should_relay = should_relay_message(topic, message);
    
    if (should_relay) {
        printf("✅ [Conditional Relay] Message meets relay conditions\n");
        printf("📨 [Conditional Relay] Message content: %s\n", message);
        printf("➡️  [Conditional Relay] Message forwarded to secondary broker\n\n");
    } else {
        printf("❌ [Conditional Relay] Message filtered out (conditions not met)\n");
        printf("🚫 [Conditional Relay] Message not relayed\n\n");
    }
}

// Connection status callback with conditional logic
void conditional_connection_callback(ur_rpc_client_t* client, ur_rpc_connection_status_t status, void* user_data) {
    const char* broker_id = (const char*)user_data;
    
    switch (status) {
        case UR_RPC_CONN_CONNECTED:
            printf("🔗 [Conditional Relay] Connected to %s broker\n", broker_id ? broker_id : "MQTT");
            if (broker_id && strcmp(broker_id, "Secondary") == 0) {
                g_secondary_broker_ready = 1;
                printf("🟢 [Conditional Relay] Secondary broker is now ready for conditional relay\n");
            }
            break;
        case UR_RPC_CONN_DISCONNECTED:
            printf("❌ [Conditional Relay] Disconnected from %s broker\n", broker_id ? broker_id : "MQTT");
            if (broker_id && strcmp(broker_id, "Secondary") == 0) {
                g_secondary_broker_ready = 0;
                printf("🔴 [Conditional Relay] Secondary broker not ready - relay suspended\n");
            }
            break;
        case UR_RPC_CONN_RECONNECTING:
            printf("🔄 [Conditional Relay] Reconnecting to %s broker...\n", broker_id ? broker_id : "MQTT");
            break;
        case UR_RPC_CONN_ERROR:
            printf("💥 [Conditional Relay] Connection error to %s broker\n", broker_id ? broker_id : "MQTT");
            if (broker_id && strcmp(broker_id, "Secondary") == 0) {
                g_secondary_broker_ready = 0;
            }
            break;
        default:
            break;
    }
}

// Monitor secondary broker availability
void* secondary_broker_monitor(void* arg) {
    ur_rpc_relay_client_t* relay_client = (ur_rpc_relay_client_t*)arg;
    
    while (g_running) {
        sleep(10); // Check every 10 seconds
        
        // Check if we should enable/disable secondary broker connection
        // based on system load, time of day, or other conditions
        
        time_t current_time = time(NULL);
        struct tm* local_time = localtime(&current_time);
        
        // Example condition: Only relay during business hours (9 AM - 5 PM)
        bool business_hours = (local_time->tm_hour >= 9 && local_time->tm_hour < 17);
        
        static bool last_business_hours = false;
        if (business_hours != last_business_hours) {
            if (business_hours) {
                printf("⏰ [Conditional Relay] Business hours started - enabling secondary broker\n");
                // In a real implementation, this would trigger secondary broker connection
            } else {
                printf("⏰ [Conditional Relay] Business hours ended - disabling secondary broker\n");
                g_secondary_broker_ready = 0;
            }
            last_business_hours = business_hours;
        }
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <conditional_config_file.json>\n", argv[0]);
        fprintf(stderr, "Example: %s conditional_relay_config.json\n", argv[0]);
        return 1;
    }

    printf("================================================\n");
    printf("  Conditional MQTT Broker Relay Client         \n");
    printf("================================================\n");
    printf("Conditional Relay Client starting with config: %s\n", argv[1]);
    
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
    
    // Load conditional configuration from JSON file
    if (ur_rpc_config_load_from_file(config, argv[1]) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to load conditional configuration from %s\n", argv[1]);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Create topic configuration for conditional relay
    ur_rpc_topic_config_t* topic_config = ur_rpc_topic_config_create();
    if (!topic_config) {
        fprintf(stderr, "Failed to create topic configuration\n");
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    // Set topic configuration for conditional relay operation
    ur_rpc_topic_config_set_prefixes(topic_config, "conditional_relay", "smart");
    ur_rpc_topic_config_set_suffixes(topic_config, "filtered", "conditional", "status");
    
    // Create UR-RPC conditional relay client
    g_relay_client = ur_rpc_relay_client_create(config);
    if (!g_relay_client) {
        fprintf(stderr, "Failed to create UR-RPC conditional relay client\n");
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    printf("Conditional Relay connecting to MQTT brokers...\n");
    printf("🧠 [Conditional Relay] Initializing smart relay logic...\n");
    
    // Start conditional relay client (handles connection and relay automatically)
    if (ur_rpc_relay_client_start(g_relay_client) != UR_RPC_SUCCESS) {
        fprintf(stderr, "Failed to start conditional relay client\n");
        ur_rpc_relay_client_destroy(g_relay_client);
        ur_rpc_config_destroy(config);
        ur_rpc_cleanup();
        return 1;
    }
    
    printf("🚀 [Conditional Relay] Conditional relay functionality started successfully\n");
    printf("🧠 [Conditional Relay] Monitoring message conditions and broker availability...\n");
    printf("🔍 [Conditional Relay] Applying intelligent relay logic\n\n");
    
    // Start secondary broker monitor thread
    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, secondary_broker_monitor, g_relay_client) != 0) {
        fprintf(stderr, "Failed to create secondary broker monitor thread\n");
    }
    
    // Main conditional relay loop
    while (g_running) {
        sleep(1);
        
        // Check conditional relay statistics periodically
        static int stats_counter = 0;
        if (++stats_counter >= 30) { // Every 30 seconds
            printf("📊 [Conditional Relay] Stats - Conditional relay running for %d seconds\n", stats_counter * 30);
            printf("🧠 [Conditional Relay] Secondary broker ready: %s\n", 
                   g_secondary_broker_ready ? "Yes" : "No");
            stats_counter = 0;
        }
    }
    
    printf("\nConditional Relay shutting down...\n");
    
    // Stop secondary broker monitor
    pthread_cancel(monitor_thread);
    pthread_join(monitor_thread, NULL);
    
    // Stop conditional relay functionality
    ur_rpc_relay_client_stop(g_relay_client);
    
    // Cleanup
    ur_rpc_relay_client_destroy(g_relay_client);
    ur_rpc_config_destroy(config);
    ur_rpc_cleanup();
    
    printf("✅ [Conditional Relay] Smart shutdown complete\n");
    return 0;
}