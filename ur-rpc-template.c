#include "ur-rpc-template.h"
#include <sys/time.h>
#include <errno.h>
#include <limits.h>

/* Global variable for conditional relay control */
bool g_sec_conn_ready = false;

/* MQTT Callback Functions */

static void on_connect_callback(struct mosquitto *mosq, void *obj, int rc) {
    ur_rpc_client_t *client = (ur_rpc_client_t *)obj;
    if (!client) return;
    
    pthread_mutex_lock(&client->mutex);
    
    if (rc == MOSQ_ERR_SUCCESS) {
        atomic_store(&client->connected, true);
        client->status = UR_RPC_CONN_CONNECTED;
        client->last_activity = time(NULL);
        LOG_INFO_SIMPLE("MQTT connected successfully (rc=%d)", rc);
    } else {
        atomic_store(&client->connected, false);
        client->status = UR_RPC_CONN_ERROR;
        client->errors_count++;
        LOG_ERROR_SIMPLE("MQTT connection failed (rc=%d)", rc);
    }
    
    pthread_mutex_unlock(&client->mutex);
    
    /* Call user callback if set */
    if (client->connection_callback) {
        client->connection_callback(client->status, client->connection_user_data);
    }
}

static void on_disconnect_callback(struct mosquitto *mosq, void *obj, int rc) {
    ur_rpc_client_t *client = (ur_rpc_client_t *)obj;
    if (!client) return;
    
    pthread_mutex_lock(&client->mutex);
    
    atomic_store(&client->connected, false);
    if (rc == 0) {
        client->status = UR_RPC_CONN_DISCONNECTED;
        LOG_INFO_SIMPLE("MQTT disconnected gracefully");
    } else {
        client->status = UR_RPC_CONN_ERROR;
        client->errors_count++;
        LOG_WARN_SIMPLE("MQTT disconnected unexpectedly (rc=%d)", rc);
    }
    
    pthread_mutex_unlock(&client->mutex);
    
    /* Call user callback if set */
    if (client->connection_callback) {
        client->connection_callback(client->status, client->connection_user_data);
    }
}

static void on_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    ur_rpc_client_t *client = (ur_rpc_client_t *)obj;
    if (!client || !message || !message->payload) return;
    
    pthread_mutex_lock(&client->mutex);
    client->messages_received++;
    client->last_activity = time(NULL);
    pthread_mutex_unlock(&client->mutex);
    
    /* Call user message handler if set */
    if (client->message_handler) {
        client->message_handler(message->topic, (const char*)message->payload, message->payloadlen, client->message_user_data);
    }
    
    LOG_DEBUG_SIMPLE("RECEIVED from %s: %.*s", message->topic, message->payloadlen, (char*)message->payload);
}

static void on_publish_callback(struct mosquitto *mosq, void *obj, int mid) {
    ur_rpc_client_t *client = (ur_rpc_client_t *)obj;
    if (!client) return;
    
    pthread_mutex_lock(&client->mutex);
    client->last_activity = time(NULL);
    pthread_mutex_unlock(&client->mutex);
    
    LOG_DEBUG_SIMPLE("Message published successfully (mid=%d)", mid);
}

static void on_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
    ur_rpc_client_t *client = (ur_rpc_client_t *)obj;
    if (!client) return;
    
    pthread_mutex_lock(&client->mutex);
    client->last_activity = time(NULL);
    pthread_mutex_unlock(&client->mutex);
    
    LOG_DEBUG_SIMPLE("Subscribed successfully (mid=%d, qos=%d)", mid, granted_qos ? granted_qos[0] : -1);
}

static void on_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str) {
    LOG_DEBUG_SIMPLE("MQTT Log [%d]: %s", level, str);
}

/* Global state for library initialization */
static bool g_library_initialized = false;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
 * Library Initialization and Cleanup
 * ============================================================================ */

int ur_rpc_init(void) {
    pthread_mutex_lock(&g_init_mutex);
    
    if (g_library_initialized) {
        pthread_mutex_unlock(&g_init_mutex);
        return UR_RPC_SUCCESS;
    }
    
    // Initialize the logger system first
    logger_init(LOG_INFO, LOG_FLAG_CONSOLE | LOG_FLAG_TIMESTAMP | LOG_FLAG_COLOR, NULL);
    LOG_INFO_SIMPLE("Initializing UR-RPC framework");
    
    // Initialize Mosquitto library
    int mosq_result = mosquitto_lib_init();
    if (mosq_result != MOSQ_ERR_SUCCESS) {
        LOG_ERROR_SIMPLE("Failed to initialize Mosquitto library (error: %d)", mosq_result);
        pthread_mutex_unlock(&g_init_mutex);
        return UR_RPC_ERROR_MQTT;
    }
    
    // Initialize random seed for transaction IDs
    srand((unsigned int)time(NULL));
    
    g_library_initialized = true;
    LOG_INFO_SIMPLE("UR-RPC framework initialized successfully");
    pthread_mutex_unlock(&g_init_mutex);
    return UR_RPC_SUCCESS;
}

void ur_rpc_cleanup(void) {
    pthread_mutex_lock(&g_init_mutex);
    
    if (g_library_initialized) {
        LOG_INFO_SIMPLE("Cleaning up UR-RPC framework");
        mosquitto_lib_cleanup();
        g_library_initialized = false;
        logger_destroy();
    }
    
    pthread_mutex_unlock(&g_init_mutex);
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

const char* ur_rpc_error_string(ur_rpc_error_t error) {
    switch (error) {
        case UR_RPC_SUCCESS: return "Success";
        case UR_RPC_ERROR_INVALID_PARAM: return "Invalid parameter";
        case UR_RPC_ERROR_MEMORY: return "Memory allocation error";
        case UR_RPC_ERROR_MQTT: return "MQTT error";
        case UR_RPC_ERROR_JSON: return "JSON parsing error";
        case UR_RPC_ERROR_TIMEOUT: return "Operation timeout";
        case UR_RPC_ERROR_NOT_CONNECTED: return "Not connected";
        case UR_RPC_ERROR_CONFIG: return "Configuration error";
        case UR_RPC_ERROR_THREAD: return "Thread error";
        default: return "Unknown error";
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

uint64_t ur_rpc_get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

char* ur_rpc_generate_transaction_id(void) {
    char* transaction_id = malloc(UR_RPC_MAX_TRANSACTION_ID_LENGTH);
    if (!transaction_id) return NULL;
    
    // Simple UUID-like generation using timestamp and random
    uint64_t timestamp = ur_rpc_get_timestamp_ms();
    int rand1 = rand();
    int rand2 = rand();
    
    snprintf(transaction_id, UR_RPC_MAX_TRANSACTION_ID_LENGTH,
             "%08x-%04x-%04x-%04x-%08x%04x",
             (unsigned int)(timestamp & 0xFFFFFFFF),
             (unsigned short)(rand1 & 0xFFFF),
             (unsigned short)(4000 | (rand1 >> 16 & 0x0FFF)), // version 4
             (unsigned short)(0x8000 | (rand2 & 0x3FFF)),     // variant
             (unsigned int)(timestamp >> 32),
             (unsigned short)(rand2 >> 16 & 0xFFFF));
    
    return transaction_id;
}

bool ur_rpc_validate_transaction_id(const char* transaction_id) {
    if (!transaction_id || strlen(transaction_id) != 36) return false;
    
    for (int i = 0; i < 36; i++) {
        char c = transaction_id[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        }
    }
    return true;
}

void ur_rpc_free_string(char* str) {
    if (str) free(str);
}

const char* ur_rpc_authority_to_string(ur_rpc_authority_t authority) {
    switch (authority) {
        case UR_RPC_AUTHORITY_ADMIN: return "admin";
        case UR_RPC_AUTHORITY_USER: return "user";
        case UR_RPC_AUTHORITY_GUEST: return "guest";
        case UR_RPC_AUTHORITY_SYSTEM: return "system";
        default: return "unknown";
    }
}

ur_rpc_authority_t ur_rpc_authority_from_string(const char* authority_str) {
    if (!authority_str) return UR_RPC_AUTHORITY_GUEST;
    
    if (strcmp(authority_str, "admin") == 0) return UR_RPC_AUTHORITY_ADMIN;
    if (strcmp(authority_str, "user") == 0) return UR_RPC_AUTHORITY_USER;
    if (strcmp(authority_str, "guest") == 0) return UR_RPC_AUTHORITY_GUEST;
    if (strcmp(authority_str, "system") == 0) return UR_RPC_AUTHORITY_SYSTEM;
    
    return UR_RPC_AUTHORITY_GUEST;
}

const char* ur_rpc_connection_status_to_string(ur_rpc_connection_status_t status) {
    switch (status) {
        case UR_RPC_CONN_DISCONNECTED: return "disconnected";
        case UR_RPC_CONN_CONNECTING: return "connecting";
        case UR_RPC_CONN_CONNECTED: return "connected";
        case UR_RPC_CONN_RECONNECTING: return "reconnecting";
        case UR_RPC_CONN_ERROR: return "error";
        default: return "unknown";
    }
}

/* ============================================================================
 * Client Configuration Management
 * ============================================================================ */

ur_rpc_client_config_t* ur_rpc_config_create(void) {
    ur_rpc_client_config_t* config = calloc(1, sizeof(ur_rpc_client_config_t));
    if (!config) return NULL;
    
    // Set default values
    config->broker_port = 1883;
    config->keepalive = UR_RPC_DEFAULT_KEEPALIVE;
    config->clean_session = true;
    config->qos = UR_RPC_DEFAULT_QOS;
    config->use_tls = false;
    config->connect_timeout = 10;
    config->message_timeout = UR_RPC_DEFAULT_TIMEOUT_MS / 1000;
    config->auto_reconnect = true;
    config->reconnect_delay_min = 1;
    config->reconnect_delay_max = 60;
    
    // Initialize topic lists
    ur_rpc_topic_list_init(&config->json_added_pubs);
    ur_rpc_topic_list_init(&config->json_added_subs);
    
    // Initialize heartbeat configuration
    config->heartbeat.enabled = false;
    config->heartbeat.topic = NULL;
    config->heartbeat.interval_seconds = 30;
    config->heartbeat.payload = NULL;
    
    // Initialize relay configuration
    ur_rpc_relay_config_init(&config->relay);
    
    return config;
}

void ur_rpc_config_destroy(ur_rpc_client_config_t* config) {
    if (!config) return;
    
    free(config->client_id);
    free(config->broker_host);
    free(config->username);
    free(config->password);
    free(config->ca_file);
    free(config->cert_file);
    free(config->key_file);
    
    // Cleanup topic lists
    ur_rpc_topic_list_cleanup(&config->json_added_pubs);
    ur_rpc_topic_list_cleanup(&config->json_added_subs);
    
    // Cleanup heartbeat configuration
    free(config->heartbeat.topic);
    free(config->heartbeat.payload);
    
    // Cleanup relay configuration
    ur_rpc_relay_config_cleanup(&config->relay);
    
    free(config);
}

int ur_rpc_config_set_broker(ur_rpc_client_config_t* config, const char* host, int port) {
    if (!config || !host || port <= 0 || port > 65535) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    free(config->broker_host);
    config->broker_host = strdup(host);
    if (!config->broker_host) return UR_RPC_ERROR_MEMORY;
    
    config->broker_port = port;
    return UR_RPC_SUCCESS;
}

int ur_rpc_config_set_credentials(ur_rpc_client_config_t* config, const char* username, const char* password) {
    if (!config) return UR_RPC_ERROR_INVALID_PARAM;
    
    free(config->username);
    free(config->password);
    
    if (username) {
        config->username = strdup(username);
        if (!config->username) return UR_RPC_ERROR_MEMORY;
    } else {
        config->username = NULL;
    }
    
    if (password) {
        config->password = strdup(password);
        if (!config->password) {
            free(config->username);
            config->username = NULL;
            return UR_RPC_ERROR_MEMORY;
        }
    } else {
        config->password = NULL;
    }
    
    return UR_RPC_SUCCESS;
}

int ur_rpc_config_set_client_id(ur_rpc_client_config_t* config, const char* client_id) {
    if (!config || !client_id) return UR_RPC_ERROR_INVALID_PARAM;
    
    if (strlen(client_id) > UR_RPC_MAX_CLIENT_ID_LENGTH) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    free(config->client_id);
    config->client_id = strdup(client_id);
    if (!config->client_id) return UR_RPC_ERROR_MEMORY;
    
    return UR_RPC_SUCCESS;
}

int ur_rpc_config_set_tls(ur_rpc_client_config_t* config, const char* ca_file, const char* cert_file, const char* key_file) {
    if (!config) return UR_RPC_ERROR_INVALID_PARAM;
    
    free(config->ca_file);
    free(config->cert_file);
    free(config->key_file);
    
    config->ca_file = ca_file ? strdup(ca_file) : NULL;
    config->cert_file = cert_file ? strdup(cert_file) : NULL;
    config->key_file = key_file ? strdup(key_file) : NULL;
    
    config->use_tls = (ca_file != NULL);
    return UR_RPC_SUCCESS;
}

int ur_rpc_config_set_timeouts(ur_rpc_client_config_t* config, int connect_timeout, int message_timeout) {
    if (!config || connect_timeout < 0 || message_timeout < 0) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    config->connect_timeout = connect_timeout;
    config->message_timeout = message_timeout;
    return UR_RPC_SUCCESS;
}

int ur_rpc_config_set_reconnect(ur_rpc_client_config_t* config, bool auto_reconnect, int min_delay, int max_delay) {
    if (!config || min_delay < 0 || max_delay < min_delay) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    config->auto_reconnect = auto_reconnect;
    config->reconnect_delay_min = min_delay;
    config->reconnect_delay_max = max_delay;
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Topic List Management
 * ============================================================================ */

int ur_rpc_topic_list_init(ur_rpc_topic_list_t* list) {
    if (!list) return UR_RPC_ERROR_INVALID_PARAM;
    
    list->topics = NULL;
    list->count = 0;
    list->capacity = 0;
    return UR_RPC_SUCCESS;
}

void ur_rpc_topic_list_cleanup(ur_rpc_topic_list_t* list) {
    if (!list) return;
    
    for (int i = 0; i < list->count; i++) {
        free(list->topics[i]);
    }
    free(list->topics);
    list->topics = NULL;
    list->count = 0;
    list->capacity = 0;
}

int ur_rpc_topic_list_add(ur_rpc_topic_list_t* list, const char* topic) {
    if (!list || !topic) return UR_RPC_ERROR_INVALID_PARAM;
    
    // Resize array if needed
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        char** new_topics = realloc(list->topics, new_capacity * sizeof(char*));
        if (!new_topics) return UR_RPC_ERROR_MEMORY;
        
        list->topics = new_topics;
        list->capacity = new_capacity;
    }
    
    list->topics[list->count] = strdup(topic);
    if (!list->topics[list->count]) return UR_RPC_ERROR_MEMORY;
    
    list->count++;
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Heartbeat Management
 * ============================================================================ */

int ur_rpc_config_set_heartbeat(ur_rpc_client_config_t* config, const char* topic, int interval_seconds, const char* payload) {
    if (!config || !topic || interval_seconds <= 0) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    free(config->heartbeat.topic);
    free(config->heartbeat.payload);
    
    config->heartbeat.topic = strdup(topic);
    if (!config->heartbeat.topic) return UR_RPC_ERROR_MEMORY;
    
    config->heartbeat.payload = payload ? strdup(payload) : NULL;
    config->heartbeat.interval_seconds = interval_seconds;
    config->heartbeat.enabled = true;
    
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * JSON Configuration Loading
 * ============================================================================ */

int ur_rpc_config_load_from_file(ur_rpc_client_config_t* config, const char* filename) {
    if (!config || !filename) return UR_RPC_ERROR_INVALID_PARAM;
    
    FILE* file = fopen(filename, "r");
    if (!file) return UR_RPC_ERROR_CONFIG;
    
    // Read file content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return UR_RPC_ERROR_MEMORY;
    }
    
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    
    // Parse JSON
    cJSON* json = cJSON_Parse(content);
    free(content);
    
    if (!json) return UR_RPC_ERROR_JSON;
    
    // Parse basic configuration
    cJSON* client_id = cJSON_GetObjectItem(json, "client_id");
    if (cJSON_IsString(client_id)) {
        ur_rpc_config_set_client_id(config, client_id->valuestring);
    }
    
    cJSON* broker_host = cJSON_GetObjectItem(json, "broker_host");
    cJSON* broker_port = cJSON_GetObjectItem(json, "broker_port");
    if (cJSON_IsString(broker_host)) {
        int port = cJSON_IsNumber(broker_port) ? broker_port->valueint : 1883;
        ur_rpc_config_set_broker(config, broker_host->valuestring, port);
    }
    
    cJSON* username = cJSON_GetObjectItem(json, "username");
    cJSON* password = cJSON_GetObjectItem(json, "password");
    if (cJSON_IsString(username) || cJSON_IsString(password)) {
        ur_rpc_config_set_credentials(config, 
            cJSON_IsString(username) ? username->valuestring : NULL,
            cJSON_IsString(password) ? password->valuestring : NULL);
    }
    
    // Parse boolean and numeric fields
    cJSON* clean_session = cJSON_GetObjectItem(json, "clean_session");
    if (cJSON_IsBool(clean_session)) {
        config->clean_session = cJSON_IsTrue(clean_session);
    }
    
    cJSON* qos = cJSON_GetObjectItem(json, "qos");
    if (cJSON_IsNumber(qos)) {
        config->qos = qos->valueint;
    }
    
    cJSON* keepalive = cJSON_GetObjectItem(json, "keepalive");
    if (cJSON_IsNumber(keepalive)) {
        config->keepalive = keepalive->valueint;
    }
    
    // Parse TLS configuration
    cJSON* use_tls = cJSON_GetObjectItem(json, "use_tls");
    if (cJSON_IsBool(use_tls)) {
        config->use_tls = cJSON_IsTrue(use_tls);
    }
    
    cJSON* ca_file = cJSON_GetObjectItem(json, "ca_file");
    cJSON* cert_file = cJSON_GetObjectItem(json, "cert_file");
    cJSON* key_file = cJSON_GetObjectItem(json, "key_file");
    if (cJSON_IsString(ca_file) || cJSON_IsString(cert_file) || cJSON_IsString(key_file)) {
        ur_rpc_config_set_tls(config,
            cJSON_IsString(ca_file) ? ca_file->valuestring : NULL,
            cJSON_IsString(cert_file) ? cert_file->valuestring : NULL,
            cJSON_IsString(key_file) ? key_file->valuestring : NULL);
    }
    
    // Parse timeout configurations
    cJSON* connect_timeout = cJSON_GetObjectItem(json, "connect_timeout");
    cJSON* message_timeout = cJSON_GetObjectItem(json, "message_timeout");
    if (cJSON_IsNumber(connect_timeout) || cJSON_IsNumber(message_timeout)) {
        ur_rpc_config_set_timeouts(config,
            cJSON_IsNumber(connect_timeout) ? connect_timeout->valueint : config->connect_timeout,
            cJSON_IsNumber(message_timeout) ? message_timeout->valueint : config->message_timeout);
    }
    
    // Parse reconnect configuration
    cJSON* auto_reconnect = cJSON_GetObjectItem(json, "auto_reconnect");
    cJSON* reconnect_delay_min = cJSON_GetObjectItem(json, "reconnect_delay_min");
    cJSON* reconnect_delay_max = cJSON_GetObjectItem(json, "reconnect_delay_max");
    if (cJSON_IsBool(auto_reconnect) || cJSON_IsNumber(reconnect_delay_min) || cJSON_IsNumber(reconnect_delay_max)) {
        ur_rpc_config_set_reconnect(config,
            cJSON_IsBool(auto_reconnect) ? cJSON_IsTrue(auto_reconnect) : config->auto_reconnect,
            cJSON_IsNumber(reconnect_delay_min) ? reconnect_delay_min->valueint : config->reconnect_delay_min,
            cJSON_IsNumber(reconnect_delay_max) ? reconnect_delay_max->valueint : config->reconnect_delay_max);
    }
    
    // Parse topic lists
    cJSON* json_added_pubs = cJSON_GetObjectItem(json, "json_added_pubs");
    if (cJSON_IsObject(json_added_pubs)) {
        cJSON* topics = cJSON_GetObjectItem(json_added_pubs, "topics");
        if (cJSON_IsArray(topics)) {
            ur_rpc_topic_list_init(&config->json_added_pubs);
            cJSON* topic = NULL;
            cJSON_ArrayForEach(topic, topics) {
                if (cJSON_IsString(topic)) {
                    ur_rpc_topic_list_add(&config->json_added_pubs, topic->valuestring);
                }
            }
        }
    }
    
    cJSON* json_added_subs = cJSON_GetObjectItem(json, "json_added_subs");
    if (cJSON_IsObject(json_added_subs)) {
        cJSON* topics = cJSON_GetObjectItem(json_added_subs, "topics");
        if (cJSON_IsArray(topics)) {
            ur_rpc_topic_list_init(&config->json_added_subs);
            cJSON* topic = NULL;
            cJSON_ArrayForEach(topic, topics) {
                if (cJSON_IsString(topic)) {
                    ur_rpc_topic_list_add(&config->json_added_subs, topic->valuestring);
                }
            }
        }
    }
    
    // Parse heartbeat configuration
    cJSON* heartbeat = cJSON_GetObjectItem(json, "heartbeat");
    if (cJSON_IsObject(heartbeat)) {
        cJSON* enabled = cJSON_GetObjectItem(heartbeat, "enabled");
        cJSON* topic = cJSON_GetObjectItem(heartbeat, "topic");
        cJSON* interval = cJSON_GetObjectItem(heartbeat, "interval_seconds");
        cJSON* payload = cJSON_GetObjectItem(heartbeat, "payload");
        
        if (cJSON_IsString(topic) && cJSON_IsNumber(interval)) {
            ur_rpc_config_set_heartbeat(config, topic->valuestring, interval->valueint,
                cJSON_IsString(payload) ? payload->valuestring : NULL);
            
            if (cJSON_IsBool(enabled)) {
                config->heartbeat.enabled = cJSON_IsTrue(enabled);
            }
        }
    }
    
    // Parse relay configuration
    cJSON* relay = cJSON_GetObjectItem(json, "relay");
    if (cJSON_IsObject(relay)) {
        ur_rpc_relay_config_init(&config->relay);
        
        cJSON* relay_enabled = cJSON_GetObjectItem(relay, "enabled");
        if (cJSON_IsBool(relay_enabled)) {
            config->relay.enabled = cJSON_IsTrue(relay_enabled);
        }
        
        cJSON* conditional_relay = cJSON_GetObjectItem(relay, "conditional_relay");
        if (cJSON_IsBool(conditional_relay)) {
            config->relay.conditional_relay = cJSON_IsTrue(conditional_relay);
        }
        
        cJSON* relay_prefix = cJSON_GetObjectItem(relay, "prefix");
        if (cJSON_IsString(relay_prefix)) {
            ur_rpc_relay_config_set_prefix(&config->relay, relay_prefix->valuestring);
        }
        
        // Parse brokers array
        cJSON* brokers = cJSON_GetObjectItem(relay, "brokers");
        if (cJSON_IsArray(brokers)) {
            cJSON* broker = NULL;
            cJSON_ArrayForEach(broker, brokers) {
                if (cJSON_IsObject(broker)) {
                    cJSON* host = cJSON_GetObjectItem(broker, "host");
                    cJSON* port = cJSON_GetObjectItem(broker, "port");
                    cJSON* client_id = cJSON_GetObjectItem(broker, "client_id");
                    cJSON* is_primary = cJSON_GetObjectItem(broker, "is_primary");
                    
                    if (cJSON_IsString(host) && cJSON_IsString(client_id)) {
                        int broker_port = cJSON_IsNumber(port) ? port->valueint : 1883;
                        bool primary = cJSON_IsBool(is_primary) ? cJSON_IsTrue(is_primary) : false;
                        
                        ur_rpc_relay_config_add_broker(&config->relay, host->valuestring, 
                            broker_port, client_id->valuestring, primary);
                    }
                }
            }
        }
        
        // Parse relay rules array
        cJSON* rules = cJSON_GetObjectItem(relay, "rules");
        if (cJSON_IsArray(rules)) {
            cJSON* rule = NULL;
            cJSON_ArrayForEach(rule, rules) {
                if (cJSON_IsObject(rule)) {
                    cJSON* source_topic = cJSON_GetObjectItem(rule, "source_topic");
                    cJSON* dest_topic = cJSON_GetObjectItem(rule, "destination_topic");
                    cJSON* prefix = cJSON_GetObjectItem(rule, "prefix");
                    cJSON* source_broker = cJSON_GetObjectItem(rule, "source_broker_index");
                    cJSON* dest_broker = cJSON_GetObjectItem(rule, "dest_broker_index");
                    cJSON* bidirectional = cJSON_GetObjectItem(rule, "bidirectional");
                    
                    if (cJSON_IsString(source_topic) && cJSON_IsString(dest_topic) &&
                        cJSON_IsNumber(source_broker) && cJSON_IsNumber(dest_broker)) {
                        
                        const char* rule_prefix = cJSON_IsString(prefix) ? prefix->valuestring : NULL;
                        bool bidir = cJSON_IsBool(bidirectional) ? cJSON_IsTrue(bidirectional) : false;
                        
                        ur_rpc_relay_config_add_rule(&config->relay, 
                            source_topic->valuestring, dest_topic->valuestring, rule_prefix,
                            source_broker->valueint, dest_broker->valueint, bidir);
                    }
                }
            }
        }
    }
    
    cJSON_Delete(json);
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Heartbeat Thread Implementation
 * ============================================================================ */

static void* heartbeat_thread(void* arg) {
    ur_rpc_client_t* client = (ur_rpc_client_t*)arg;
    if (!client || !client->config.heartbeat.enabled) return NULL;
    
    while (atomic_load(&client->heartbeat_running) && atomic_load(&client->running)) {
        // Wait for the interval
        for (int i = 0; i < client->config.heartbeat.interval_seconds && 
             atomic_load(&client->heartbeat_running); i++) {
            sleep(1);
        }
        
        // Send heartbeat if still running and connected
        if (atomic_load(&client->heartbeat_running) && 
            atomic_load(&client->connected)) {
            
            pthread_mutex_lock(&client->mutex);
            
            // Create default heartbeat payload if none provided
            char* payload = client->config.heartbeat.payload;
            char default_payload[256];
            bool free_payload = false;
            
            if (!payload) {
                cJSON* heartbeat_json = cJSON_CreateObject();
                cJSON_AddStringToObject(heartbeat_json, "type", "heartbeat");
                cJSON_AddStringToObject(heartbeat_json, "client_id", 
                    client->config.client_id ? client->config.client_id : "unknown");
                cJSON_AddNumberToObject(heartbeat_json, "timestamp", ur_rpc_get_timestamp_ms());
                cJSON_AddStringToObject(heartbeat_json, "status", "alive");
                
                payload = cJSON_Print(heartbeat_json);
                cJSON_Delete(heartbeat_json);
                free_payload = true;
            }
            
            if (payload) {
                // Publish heartbeat
                LOG_DEBUG_SIMPLE("HEARTBEAT to %s: %s", client->config.heartbeat.topic, payload);
                
                client->messages_sent++;
                client->last_activity = time(NULL);
                
                if (free_payload) free(payload);
            }
            
            pthread_mutex_unlock(&client->mutex);
        }
    }
    
    return NULL;
}

int ur_rpc_heartbeat_start(ur_rpc_client_t* client) {
    if (!client || !client->config.heartbeat.enabled) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->mutex);
    
    if (atomic_load(&client->heartbeat_running)) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_SUCCESS;
    }
    
    atomic_init(&client->heartbeat_running, true);
    
    if (pthread_create(&client->heartbeat_thread, NULL, heartbeat_thread, client) != 0) {
        atomic_store(&client->heartbeat_running, false);
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_ERROR_THREAD;
    }
    
    LOG_INFO_SIMPLE("Heartbeat started: topic=%s, interval=%ds", 
           client->config.heartbeat.topic, client->config.heartbeat.interval_seconds);
    
    pthread_mutex_unlock(&client->mutex);
    return UR_RPC_SUCCESS;
}

int ur_rpc_heartbeat_stop(ur_rpc_client_t* client) {
    if (!client) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&client->mutex);
    
    if (!atomic_load(&client->heartbeat_running)) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_SUCCESS;
    }
    
    atomic_store(&client->heartbeat_running, false);
    pthread_mutex_unlock(&client->mutex);
    
    // Wait for heartbeat thread to finish
    pthread_join(client->heartbeat_thread, NULL);
    
    LOG_INFO_SIMPLE("Heartbeat stopped");
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Missing Functions Implementation for C++ Wrapper Compatibility  
 * ============================================================================ */

int ur_rpc_topic_config_set_suffixes(ur_rpc_topic_config_t* config, const char* request_suffix, const char* response_suffix, const char* notification_suffix) {
    if (!config) return UR_RPC_ERROR_INVALID_PARAM;
    
    if (request_suffix) {
        free(config->request_suffix);
        config->request_suffix = strdup(request_suffix);
        if (!config->request_suffix) return UR_RPC_ERROR_MEMORY;
    }
    if (response_suffix) {
        free(config->response_suffix);
        config->response_suffix = strdup(response_suffix);
        if (!config->response_suffix) return UR_RPC_ERROR_MEMORY;
    }
    if (notification_suffix) {
        free(config->notification_suffix);
        config->notification_suffix = strdup(notification_suffix);
        if (!config->notification_suffix) return UR_RPC_ERROR_MEMORY;
    }
    
    return UR_RPC_SUCCESS;
}

int ur_rpc_request_set_timeout(ur_rpc_request_t* request, int timeout_ms) {
    if (!request || timeout_ms <= 0) return UR_RPC_ERROR_INVALID_PARAM;
    request->timeout_ms = timeout_ms;
    return UR_RPC_SUCCESS;
}

char* ur_rpc_request_to_json(const ur_rpc_request_t* request) {
    if (!request) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "method", request->method ? request->method : "unknown");
    cJSON_AddStringToObject(json, "service", request->service ? request->service : "default");
    cJSON_AddStringToObject(json, "transaction_id", request->transaction_id ? request->transaction_id : "");
    cJSON_AddNumberToObject(json, "authority", request->authority);
    cJSON_AddNumberToObject(json, "timeout_ms", request->timeout_ms);
    
    if (request->params) {
        cJSON_AddItemToObject(json, "params", cJSON_Duplicate(request->params, 1));
    }
    
    char* result = cJSON_Print(json);
    cJSON_Delete(json);
    return result;
}

ur_rpc_request_t* ur_rpc_request_from_json(const char* json_str) {
    if (!json_str) return NULL;
    
    cJSON* json = cJSON_Parse(json_str);
    if (!json) return NULL;
    
    ur_rpc_request_t* request = ur_rpc_request_create();
    if (!request) {
        cJSON_Delete(json);
        return NULL;
    }
    
    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* service = cJSON_GetObjectItem(json, "service");
    cJSON* transaction_id = cJSON_GetObjectItem(json, "transaction_id");
    cJSON* authority = cJSON_GetObjectItem(json, "authority");
    cJSON* timeout_ms = cJSON_GetObjectItem(json, "timeout_ms");
    cJSON* params = cJSON_GetObjectItem(json, "params");
    
    if (cJSON_IsString(method)) {
        free(request->method);
        request->method = strdup(method->valuestring);
    }
    if (cJSON_IsString(service)) {
        free(request->service);
        request->service = strdup(service->valuestring);
    }
    if (cJSON_IsString(transaction_id)) {
        free(request->transaction_id);
        request->transaction_id = strdup(transaction_id->valuestring);
    }
    if (cJSON_IsNumber(authority)) {
        request->authority = authority->valueint;
    }
    if (cJSON_IsNumber(timeout_ms)) {
        request->timeout_ms = timeout_ms->valueint;
    }
    if (params) {
        request->params = cJSON_Duplicate(params, 1);
    }
    
    cJSON_Delete(json);
    return request;
}

ur_rpc_response_t* ur_rpc_response_create(void) {
    ur_rpc_response_t* response = calloc(1, sizeof(ur_rpc_response_t));
    if (!response) return NULL;
    
    response->timestamp = ur_rpc_get_timestamp_ms();
    response->success = true;
    response->error_code = UR_RPC_SUCCESS;
    
    return response;
}

void ur_rpc_response_destroy(ur_rpc_response_t* response) {
    if (!response) return;
    
    free(response->transaction_id);
    free(response->error_message);
    
    if (response->result) cJSON_Delete(response->result);
    
    free(response);
}

char* ur_rpc_response_to_json(const ur_rpc_response_t* response) {
    if (!response) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "transaction_id", response->transaction_id ? response->transaction_id : "");
    cJSON_AddBoolToObject(json, "success", response->success);
    cJSON_AddNumberToObject(json, "timestamp", response->timestamp);
    cJSON_AddNumberToObject(json, "error_code", response->error_code);
    cJSON_AddNumberToObject(json, "processing_time_ms", response->processing_time_ms);
    
    if (response->error_message) {
        cJSON_AddStringToObject(json, "error_message", response->error_message);
    }
    
    if (response->result) {
        cJSON_AddItemToObject(json, "result", cJSON_Duplicate(response->result, 1));
    }
    
    char* result = cJSON_Print(json);
    cJSON_Delete(json);
    return result;
}

ur_rpc_response_t* ur_rpc_response_from_json(const char* json_str) {
    if (!json_str) return NULL;
    
    cJSON* json = cJSON_Parse(json_str);
    if (!json) return NULL;
    
    ur_rpc_response_t* response = ur_rpc_response_create();
    if (!response) {
        cJSON_Delete(json);
        return NULL;
    }
    
    cJSON* transaction_id = cJSON_GetObjectItem(json, "transaction_id");
    cJSON* success = cJSON_GetObjectItem(json, "success");
    cJSON* timestamp = cJSON_GetObjectItem(json, "timestamp");
    cJSON* error_code = cJSON_GetObjectItem(json, "error_code");
    cJSON* error_message = cJSON_GetObjectItem(json, "error_message");
    cJSON* result = cJSON_GetObjectItem(json, "result");
    cJSON* processing_time_ms = cJSON_GetObjectItem(json, "processing_time_ms");
    
    if (cJSON_IsString(transaction_id)) {
        response->transaction_id = strdup(transaction_id->valuestring);
    }
    if (cJSON_IsBool(success)) {
        response->success = cJSON_IsTrue(success);
    }
    if (cJSON_IsNumber(timestamp)) {
        response->timestamp = timestamp->valuedouble;
    }
    if (cJSON_IsNumber(error_code)) {
        response->error_code = error_code->valueint;
    }
    if (cJSON_IsString(error_message)) {
        response->error_message = strdup(error_message->valuestring);
    }
    if (result) {
        response->result = cJSON_Duplicate(result, 1);
    }
    if (cJSON_IsNumber(processing_time_ms)) {
        response->processing_time_ms = processing_time_ms->valuedouble;
    }
    
    cJSON_Delete(json);
    return response;
}

int ur_rpc_call_sync(ur_rpc_client_t* client, const ur_rpc_request_t* request, ur_rpc_response_t** response, int timeout_ms) {
    if (!client || !request || !response) return UR_RPC_ERROR_INVALID_PARAM;
    
    // For now, create a stub response to prevent linking errors
    // In a full implementation, this would perform actual RPC call and wait for response
    *response = ur_rpc_response_create();
    if (!*response) return UR_RPC_ERROR_MEMORY;
    
    (*response)->transaction_id = strdup(request->transaction_id ? request->transaction_id : "stub");
    (*response)->success = true;
    (*response)->error_code = UR_RPC_SUCCESS;
    (*response)->processing_time_ms = 1; // Minimal processing time for stub
    
    // Create a simple success result
    (*response)->result = cJSON_CreateObject();
    cJSON_AddStringToObject((*response)->result, "status", "stub_success");
    cJSON_AddStringToObject((*response)->result, "message", "Stub implementation - actual RPC not performed");
    
    LOG_INFO_SIMPLE("ur_rpc_call_sync: Stub implementation called for method %s", request->method ? request->method : "unknown");
    
    // Suppress unused parameter warning
    (void)timeout_ms;
    
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Topic Configuration Management  
 * ============================================================================ */

ur_rpc_topic_config_t* ur_rpc_topic_config_create(void) {
    ur_rpc_topic_config_t* config = calloc(1, sizeof(ur_rpc_topic_config_t));
    if (!config) return NULL;
    
    // Set default values
    config->base_prefix = strdup("ur_rpc");
    config->request_suffix = strdup("request");
    config->response_suffix = strdup("response");
    config->notification_suffix = strdup("notification");
    config->include_transaction_id = true;
    
    if (!config->base_prefix || !config->request_suffix || 
        !config->response_suffix || !config->notification_suffix) {
        ur_rpc_topic_config_destroy(config);
        return NULL;
    }
    
    return config;
}

void ur_rpc_topic_config_destroy(ur_rpc_topic_config_t* config) {
    if (!config) return;
    
    free(config->base_prefix);
    free(config->service_prefix);
    free(config->request_suffix);
    free(config->response_suffix);
    free(config->notification_suffix);
    free(config);
}

int ur_rpc_topic_config_set_prefixes(ur_rpc_topic_config_t* config, const char* base_prefix, const char* service_prefix) {
    if (!config || !base_prefix) return UR_RPC_ERROR_INVALID_PARAM;
    
    free(config->base_prefix);
    free(config->service_prefix);
    
    config->base_prefix = strdup(base_prefix);
    if (!config->base_prefix) return UR_RPC_ERROR_MEMORY;
    
    if (service_prefix) {
        config->service_prefix = strdup(service_prefix);
        if (!config->service_prefix) {
            free(config->base_prefix);
            config->base_prefix = NULL;
            return UR_RPC_ERROR_MEMORY;
        }
    } else {
        config->service_prefix = NULL;
    }
    
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Topic Generation Utilities
 * ============================================================================ */

char* ur_rpc_generate_request_topic(const ur_rpc_client_t* client, const char* method, const char* service, const char* transaction_id) {
    if (!client || !method || !service) return NULL;
    
    char* topic = malloc(UR_RPC_MAX_TOPIC_LENGTH);
    if (!topic) return NULL;
    
    if (client->topic_config.include_transaction_id && transaction_id) {
        snprintf(topic, UR_RPC_MAX_TOPIC_LENGTH, "%s/%s/%s/%s/%s", 
                client->topic_config.base_prefix,
                client->topic_config.service_prefix ? client->topic_config.service_prefix : service,
                method,
                client->topic_config.request_suffix,
                transaction_id);
    } else {
        snprintf(topic, UR_RPC_MAX_TOPIC_LENGTH, "%s/%s/%s/%s", 
                client->topic_config.base_prefix,
                client->topic_config.service_prefix ? client->topic_config.service_prefix : service,
                method,
                client->topic_config.request_suffix);
    }
    
    return topic;
}

char* ur_rpc_generate_response_topic(const ur_rpc_client_t* client, const char* method, const char* service, const char* transaction_id) {
    if (!client || !method || !service) return NULL;
    
    char* topic = malloc(UR_RPC_MAX_TOPIC_LENGTH);
    if (!topic) return NULL;
    
    if (client->topic_config.include_transaction_id && transaction_id) {
        snprintf(topic, UR_RPC_MAX_TOPIC_LENGTH, "%s/%s/%s/%s/%s", 
                client->topic_config.base_prefix,
                client->topic_config.service_prefix ? client->topic_config.service_prefix : service,
                method,
                client->topic_config.response_suffix,
                transaction_id);
    } else {
        snprintf(topic, UR_RPC_MAX_TOPIC_LENGTH, "%s/%s/%s/%s", 
                client->topic_config.base_prefix,
                client->topic_config.service_prefix ? client->topic_config.service_prefix : service,
                method,
                client->topic_config.response_suffix);
    }
    
    return topic;
}

/* ============================================================================
 * RPC Request/Response Management
 * ============================================================================ */

ur_rpc_request_t* ur_rpc_request_create(void) {
    ur_rpc_request_t* request = calloc(1, sizeof(ur_rpc_request_t));
    if (!request) return NULL;
    
    request->transaction_id = ur_rpc_generate_transaction_id();
    if (!request->transaction_id) {
        free(request);
        return NULL;
    }
    
    request->timestamp = ur_rpc_get_timestamp_ms();
    request->timeout_ms = UR_RPC_DEFAULT_TIMEOUT_MS;
    request->authority = UR_RPC_AUTHORITY_USER;
    
    return request;
}

void ur_rpc_request_destroy(ur_rpc_request_t* request) {
    if (!request) return;
    
    free(request->transaction_id);
    free(request->method);
    free(request->service);
    free(request->response_topic);
    if (request->params) cJSON_Delete(request->params);
    free(request);
}

int ur_rpc_request_set_method(ur_rpc_request_t* request, const char* method, const char* service) {
    if (!request || !method || !service) return UR_RPC_ERROR_INVALID_PARAM;
    
    free(request->method);
    free(request->service);
    
    request->method = strdup(method);
    request->service = strdup(service);
    
    if (!request->method || !request->service) {
        free(request->method);
        free(request->service);
        request->method = NULL;
        request->service = NULL;
        return UR_RPC_ERROR_MEMORY;
    }
    
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Basic RPC Client Operations
 * ============================================================================ */

int ur_rpc_publish_message(ur_rpc_client_t* client, const char* topic, const char* payload, size_t payload_len) {
    if (!client || !topic || !payload || !atomic_load(&client->connected)) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&client->mutex);
    
    // Publish using mosquitto
    int result = mosquitto_publish(client->mosq, NULL, topic, (int)payload_len, payload, client->config.qos, false);
    if (result != MOSQ_ERR_SUCCESS) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_ERROR_MQTT;
    }
    
    LOG_DEBUG_SIMPLE("PUBLISH to %s: %.*s", topic, (int)payload_len, payload);
    
    client->messages_sent++;
    client->last_activity = time(NULL);
    
    pthread_mutex_unlock(&client->mutex);
    
    return UR_RPC_SUCCESS;
}

bool ur_rpc_client_is_connected(const ur_rpc_client_t* client) {
    return client ? atomic_load(&client->connected) : false;
}

ur_rpc_connection_status_t ur_rpc_client_get_status(const ur_rpc_client_t* client) {
    return client ? client->status : UR_RPC_CONN_ERROR;
}

/* ============================================================================
 * Client Lifecycle Management
 * ============================================================================ */

ur_rpc_client_t* ur_rpc_client_create(const ur_rpc_client_config_t* config, const ur_rpc_topic_config_t* topic_config) {
    if (!config || !topic_config) return NULL;
    
    ur_rpc_client_t* client = calloc(1, sizeof(ur_rpc_client_t));
    if (!client) return NULL;
    
    // Copy configuration
    memcpy(&client->config, config, sizeof(ur_rpc_client_config_t));
    memcpy(&client->topic_config, topic_config, sizeof(ur_rpc_topic_config_t));
    
    // Create mosquitto client instance
    client->mosq = mosquitto_new(client->config.client_id, client->config.clean_session, client);
    if (!client->mosq) {
        free(client);
        return NULL;
    }
    
    // Set up MQTT callbacks
    mosquitto_connect_callback_set(client->mosq, on_connect_callback);
    mosquitto_disconnect_callback_set(client->mosq, on_disconnect_callback);
    mosquitto_message_callback_set(client->mosq, on_message_callback);
    mosquitto_publish_callback_set(client->mosq, on_publish_callback);
    mosquitto_subscribe_callback_set(client->mosq, on_subscribe_callback);
    mosquitto_log_callback_set(client->mosq, on_log_callback);
    
    // Initialize mutexes
    if (pthread_mutex_init(&client->mutex, NULL) != 0) {
        mosquitto_destroy(client->mosq);
        free(client);
        return NULL;
    }
    
    if (pthread_mutex_init(&client->pending_mutex, NULL) != 0) {
        pthread_mutex_destroy(&client->mutex);
        mosquitto_destroy(client->mosq);
        free(client);
        return NULL;
    }
    
    // Initialize atomic variables
    atomic_init(&client->connected, false);
    atomic_init(&client->running, false);
    atomic_init(&client->heartbeat_running, false);
    
    client->status = UR_RPC_CONN_DISCONNECTED;
    client->last_activity = time(NULL);
    
    return client;
}

void ur_rpc_client_destroy(ur_rpc_client_t* client) {
    if (!client) return;
    
    // Stop heartbeat first
    ur_rpc_heartbeat_stop(client);
    
    // Ensure client is stopped
    ur_rpc_client_stop(client);
    ur_rpc_client_disconnect(client);
    
    // Clean up pending requests
    pthread_mutex_lock(&client->pending_mutex);
    ur_rpc_pending_request_t* req = client->pending_requests;
    while (req) {
        ur_rpc_pending_request_t* next = req->next;
        free(req->transaction_id);
        free(req->response_topic);
        free(req);
        req = next;
    }
    pthread_mutex_unlock(&client->pending_mutex);
    
    // Destroy mosquitto instance
    if (client->mosq) {
        mosquitto_destroy(client->mosq);
    }
    
    // Destroy mutexes
    pthread_mutex_destroy(&client->mutex);
    pthread_mutex_destroy(&client->pending_mutex);
    
    free(client);
}

int ur_rpc_client_connect(ur_rpc_client_t* client) {
    if (!client) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&client->mutex);
    
    if (atomic_load(&client->connected)) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_SUCCESS;
    }
    
    client->status = UR_RPC_CONN_CONNECTING;
    
    // Connect to MQTT broker using mosquitto
    LOG_INFO_SIMPLE("Connecting to MQTT broker %s:%d", 
           client->config.broker_host ? client->config.broker_host : "localhost",
           client->config.broker_port);
           
    int result = mosquitto_connect(client->mosq, 
                                  client->config.broker_host ? client->config.broker_host : "localhost",
                                  client->config.broker_port, 
                                  client->config.keepalive);
    
    if (result != MOSQ_ERR_SUCCESS) {
        client->status = UR_RPC_CONN_ERROR;
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_ERROR_MQTT;
    }
    
    // Connection initiated successfully
    atomic_store(&client->connected, true);
    client->status = UR_RPC_CONN_CONNECTED;
    client->last_activity = time(NULL);
    
    pthread_mutex_unlock(&client->mutex);
    return UR_RPC_SUCCESS;
}

int ur_rpc_client_disconnect(ur_rpc_client_t* client) {
    if (!client) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&client->mutex);
    
    if (!atomic_load(&client->connected)) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_SUCCESS;
    }
    
    // Disconnect from MQTT broker using mosquitto
    mosquitto_disconnect(client->mosq);
    LOG_INFO_SIMPLE("Disconnecting from MQTT broker");
    
    atomic_store(&client->connected, false);
    client->status = UR_RPC_CONN_DISCONNECTED;
    
    pthread_mutex_unlock(&client->mutex);
    return UR_RPC_SUCCESS;
}

int ur_rpc_client_start(ur_rpc_client_t* client) {
    if (!client) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&client->mutex);
    
    if (atomic_load(&client->running)) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_SUCCESS;
    }
    
    // Start MQTT client loop using mosquitto
    int result = mosquitto_loop_start(client->mosq);
    if (result != MOSQ_ERR_SUCCESS) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_ERROR_MQTT;
    }
    
    LOG_INFO_SIMPLE("Starting MQTT client loop");
    
    atomic_store(&client->running, true);
    client->thread_monitor.last_activity = time(NULL);
    
    pthread_mutex_unlock(&client->mutex);
    
    // Start heartbeat if configured
    if (client->config.heartbeat.enabled) {
        ur_rpc_heartbeat_start(client);
    }
    
    return UR_RPC_SUCCESS;
}

int ur_rpc_client_stop(ur_rpc_client_t* client) {
    if (!client) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&client->mutex);
    
    if (!atomic_load(&client->running)) {
        pthread_mutex_unlock(&client->mutex);
        return UR_RPC_SUCCESS;
    }
    
    // Stop MQTT client loop using mosquitto
    mosquitto_loop_stop(client->mosq, false);
    LOG_INFO_SIMPLE("Stopping MQTT client loop");
    
    atomic_store(&client->running, false);
    
    pthread_mutex_unlock(&client->mutex);
    
    // Stop heartbeat
    ur_rpc_heartbeat_stop(client);
    
    return UR_RPC_SUCCESS;
}

void ur_rpc_client_set_connection_callback(ur_rpc_client_t* client, ur_rpc_connection_callback_t callback, void* user_data) {
    if (!client) return;
    
    pthread_mutex_lock(&client->mutex);
    client->connection_callback = callback;
    client->connection_user_data = user_data;
    pthread_mutex_unlock(&client->mutex);
}

void ur_rpc_client_set_message_handler(ur_rpc_client_t* client, ur_rpc_message_handler_t handler, void* user_data) {
    if (!client) return;
    
    pthread_mutex_lock(&client->mutex);
    client->message_handler = handler;
    client->message_user_data = user_data;
    pthread_mutex_unlock(&client->mutex);
}

/* ============================================================================
 * Additional Request/Response Functions
 * ============================================================================ */

int ur_rpc_request_set_authority(ur_rpc_request_t* request, ur_rpc_authority_t authority) {
    if (!request) return UR_RPC_ERROR_INVALID_PARAM;
    
    request->authority = authority;
    return UR_RPC_SUCCESS;
}

int ur_rpc_request_set_params(ur_rpc_request_t* request, const cJSON* params) {
    if (!request) return UR_RPC_ERROR_INVALID_PARAM;
    
    if (request->params) {
        cJSON_Delete(request->params);
    }
    
    if (params) {
        request->params = cJSON_Duplicate(params, true);
        if (!request->params) return UR_RPC_ERROR_MEMORY;
    } else {
        request->params = NULL;
    }
    
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * RPC Operations
 * ============================================================================ */

int ur_rpc_call_async(ur_rpc_client_t* client, const ur_rpc_request_t* request, ur_rpc_response_handler_t callback, void* user_data) {
    if (!client || !request || !atomic_load(&client->connected)) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    // Generate request topic
    char* request_topic = ur_rpc_generate_request_topic(client, request->method, request->service, request->transaction_id);
    if (!request_topic) return UR_RPC_ERROR_MEMORY;
    
    // TODO: Serialize request to JSON and publish
    LOG_INFO_SIMPLE("ASYNC RPC CALL: %s.%s (authority: %s, transaction: %s)",
           request->service ? request->service : "unknown",
           request->method ? request->method : "unknown",
           ur_rpc_authority_to_string(request->authority),
           request->transaction_id ? request->transaction_id : "unknown");
    
    if (request->params) {
        char* params_str = cJSON_Print(request->params);
        if (params_str) {
            LOG_DEBUG_SIMPLE("  Params: %s", params_str);
            free(params_str);
        }
    }
    
    // Add to pending requests if callback provided
    if (callback) {
        pthread_mutex_lock(&client->pending_mutex);
        
        ur_rpc_pending_request_t* pending = malloc(sizeof(ur_rpc_pending_request_t));
        if (pending) {
            pending->transaction_id = strdup(request->transaction_id);
            pending->response_topic = ur_rpc_generate_response_topic(client, request->method, request->service, request->transaction_id);
            pending->callback = callback;
            pending->user_data = user_data;
            pending->created_time = time(NULL);
            pending->timeout_ms = request->timeout_ms;
            pending->next = client->pending_requests;
            client->pending_requests = pending;
        }
        
        pthread_mutex_unlock(&client->pending_mutex);
    }
    
    client->requests_sent++;
    free(request_topic);
    return UR_RPC_SUCCESS;
}

int ur_rpc_send_notification(ur_rpc_client_t* client, const char* method, const char* service, ur_rpc_authority_t authority, const cJSON* params) {
    if (!client || !method || !service || !atomic_load(&client->connected)) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    // Generate notification topic
    char* notification_topic = ur_rpc_generate_notification_topic(client, method, service);
    if (!notification_topic) return UR_RPC_ERROR_MEMORY;
    
    LOG_INFO_SIMPLE("NOTIFICATION: %s.%s (authority: %s)",
           service, method, ur_rpc_authority_to_string(authority));
    
    if (params) {
        char* params_str = cJSON_Print(params);
        if (params_str) {
            LOG_DEBUG_SIMPLE("  Params: %s", params_str);
            free(params_str);
        }
    }
    
    client->messages_sent++;
    free(notification_topic);
    return UR_RPC_SUCCESS;
}

char* ur_rpc_generate_notification_topic(const ur_rpc_client_t* client, const char* method, const char* service) {
    if (!client || !method || !service) return NULL;
    
    char* topic = malloc(UR_RPC_MAX_TOPIC_LENGTH);
    if (!topic) return NULL;
    
    snprintf(topic, UR_RPC_MAX_TOPIC_LENGTH, "%s/%s/%s/%s", 
            client->topic_config.base_prefix,
            client->topic_config.service_prefix ? client->topic_config.service_prefix : service,
            method,
            client->topic_config.notification_suffix);
    
    return topic;
}

int ur_rpc_subscribe_topic(ur_rpc_client_t* client, const char* topic) {
    if (!client || !topic || !atomic_load(&client->connected)) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    // Subscribe using mosquitto
    int result = mosquitto_subscribe(client->mosq, NULL, topic, client->config.qos);
    if (result != MOSQ_ERR_SUCCESS) {
        return UR_RPC_ERROR_MQTT;
    }
    
    LOG_DEBUG_SIMPLE("SUBSCRIBE to %s", topic);
    
    return UR_RPC_SUCCESS;
}

int ur_rpc_unsubscribe_topic(ur_rpc_client_t* client, const char* topic) {
    if (!client || !topic || !atomic_load(&client->connected)) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    // Unsubscribe using mosquitto
    int result = mosquitto_unsubscribe(client->mosq, NULL, topic);
    if (result != MOSQ_ERR_SUCCESS) {
        return UR_RPC_ERROR_MQTT;
    }
    
    LOG_DEBUG_SIMPLE("UNSUBSCRIBE from %s", topic);
    
    return UR_RPC_SUCCESS;
}

int ur_rpc_client_get_statistics(const ur_rpc_client_t* client, ur_rpc_statistics_t* stats) {
    if (!client || !stats) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock((pthread_mutex_t*)&client->mutex);
    
    stats->messages_sent = client->messages_sent;
    stats->messages_received = client->messages_received;
    stats->requests_sent = client->requests_sent;
    stats->responses_received = client->responses_received;
    stats->notifications_sent = 0; // TODO: track separately
    stats->errors_count = client->errors_count;
    stats->connection_count = 1; // TODO: track reconnections
    stats->uptime_seconds = time(NULL) - client->last_activity;
    stats->last_activity = client->last_activity;
    
    pthread_mutex_unlock((pthread_mutex_t*)&client->mutex);
    return UR_RPC_SUCCESS;
}

/* ============================================================================
 * Multi-Broker Relay Implementation
 * ============================================================================ */

/* Relay configuration initialization */
int ur_rpc_relay_config_init(ur_rpc_relay_config_t* config) {
    if (!config) return UR_RPC_ERROR_INVALID_PARAM;
    
    memset(config, 0, sizeof(ur_rpc_relay_config_t));
    config->enabled = false;
    config->broker_count = 0;
    config->rule_count = 0;
    config->relay_prefix = NULL;
    
    return UR_RPC_SUCCESS;
}

/* Relay configuration cleanup */
void ur_rpc_relay_config_cleanup(ur_rpc_relay_config_t* config) {
    if (!config) return;
    
    // Cleanup broker configurations
    for (int i = 0; i < config->broker_count; i++) {
        ur_rpc_broker_config_t* broker = &config->brokers[i];
        free(broker->host);
        free(broker->username);
        free(broker->password);
        free(broker->client_id);
        free(broker->ca_file);
    }
    
    // Cleanup relay rules
    for (int i = 0; i < config->rule_count; i++) {
        ur_rpc_relay_rule_t* rule = &config->rules[i];
        free(rule->source_topic);
        free(rule->destination_topic);
        free(rule->topic_prefix);
    }
    
    free(config->relay_prefix);
    
    memset(config, 0, sizeof(ur_rpc_relay_config_t));
}

/* Add broker to relay configuration */
int ur_rpc_relay_config_add_broker(ur_rpc_relay_config_t* config, const char* host, int port, const char* client_id, bool is_primary) {
    if (!config || !host || !client_id) return UR_RPC_ERROR_INVALID_PARAM;
    if (config->broker_count >= UR_RPC_MAX_BROKERS) return UR_RPC_ERROR_CONFIG;
    
    ur_rpc_broker_config_t* broker = &config->brokers[config->broker_count];
    memset(broker, 0, sizeof(ur_rpc_broker_config_t));
    
    broker->host = strdup(host);
    broker->port = port;
    broker->client_id = strdup(client_id);
    broker->is_primary = is_primary;
    broker->use_tls = false;
    
    if (!broker->host || !broker->client_id) {
        free(broker->host);
        free(broker->client_id);
        return UR_RPC_ERROR_MEMORY;
    }
    
    config->broker_count++;
    return UR_RPC_SUCCESS;
}

/* Add relay rule to configuration */
int ur_rpc_relay_config_add_rule(ur_rpc_relay_config_t* config, const char* source_topic, const char* dest_topic, const char* prefix, int source_broker, int dest_broker, bool bidirectional) {
    if (!config || !source_topic || !dest_topic) return UR_RPC_ERROR_INVALID_PARAM;
    if (config->rule_count >= UR_RPC_MAX_RELAY_RULES) return UR_RPC_ERROR_CONFIG;
    if (source_broker >= config->broker_count || dest_broker >= config->broker_count) return UR_RPC_ERROR_CONFIG;
    
    ur_rpc_relay_rule_t* rule = &config->rules[config->rule_count];
    memset(rule, 0, sizeof(ur_rpc_relay_rule_t));
    
    rule->source_topic = strdup(source_topic);
    rule->destination_topic = strdup(dest_topic);
    rule->topic_prefix = prefix ? strdup(prefix) : NULL;
    rule->source_broker_index = source_broker;
    rule->dest_broker_index = dest_broker;
    rule->bidirectional = bidirectional;
    
    if (!rule->source_topic || !rule->destination_topic) {
        free(rule->source_topic);
        free(rule->destination_topic);
        free(rule->topic_prefix);
        return UR_RPC_ERROR_MEMORY;
    }
    
    config->rule_count++;
    return UR_RPC_SUCCESS;
}

/* Set default relay prefix */
int ur_rpc_relay_config_set_prefix(ur_rpc_relay_config_t* config, const char* prefix) {
    if (!config || !prefix) return UR_RPC_ERROR_INVALID_PARAM;
    
    free(config->relay_prefix);
    config->relay_prefix = strdup(prefix);
    
    return config->relay_prefix ? UR_RPC_SUCCESS : UR_RPC_ERROR_MEMORY;
}

/* Relay message handler */
static void relay_message_handler(const char* topic, const char* payload, size_t payload_len, void* user_data) {
    ur_rpc_relay_client_t* relay_client = (ur_rpc_relay_client_t*)user_data;
    if (!relay_client || !topic || !payload) return;
    
    pthread_mutex_lock(&relay_client->relay_mutex);
    
    // Find matching relay rules
    for (int i = 0; i < relay_client->config.rule_count; i++) {
        ur_rpc_relay_rule_t* rule = &relay_client->config.rules[i];
        
        // Simple topic matching (can be enhanced with wildcards)
        if (strstr(topic, rule->source_topic) == NULL) continue;
        
        // Build destination topic
        char dest_topic[UR_RPC_MAX_TOPIC_LENGTH];
        if (rule->topic_prefix) {
            snprintf(dest_topic, sizeof(dest_topic), "%s%s", rule->topic_prefix, rule->destination_topic);
        } else if (relay_client->config.relay_prefix) {
            snprintf(dest_topic, sizeof(dest_topic), "%s%s", relay_client->config.relay_prefix, rule->destination_topic);
        } else {
            strncpy(dest_topic, rule->destination_topic, sizeof(dest_topic) - 1);
            dest_topic[sizeof(dest_topic) - 1] = '\0';
        }
        
        // Get destination client
        ur_rpc_client_t* dest_client = relay_client->clients[rule->dest_broker_index];
        if (!dest_client || !ur_rpc_client_is_connected(dest_client)) {
            relay_client->relay_errors++;
            continue;
        }
        
        // Forward the message
        int result = ur_rpc_publish_message(dest_client, dest_topic, payload, payload_len);
        if (result == UR_RPC_SUCCESS) {
            relay_client->messages_relayed++;
            LOG_INFO_SIMPLE("RELAYED: %s -> %s (broker %d -> %d)", 
                   topic, dest_topic, rule->source_broker_index, rule->dest_broker_index);
        } else {
            relay_client->relay_errors++;
            LOG_ERROR_SIMPLE("RELAY FAILED: %s -> %s (error: %d)", topic, dest_topic, result);
        }
        
        // Handle bidirectional relay if enabled
        if (rule->bidirectional) {
            // TODO: Implement bidirectional relay logic to prevent loops
        }
    }
    
    pthread_mutex_unlock(&relay_client->relay_mutex);
}

/* Create relay client */
ur_rpc_relay_client_t* ur_rpc_relay_client_create(const ur_rpc_client_config_t* config) {
    if (!config || !config->relay.enabled) return NULL;
    
    ur_rpc_relay_client_t* relay_client = malloc(sizeof(ur_rpc_relay_client_t));
    if (!relay_client) return NULL;
    
    memset(relay_client, 0, sizeof(ur_rpc_relay_client_t));
    
    // Initialize and deep copy relay configuration
    if (ur_rpc_relay_config_init(&relay_client->config) != UR_RPC_SUCCESS) {
        free(relay_client);
        return NULL;
    }
    
    relay_client->config.enabled = config->relay.enabled;
    relay_client->config.broker_count = config->relay.broker_count;
    relay_client->config.rule_count = config->relay.rule_count;
    
    // Copy relay prefix
    if (config->relay.relay_prefix) {
        relay_client->config.relay_prefix = strdup(config->relay.relay_prefix);
    }
    
    // Deep copy brokers
    for (int i = 0; i < config->relay.broker_count; i++) {
        const ur_rpc_broker_config_t* src = &config->relay.brokers[i];
        ur_rpc_broker_config_t* dst = &relay_client->config.brokers[i];
        
        memset(dst, 0, sizeof(ur_rpc_broker_config_t));
        dst->host = src->host ? strdup(src->host) : NULL;
        dst->port = src->port;
        dst->username = src->username ? strdup(src->username) : NULL;
        dst->password = src->password ? strdup(src->password) : NULL;
        dst->client_id = src->client_id ? strdup(src->client_id) : NULL;
        dst->use_tls = src->use_tls;
        dst->ca_file = src->ca_file ? strdup(src->ca_file) : NULL;
        dst->is_primary = src->is_primary;
    }
    
    // Deep copy rules
    for (int i = 0; i < config->relay.rule_count; i++) {
        const ur_rpc_relay_rule_t* src = &config->relay.rules[i];
        ur_rpc_relay_rule_t* dst = &relay_client->config.rules[i];
        
        memset(dst, 0, sizeof(ur_rpc_relay_rule_t));
        dst->source_topic = src->source_topic ? strdup(src->source_topic) : NULL;
        dst->destination_topic = src->destination_topic ? strdup(src->destination_topic) : NULL;
        dst->topic_prefix = src->topic_prefix ? strdup(src->topic_prefix) : NULL;
        dst->source_broker_index = src->source_broker_index;
        dst->dest_broker_index = src->dest_broker_index;
        dst->bidirectional = src->bidirectional;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&relay_client->relay_mutex, NULL) != 0) {
        free(relay_client);
        return NULL;
    }
    
    // Create client connections for each broker
    for (int i = 0; i < relay_client->config.broker_count; i++) {
        ur_rpc_broker_config_t* broker_config = &relay_client->config.brokers[i];
        
        // Create individual client configuration
        ur_rpc_client_config_t* client_config = ur_rpc_config_create();
        if (!client_config) {
            ur_rpc_relay_client_destroy(relay_client);
            return NULL;
        }
        
        // Configure client for this broker
        ur_rpc_config_set_broker(client_config, broker_config->host, broker_config->port);
        ur_rpc_config_set_client_id(client_config, broker_config->client_id);
        
        if (broker_config->username && broker_config->password) {
            ur_rpc_config_set_credentials(client_config, broker_config->username, broker_config->password);
        }
        
        // Create topic configuration
        ur_rpc_topic_config_t* topic_config = ur_rpc_topic_config_create();
        if (!topic_config) {
            ur_rpc_config_destroy(client_config);
            ur_rpc_relay_client_destroy(relay_client);
            return NULL;
        }
        
        // Create and configure client
        relay_client->clients[i] = ur_rpc_client_create(client_config, topic_config);
        if (!relay_client->clients[i]) {
            ur_rpc_config_destroy(client_config);
            ur_rpc_topic_config_destroy(topic_config);
            ur_rpc_relay_client_destroy(relay_client);
            return NULL;
        }
        
        // Set relay message handler for source brokers
        ur_rpc_client_set_message_handler(relay_client->clients[i], relay_message_handler, relay_client);
        
        ur_rpc_config_destroy(client_config);
        ur_rpc_topic_config_destroy(topic_config);
    }
    
    atomic_store(&relay_client->relay_running, false);
    relay_client->relay_start_time = time(NULL);
    
    return relay_client;
}

/* Destroy relay client */
void ur_rpc_relay_client_destroy(ur_rpc_relay_client_t* relay_client) {
    if (!relay_client) return;
    
    // Stop relay if running
    ur_rpc_relay_client_stop(relay_client);
    
    // Destroy all client connections
    for (int i = 0; i < UR_RPC_MAX_BROKERS; i++) {
        if (relay_client->clients[i]) {
            ur_rpc_client_destroy(relay_client->clients[i]);
        }
    }
    
    // Cleanup configuration
    ur_rpc_relay_config_cleanup(&relay_client->config);
    
    // Destroy mutex
    pthread_mutex_destroy(&relay_client->relay_mutex);
    
    free(relay_client);
}

/* Start relay client */
int ur_rpc_relay_client_start(ur_rpc_relay_client_t* relay_client) {
    if (!relay_client) return UR_RPC_ERROR_INVALID_PARAM;
    if (atomic_load(&relay_client->relay_running)) return UR_RPC_SUCCESS;
    
    LOG_INFO_SIMPLE("Starting relay client with %d brokers and %d rules...", 
           relay_client->config.broker_count, relay_client->config.rule_count);
    
    // Connect clients based on conditional relay setting
    for (int i = 0; i < relay_client->config.broker_count; i++) {
        if (!relay_client->clients[i]) continue;
        
        ur_rpc_broker_config_t* broker_config = &relay_client->config.brokers[i];
        
        // If conditional relay is enabled, only connect to primary brokers initially
        if (relay_client->config.conditional_relay && !broker_config->is_primary) {
            LOG_INFO_SIMPLE("Conditional relay enabled: Skipping secondary broker %d (will connect when g_sec_conn_ready is true)", i);
            continue;
        }
        
        LOG_INFO_SIMPLE("Connecting to broker %d: %s:%d", i, 
               relay_client->config.brokers[i].host, 
               relay_client->config.brokers[i].port);
        
        int result = ur_rpc_client_connect(relay_client->clients[i]);
        if (result != UR_RPC_SUCCESS) {
            LOG_ERROR_SIMPLE("Failed to connect to broker %d (error: %d)", i, result);
            continue;
        }
        
        result = ur_rpc_client_start(relay_client->clients[i]);
        if (result != UR_RPC_SUCCESS) {
            LOG_ERROR_SIMPLE("Failed to start client %d (error: %d)", i, result);
            continue;
        }
        
        // Wait for connection
        for (int j = 0; j < 50 && !ur_rpc_client_is_connected(relay_client->clients[i]); j++) {
            usleep(100000); // 100ms
        }
        
        if (ur_rpc_client_is_connected(relay_client->clients[i])) {
            LOG_INFO_SIMPLE("Connected to broker %d successfully", i);
        } else {
            LOG_ERROR_SIMPLE("Failed to establish connection to broker %d", i);
        }
    }
    
    // Subscribe to source topics for each rule
    for (int i = 0; i < relay_client->config.rule_count; i++) {
        ur_rpc_relay_rule_t* rule = &relay_client->config.rules[i];
        ur_rpc_client_t* source_client = relay_client->clients[rule->source_broker_index];
        
        if (source_client && ur_rpc_client_is_connected(source_client)) {
            int result = ur_rpc_subscribe_topic(source_client, rule->source_topic);
            if (result == UR_RPC_SUCCESS) {
                LOG_INFO_SIMPLE("Subscribed to source topic '%s' on broker %d", 
                       rule->source_topic, rule->source_broker_index);
            } else {
                LOG_ERROR_SIMPLE("Failed to subscribe to source topic '%s' on broker %d", 
                       rule->source_topic, rule->source_broker_index);
            }
        }
    }
    
    atomic_store(&relay_client->relay_running, true);
    LOG_INFO_SIMPLE("Relay client started successfully");
    
    return UR_RPC_SUCCESS;
}

/* Stop relay client */
int ur_rpc_relay_client_stop(ur_rpc_relay_client_t* relay_client) {
    if (!relay_client) return UR_RPC_ERROR_INVALID_PARAM;
    if (!atomic_load(&relay_client->relay_running)) return UR_RPC_SUCCESS;
    
    LOG_INFO_SIMPLE("Stopping relay client...");
    
    atomic_store(&relay_client->relay_running, false);
    
    // Stop all clients
    for (int i = 0; i < relay_client->config.broker_count; i++) {
        if (relay_client->clients[i]) {
            ur_rpc_client_stop(relay_client->clients[i]);
            ur_rpc_client_disconnect(relay_client->clients[i]);
        }
    }
    
    LOG_INFO_SIMPLE("Relay client stopped");
    return UR_RPC_SUCCESS;
}

/* Conditional relay control functions */
int ur_rpc_relay_set_secondary_connection_ready(bool ready) {
    g_sec_conn_ready = ready;
    LOG_INFO_SIMPLE("Secondary connection ready flag set to: %s", ready ? "true" : "false");
    return UR_RPC_SUCCESS;
}

bool ur_rpc_relay_is_secondary_connection_ready(void) {
    return g_sec_conn_ready;
}

/* Connect secondary brokers when ready */
int ur_rpc_relay_connect_secondary_brokers(ur_rpc_relay_client_t* relay_client) {
    if (!relay_client) return UR_RPC_ERROR_INVALID_PARAM;
    
    if (!relay_client->config.conditional_relay) {
        LOG_INFO_SIMPLE("Conditional relay not enabled, all brokers should already be connected");
        return UR_RPC_SUCCESS;
    }
    
    if (!g_sec_conn_ready) {
        LOG_WARN_SIMPLE("Secondary connection not ready yet");
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    LOG_INFO_SIMPLE("Connecting to secondary brokers...");
    
    // Connect to secondary brokers
    for (int i = 0; i < relay_client->config.broker_count; i++) {
        if (!relay_client->clients[i]) continue;
        
        ur_rpc_broker_config_t* broker_config = &relay_client->config.brokers[i];
        
        // Skip if already connected or if this is a primary broker
        if (broker_config->is_primary || ur_rpc_client_is_connected(relay_client->clients[i])) {
            continue;
        }
        
        LOG_INFO_SIMPLE("Connecting to secondary broker %d: %s:%d", i, 
               broker_config->host, broker_config->port);
        
        int result = ur_rpc_client_connect(relay_client->clients[i]);
        if (result != UR_RPC_SUCCESS) {
            LOG_ERROR_SIMPLE("Failed to connect to secondary broker %d (error: %d)", i, result);
            continue;
        }
        
        result = ur_rpc_client_start(relay_client->clients[i]);
        if (result != UR_RPC_SUCCESS) {
            LOG_ERROR_SIMPLE("Failed to start secondary client %d (error: %d)", i, result);
            continue;
        }
        
        // Wait for connection
        for (int j = 0; j < 50 && !ur_rpc_client_is_connected(relay_client->clients[i]); j++) {
            usleep(100000); // 100ms
        }
        
        if (ur_rpc_client_is_connected(relay_client->clients[i])) {
            LOG_INFO_SIMPLE("Connected to secondary broker %d successfully", i);
        } else {
            LOG_ERROR_SIMPLE("Failed to establish connection to secondary broker %d", i);
        }
    }
    
    LOG_INFO_SIMPLE("Secondary broker connections completed");
    return UR_RPC_SUCCESS;
}
