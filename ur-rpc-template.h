#ifndef UR_RPC_TEMPLATE_H
#define UR_RPC_TEMPLATE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mosquitto.h>
#include <cJSON.h>
#include <stdatomic.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#define atomic_bool bool
#define atomic_load(x) (*(x))
#define atomic_store(x, val) ((*(x)) = (val))
#endif

/* Error codes */
typedef enum {
    UR_RPC_SUCCESS = 0,
    UR_RPC_ERROR_INVALID_PARAM = -1,
    UR_RPC_ERROR_MEMORY = -2,
    UR_RPC_ERROR_MQTT = -3,
    UR_RPC_ERROR_JSON = -4,
    UR_RPC_ERROR_TIMEOUT = -5,
    UR_RPC_ERROR_NOT_CONNECTED = -6,
    UR_RPC_ERROR_CONFIG = -7,
    UR_RPC_ERROR_THREAD = -8
} ur_rpc_error_t;

/* Request authority levels */
typedef enum {
    UR_RPC_AUTHORITY_ADMIN = 0,
    UR_RPC_AUTHORITY_USER = 1,
    UR_RPC_AUTHORITY_GUEST = 2,
    UR_RPC_AUTHORITY_SYSTEM = 3
} ur_rpc_authority_t;

/* RPC method types */
typedef enum {
    UR_RPC_METHOD_REQUEST_RESPONSE = 0,
    UR_RPC_METHOD_REQUEST_ONLY = 1,
    UR_RPC_METHOD_NOTIFICATION = 2
} ur_rpc_method_type_t;

/* Connection status */
typedef enum {
    UR_RPC_CONN_DISCONNECTED = 0,
    UR_RPC_CONN_CONNECTING = 1,
    UR_RPC_CONN_CONNECTED = 2,
    UR_RPC_CONN_RECONNECTING = 3,
    UR_RPC_CONN_ERROR = 4
} ur_rpc_connection_status_t;

/* Configuration constants */
#define UR_RPC_MAX_TOPIC_LENGTH 256
#define UR_RPC_MAX_PAYLOAD_LENGTH 4096
#define UR_RPC_MAX_CLIENT_ID_LENGTH 64
#define UR_RPC_MAX_TRANSACTION_ID_LENGTH 37
#define UR_RPC_DEFAULT_KEEPALIVE 60
#define UR_RPC_DEFAULT_QOS 1
#define UR_RPC_DEFAULT_TIMEOUT_MS 30000
#define UR_RPC_MAX_BROKERS 16
#define UR_RPC_MAX_PREFIX_LENGTH 128
#define UR_RPC_MAX_RELAY_RULES 32

/* Topic list structure for JSON configuration */
typedef struct {
    char** topics;
    int count;
    int capacity;
} ur_rpc_topic_list_t;

/* Heartbeat configuration structure */
typedef struct {
    bool enabled;              // Enable/disable heartbeat
    char* topic;               // Heartbeat topic
    int interval_seconds;      // Heartbeat interval in seconds
    char* payload;            // Custom heartbeat payload (JSON string)
} ur_rpc_heartbeat_config_t;

/* Broker configuration for relay */
typedef struct {
    char* host;                // Broker hostname
    int port;                  // Broker port
    char* username;            // Optional username
    char* password;            // Optional password
    char* client_id;           // Client ID for this broker
    bool use_tls;              // Enable TLS/SSL
    char* ca_file;             // CA certificate file
    bool is_primary;           // Primary broker flag
} ur_rpc_broker_config_t;

/* Topic relay rule configuration */
typedef struct {
    char* source_topic;        // Source topic pattern (supports wildcards)
    char* destination_topic;   // Destination topic pattern
    char* topic_prefix;        // Prefix to add to destination topic
    int source_broker_index;   // Index of source broker
    int dest_broker_index;     // Index of destination broker
    bool bidirectional;        // Enable bidirectional forwarding
} ur_rpc_relay_rule_t;

/* Relay configuration structure */
typedef struct {
    bool enabled;                                    // Enable/disable relay functionality
    bool conditional_relay;                         // Enable conditional relay (connect to second broker only when ready)
    ur_rpc_broker_config_t brokers[UR_RPC_MAX_BROKERS]; // Broker configurations
    int broker_count;                               // Number of configured brokers
    ur_rpc_relay_rule_t rules[UR_RPC_MAX_RELAY_RULES]; // Relay rules
    int rule_count;                                 // Number of relay rules
    char* relay_prefix;                            // Default prefix for relayed messages
} ur_rpc_relay_config_t;

/* Client configuration structure */
typedef struct {
    char* client_id;           // MQTT client ID
    char* broker_host;         // MQTT broker hostname
    int broker_port;           // MQTT broker port
    char* username;            // MQTT username (optional)
    char* password;            // MQTT password (optional)
    int keepalive;             // MQTT keepalive interval
    bool clean_session;        // MQTT clean session flag
    int qos;                   // Default QoS level
    bool use_tls;              // Enable TLS/SSL
    char* ca_file;             // CA certificate file
    char* cert_file;           // Client certificate file
    char* key_file;            // Client private key file
    char* tls_version;         // TLS version (e.g., "tlsv1.2", "tlsv1.3")
    bool tls_insecure;         // Disable TLS certificate verification (for testing)
    int connect_timeout;       // Connection timeout in seconds
    int message_timeout;       // Message timeout in seconds
    bool auto_reconnect;       // Enable automatic reconnection
    int reconnect_delay_min;   // Minimum reconnect delay (seconds)
    int reconnect_delay_max;   // Maximum reconnect delay (seconds)
    
    /* Topic configuration from JSON */
    ur_rpc_topic_list_t json_added_pubs;  // Topics to publish from JSON config
    ur_rpc_topic_list_t json_added_subs;  // Topics to subscribe from JSON config
    
    /* Heartbeat configuration */
    ur_rpc_heartbeat_config_t heartbeat;
    
    /* Relay configuration */
    ur_rpc_relay_config_t relay;
} ur_rpc_client_config_t;

/* RPC request structure */
typedef struct {
    char* transaction_id;      // Unique transaction identifier
    char* method;              // RPC method name
    char* service;             // Target service name
    ur_rpc_authority_t authority; // Request authority level
    cJSON* params;             // Request parameters (JSON object)
    char* response_topic;      // Response topic for this request
    uint64_t timestamp;        // Request timestamp
    int timeout_ms;            // Request timeout in milliseconds
} ur_rpc_request_t;

/* RPC response structure */
typedef struct {
    char* transaction_id;      // Transaction identifier (matches request)
    bool success;              // Success/failure flag
    cJSON* result;             // Result data (JSON object)
    char* error_message;       // Error message (if failed)
    int error_code;            // Error code (if failed)
    uint64_t timestamp;        // Response timestamp
    uint64_t processing_time_ms; // Processing time in milliseconds
} ur_rpc_response_t;

/* Topic configuration structure */
typedef struct {
    char* base_prefix;         // Base topic prefix (e.g., "ur_rpc")
    char* service_prefix;      // Service-specific prefix
    char* request_suffix;      // Request topic suffix (e.g., "request")
    char* response_suffix;     // Response topic suffix (e.g., "response")
    char* notification_suffix; // Notification topic suffix
    bool include_transaction_id; // Include transaction ID in topics
} ur_rpc_topic_config_t;

/* Message handler callback function */
typedef void (*ur_rpc_message_handler_t)(const char* topic, const char* payload, size_t payload_len, void* user_data);

/* Response handler callback function */
typedef void (*ur_rpc_response_handler_t)(const ur_rpc_response_t* response, void* user_data);

/* Connection status callback function */
typedef void (*ur_rpc_connection_callback_t)(ur_rpc_connection_status_t status, void* user_data);

/* Thread monitor structure */
typedef struct {
    pthread_t thread_id;
    atomic_bool running;
    atomic_bool healthy;
    time_t last_activity;
    uint64_t message_count;
    uint64_t error_count;
} ur_rpc_thread_monitor_t;

/* Pending request structure (for tracking responses) */
typedef struct ur_rpc_pending_request {
    char* transaction_id;
    char* response_topic;
    ur_rpc_response_handler_t callback;
    void* user_data;
    time_t created_time;
    int timeout_ms;
    struct ur_rpc_pending_request* next;
} ur_rpc_pending_request_t;

/* Main RPC client structure */
typedef struct {
    struct mosquitto* mosq;
    ur_rpc_client_config_t config;
    ur_rpc_topic_config_t topic_config;
    
    /* Connection management */
    atomic_bool connected;
    atomic_bool running;
    ur_rpc_connection_status_t status;
    ur_rpc_connection_callback_t connection_callback;
    void* connection_user_data;
    
    /* Threading */
    pthread_t mqtt_thread;
    pthread_t heartbeat_thread;
    atomic_bool heartbeat_running;
    pthread_mutex_t mutex;
    ur_rpc_thread_monitor_t thread_monitor;
    
    /* Message handling */
    ur_rpc_message_handler_t message_handler;
    void* message_user_data;
    
    /* Pending requests tracking */
    ur_rpc_pending_request_t* pending_requests;
    pthread_mutex_t pending_mutex;
    
    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t requests_sent;
    uint64_t responses_received;
    uint64_t errors_count;
    time_t last_activity;
} ur_rpc_client_t;

/* Multi-broker relay client structure */
typedef struct {
    ur_rpc_client_t* clients[UR_RPC_MAX_BROKERS];  // Array of client connections
    ur_rpc_relay_config_t config;                  // Relay configuration
    
    /* Threading */
    pthread_t relay_thread;
    atomic_bool relay_running;
    pthread_mutex_t relay_mutex;
    
    /* Statistics */
    uint64_t messages_relayed;
    uint64_t relay_errors;
    time_t relay_start_time;
    
    /* Message handling */
    ur_rpc_message_handler_t relay_message_handler;
    void* relay_user_data;
} ur_rpc_relay_client_t; 


/* Function prototypes */

/* Library initialization and cleanup */
int ur_rpc_init(void);
void ur_rpc_cleanup(void);
const char* ur_rpc_error_string(ur_rpc_error_t error);

/* Client configuration management */
ur_rpc_client_config_t* ur_rpc_config_create(void);
void ur_rpc_config_destroy(ur_rpc_client_config_t* config);
int ur_rpc_config_set_broker(ur_rpc_client_config_t* config, const char* host, int port);
int ur_rpc_config_set_credentials(ur_rpc_client_config_t* config, const char* username, const char* password);
int ur_rpc_config_set_client_id(ur_rpc_client_config_t* config, const char* client_id);
int ur_rpc_config_set_tls(ur_rpc_client_config_t* config, const char* ca_file, const char* cert_file, const char* key_file);
int ur_rpc_config_set_tls_version(ur_rpc_client_config_t* config, const char* tls_version);
int ur_rpc_config_set_tls_insecure(ur_rpc_client_config_t* config, bool insecure);
int ur_rpc_config_set_timeouts(ur_rpc_client_config_t* config, int connect_timeout, int message_timeout);
int ur_rpc_config_set_reconnect(ur_rpc_client_config_t* config, bool auto_reconnect, int min_delay, int max_delay);
int ur_rpc_config_load_from_file(ur_rpc_client_config_t* config, const char* filename);

/* Topic list management */
int ur_rpc_topic_list_init(ur_rpc_topic_list_t* list);
void ur_rpc_topic_list_cleanup(ur_rpc_topic_list_t* list);
int ur_rpc_topic_list_add(ur_rpc_topic_list_t* list, const char* topic);

/* Heartbeat management */
int ur_rpc_heartbeat_start(ur_rpc_client_t* client);
int ur_rpc_heartbeat_stop(ur_rpc_client_t* client);
int ur_rpc_config_set_heartbeat(ur_rpc_client_config_t* config, const char* topic, int interval_seconds, const char* payload);

/* Topic configuration management */
ur_rpc_topic_config_t* ur_rpc_topic_config_create(void);
void ur_rpc_topic_config_destroy(ur_rpc_topic_config_t* config);
int ur_rpc_topic_config_set_prefixes(ur_rpc_topic_config_t* config, const char* base_prefix, const char* service_prefix);
int ur_rpc_topic_config_set_suffixes(ur_rpc_topic_config_t* config, const char* request_suffix, const char* response_suffix, const char* notification_suffix);

/* Client lifecycle management */
ur_rpc_client_t* ur_rpc_client_create(const ur_rpc_client_config_t* config, const ur_rpc_topic_config_t* topic_config);
void ur_rpc_client_destroy(ur_rpc_client_t* client);
int ur_rpc_client_connect(ur_rpc_client_t* client);
int ur_rpc_client_disconnect(ur_rpc_client_t* client);
int ur_rpc_client_start(ur_rpc_client_t* client);
int ur_rpc_client_stop(ur_rpc_client_t* client);
bool ur_rpc_client_is_connected(const ur_rpc_client_t* client);
ur_rpc_connection_status_t ur_rpc_client_get_status(const ur_rpc_client_t* client);

/* Callback management */
void ur_rpc_client_set_connection_callback(ur_rpc_client_t* client, ur_rpc_connection_callback_t callback, void* user_data);
void ur_rpc_client_set_message_handler(ur_rpc_client_t* client, ur_rpc_message_handler_t handler, void* user_data);

/* Request/Response management */
ur_rpc_request_t* ur_rpc_request_create(void);
void ur_rpc_request_destroy(ur_rpc_request_t* request);
int ur_rpc_request_set_method(ur_rpc_request_t* request, const char* method, const char* service);
int ur_rpc_request_set_authority(ur_rpc_request_t* request, ur_rpc_authority_t authority);
int ur_rpc_request_set_params(ur_rpc_request_t* request, const cJSON* params);
int ur_rpc_request_set_timeout(ur_rpc_request_t* request, int timeout_ms);

ur_rpc_response_t* ur_rpc_response_create(void);
void ur_rpc_response_destroy(ur_rpc_response_t* response);

/* RPC operations */
int ur_rpc_call_async(ur_rpc_client_t* client, const ur_rpc_request_t* request, ur_rpc_response_handler_t callback, void* user_data);
int ur_rpc_call_sync(ur_rpc_client_t* client, const ur_rpc_request_t* request, ur_rpc_response_t** response, int timeout_ms);
int ur_rpc_send_notification(ur_rpc_client_t* client, const char* method, const char* service, ur_rpc_authority_t authority, const cJSON* params);
int ur_rpc_publish_message(ur_rpc_client_t* client, const char* topic, const char* payload, size_t payload_len);
int ur_rpc_subscribe_topic(ur_rpc_client_t* client, const char* topic);
int ur_rpc_unsubscribe_topic(ur_rpc_client_t* client, const char* topic);

/* Topic generation utilities */
char* ur_rpc_generate_request_topic(const ur_rpc_client_t* client, const char* method, const char* service, const char* transaction_id);
char* ur_rpc_generate_response_topic(const ur_rpc_client_t* client, const char* method, const char* service, const char* transaction_id);
char* ur_rpc_generate_notification_topic(const ur_rpc_client_t* client, const char* method, const char* service);
void ur_rpc_free_string(char* str);

/* Transaction management */
char* ur_rpc_generate_transaction_id(void);
bool ur_rpc_validate_transaction_id(const char* transaction_id);

/* JSON utilities */
char* ur_rpc_request_to_json(const ur_rpc_request_t* request);
ur_rpc_request_t* ur_rpc_request_from_json(const char* json_str);
char* ur_rpc_response_to_json(const ur_rpc_response_t* response);
ur_rpc_response_t* ur_rpc_response_from_json(const char* json_str);

/* Statistics and monitoring */
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t requests_sent;
    uint64_t responses_received;
    uint64_t notifications_sent;
    uint64_t errors_count;
    uint64_t connection_count;
    uint64_t uptime_seconds;
    time_t last_activity;
} ur_rpc_statistics_t;

int ur_rpc_client_get_statistics(const ur_rpc_client_t* client, ur_rpc_statistics_t* stats);
int ur_rpc_client_reset_statistics(ur_rpc_client_t* client);

/* Relay functionality */
ur_rpc_relay_client_t* ur_rpc_relay_client_create(const ur_rpc_client_config_t* config);
void ur_rpc_relay_client_destroy(ur_rpc_relay_client_t* relay_client);
int ur_rpc_relay_client_start(ur_rpc_relay_client_t* relay_client);
int ur_rpc_relay_client_stop(ur_rpc_relay_client_t* relay_client);
int ur_rpc_relay_config_add_broker(ur_rpc_relay_config_t* config, const char* host, int port, const char* client_id, bool is_primary);
int ur_rpc_relay_config_add_rule(ur_rpc_relay_config_t* config, const char* source_topic, const char* dest_topic, const char* prefix, int source_broker, int dest_broker, bool bidirectional);
int ur_rpc_relay_config_set_prefix(ur_rpc_relay_config_t* config, const char* prefix);

/* Global variable for conditional relay control */
extern bool g_sec_conn_ready;

/* Conditional relay functionality */
int ur_rpc_relay_set_secondary_connection_ready(bool ready);
bool ur_rpc_relay_is_secondary_connection_ready(void);
int ur_rpc_relay_connect_secondary_brokers(ur_rpc_relay_client_t* relay_client);

/* Relay configuration management */
int ur_rpc_relay_config_init(ur_rpc_relay_config_t* config);
void ur_rpc_relay_config_cleanup(ur_rpc_relay_config_t* config);

/* Utility functions */
const char* ur_rpc_authority_to_string(ur_rpc_authority_t authority);
ur_rpc_authority_t ur_rpc_authority_from_string(const char* authority_str);
const char* ur_rpc_connection_status_to_string(ur_rpc_connection_status_t status);
uint64_t ur_rpc_get_timestamp_ms(void);

#ifdef __cplusplus
#undef atomic_bool
#undef atomic_load
#undef atomic_store
}
#endif

#endif /* UR_RPC_TEMPLATE_H */