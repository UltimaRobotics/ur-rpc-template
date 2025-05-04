#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <mosquitto.h>
#include <cJSON.h>
#include <ur-rpc-template.h>

MqttThreadContext* context = NULL;

DeviceState* create_device_state(const char* dev_path, bool enable) {
    if (!dev_path) return NULL;

    DeviceState* state = malloc(sizeof(DeviceState));
    if (!state) return NULL;

    // Manual string copy instead of strdup
    state->dev_path = malloc(strlen(dev_path) + 1);
    if (!state->dev_path) {
        free(state);
        return NULL;
    }
    strcpy(state->dev_path, dev_path);

    state->enable = enable;
    return state;
}

void serialize_device_state(const DeviceState* state,char* json_str,char* distop) {
    if (!state) return NULL;
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "dev_path", state->dev_path);
    cJSON_AddBoolToObject(root, "enable", state->enable);
    json_str = cJSON_Print(root);
    publish_to_custom_topic(distop,json_str);
    cJSON_Delete(root);
}

DeviceState* deserialize_device_state(const char* data) {
    if (!data) return NULL;

    // Find the separator
    const char* separator = strchr(data, '|');
    if (!separator || separator == data || *(separator + 1) == '\0') {
        return NULL;
    }

    // Calculate path length
    size_t path_len = separator - data;
    
    // Allocate new state
    DeviceState* state = malloc(sizeof(DeviceState));
    if (!state) return NULL;

    // Allocate and copy path
    state->dev_path = malloc(path_len + 1);
    if (!state->dev_path) {
        free(state);
        return NULL;
    }
    memcpy(state->dev_path, data, path_len);
    state->dev_path[path_len] = '\0';

    // Parse enable flag
    state->enable = (*(separator + 1) == '1');

    return state;
}

void free_device_state(DeviceState* state) {
    if (state) {
        free(state->dev_path);
        free(state);
    }
}

// Parse base configuration from JSON file
Config_mqtt parse_base_config(const char* filename) {
    Config_mqtt config = {0};
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file: %s\n", filename);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* json_data = malloc(file_size + 1);
    fread(json_data, 1, file_size, file);
    fclose(file);
    json_data[file_size] = '\0';

    cJSON* json = cJSON_Parse(json_data);
    if (!json) {
        fprintf(stderr, "Failed to parse JSON\n");
        free(json_data);
        exit(1);
    }

    // Extract configuration values with null checks
    cJSON* item;
    if ((item = cJSON_GetObjectItem(json, "process_id")) && cJSON_IsString(item))
        config.process_id = strdup(item->valuestring);
    if ((item = cJSON_GetObjectItem(json, "broker_url")) && cJSON_IsString(item))
        config.broker_url = strdup(item->valuestring);
    if ((item = cJSON_GetObjectItem(json, "broker_port")) && cJSON_IsNumber(item))
        config.broker_port = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "heartbeat_topic")) && cJSON_IsString(item))
        config.heartbeat_topic = strdup(item->valuestring);
    if ((item = cJSON_GetObjectItem(json, "response_topic")) && cJSON_IsString(item))
        config.response_topic = strdup(item->valuestring);
    if ((item = cJSON_GetObjectItem(json, "query_topic")) && cJSON_IsString(item))
        config.query_topic = strdup(item->valuestring);
    if ((item = cJSON_GetObjectItem(json, "ip_query_topic")) && cJSON_IsString(item))
        config.ip_query_topic = strdup(item->valuestring);
    if ((item = cJSON_GetObjectItem(json, "module_update_topic")) && cJSON_IsString(item))
        config.module_update_topic = strdup(item->valuestring);
    if ((item = cJSON_GetObjectItem(json, "heartbeat_interval")) && cJSON_IsNumber(item))
        config.heartbeat_interval = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "heartbeat_timeout")) && cJSON_IsNumber(item))
        config.heartbeat_timeout = item->valueint;

    cJSON_Delete(json);
    free(json_data);
    return config;
}

// Parse custom topics from JSON file
CustomLoader parse_custom_topics(const char* filename) {
    CustomLoader custom = {0};
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open custom topics file: %s\n", filename);
        return custom;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* json_data = malloc(file_size + 1);
    fread(json_data, 1, file_size, file);
    fclose(file);
    json_data[file_size] = '\0';

    cJSON* json = cJSON_Parse(json_data);
    if (!json) {
        fprintf(stderr, "Failed to parse custom topics JSON\n");
        free(json_data);
        return custom;
    }

    // Parse publish topics
    cJSON* pubs = cJSON_GetObjectItem(json, "json_added_pubs");
    if (pubs && cJSON_IsObject(pubs)) {
        cJSON* topics = cJSON_GetObjectItem(pubs, "topics");
        if (topics && cJSON_IsArray(topics)) {
            custom.json_added_pubs.topics_num = cJSON_GetArraySize(topics);
            custom.json_added_pubs.topics = malloc(custom.json_added_pubs.topics_num * sizeof(char*));
            
            for (int i = 0; i < custom.json_added_pubs.topics_num; i++) {
                cJSON* item = cJSON_GetArrayItem(topics, i);
                if (item && cJSON_IsString(item)) {
                    custom.json_added_pubs.topics[i] = strdup(item->valuestring);
                }
            }
        }
    }

    // Parse subscribe topics
    cJSON* subs = cJSON_GetObjectItem(json, "json_added_subs");
    if (subs && cJSON_IsObject(subs)) {
        cJSON* topics = cJSON_GetObjectItem(subs, "topics");
        if (topics && cJSON_IsArray(topics)) {
            custom.json_added_subs.topics_num = cJSON_GetArraySize(topics);
            custom.json_added_subs.topics = malloc(custom.json_added_subs.topics_num * sizeof(char*));
            
            for (int i = 0; i < custom.json_added_subs.topics_num; i++) {
                cJSON* item = cJSON_GetArrayItem(topics, i);
                if (item && cJSON_IsString(item)) {
                    custom.json_added_subs.topics[i] = strdup(item->valuestring);
                }
            }
        }
    }

    cJSON_Delete(json);
    free(json_data);
    return custom;
}

// Free base configuration memory
void free_base_config(Config_mqtt* config) {
    if (config->process_id) free(config->process_id);
    if (config->broker_url) free(config->broker_url);
    if (config->heartbeat_topic) free(config->heartbeat_topic);
    if (config->response_topic) free(config->response_topic);
    if (config->query_topic) free(config->query_topic);
    if (config->ip_query_topic) free(config->ip_query_topic);
    if (config->module_update_topic) free(config->module_update_topic);
}

// Free custom topics memory
void free_custom_topics(CustomLoader* custom) {
    for (int i = 0; i < custom->json_added_pubs.topics_num; i++) {
        free(custom->json_added_pubs.topics[i]);
    }
    free(custom->json_added_pubs.topics);

    for (int i = 0; i < custom->json_added_subs.topics_num; i++) {
        free(custom->json_added_subs.topics[i]);
    }
    free(custom->json_added_subs.topics);
}



// Function to query the status of another process
bool query_process_status(MqttThreadContext* context, const char* process_id) {
    // Lock mutex for thread-safe access to context
    pthread_mutex_lock(&context->mutex);
    
    // Create query JSON
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "process_id", process_id);
    cJSON_AddStringToObject(json, "requester_id", context->config_base.process_id);
    char* query = cJSON_PrintUnformatted(json);

    // Publish query
    int rc = mosquitto_publish(context->mosq, NULL, 
                              context->config_base.query_topic, 
                              strlen(query), query, 0, false);
    
    bool alive = false;
    if (rc == MOSQ_ERR_SUCCESS) {
        // Wait for response with timeout
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 5) {  // Wait for 5 seconds
            mosquitto_loop(context->mosq, 100, 1);
            
        }
    } else {
        fprintf(stderr, "Failed to publish process status query: %s\n", mosquitto_strerror(rc));
    }

    // Cleanup
    cJSON_Delete(json);
    free(query);
    
    pthread_mutex_unlock(&context->mutex);
    return alive;
}

// Function to query the availability of an IP
bool query_ip_availability(MqttThreadContext* context, const char* ip) {
    // Lock mutex for thread-safe access to context
    pthread_mutex_lock(&context->mutex);
    
    // Create query JSON
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "ip", ip);
    cJSON_AddStringToObject(json, "requester_id", context->config_base.process_id);
    char* query = cJSON_PrintUnformatted(json);

    // Publish query
    int rc = mosquitto_publish(context->mosq, NULL, 
                              context->config_base.ip_query_topic, 
                              strlen(query), query, 0, false);
    
    bool available = false;
    if (rc == MOSQ_ERR_SUCCESS) {
        // Wait for response with timeout
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 5) {  // Wait for 5 seconds
            mosquitto_loop(context->mosq, 100, 1);
            
            // In a real implementation, you would:
            // 1. Set up a response handler callback
            // 2. Check for the specific response
            // 3. Set 'available' based on the response
            // This is a simplified placeholder
        }
    } else {
        fprintf(stderr, "Failed to publish IP availability query: %s\n", mosquitto_strerror(rc));
    }

    // Cleanup
    cJSON_Delete(json);
    free(query);
    
    pthread_mutex_unlock(&context->mutex);
    return available;
}

// Helper function to publish to additional topics
void publish_to_custom_topic(const char* topic, const char* message) {
    bool valid_topic = false;
    for (int i = 0; i < context->config_additional.json_added_pubs.topics_num; i++) {
        if (strcmp(topic, context->config_additional.json_added_pubs.topics[i]) == 0) {
            valid_topic = true;
            break;
        }
    }
    if (valid_topic) {
        int rc = mosquitto_publish(context->mosq, NULL, topic, strlen(message), message, 0, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Failed to publish to custom topic %s: %s\n", topic, mosquitto_strerror(rc));
        }
    } else {
        fprintf(stderr, "Attempted to publish to unauthorized topic: %s\n", topic);
    }
}

// MQTT thread function (detached)
void* mqtt_thread_func(void* arg) {

    MqttThreadContext* context = (MqttThreadContext*)arg;
    int rc;
    mosquitto_lib_init();
    context->mosq = mosquitto_new(NULL, true, context);
    if (!context->mosq) {
        fprintf(stderr, "[MQTT] Failed to create Mosquitto instance\n");
        return NULL;
    }

    mosquitto_message_callback_set(context->mosq, on_message);
    mosquitto_connect_callback_set(context->mosq, on_connect);
    rc = mosquitto_connect(context->mosq, context->config_base.broker_url, context->config_base.broker_port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "[MQTT] Failed to connect to broker %s:%d: %s\n",context->config_base.broker_url,context->config_base.broker_port, mosquitto_strerror(rc));
        printf("files %s %s \n",context->config_paths.base_config_path,context->config_paths.custom_config_path);
        mosquitto_destroy(context->mosq);
        mosquitto_lib_cleanup();
        return NULL;
    }

    int ret = mosquitto_loop_start(context->mosq);
    if (ret != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Unable to start loop: %s\n", mosquitto_strerror(ret));
        mosquitto_destroy(context->mosq);
        mosquitto_lib_cleanup();
        return 1;
    }
    while(1){
        usleep(100000);  // Sleep for 100 ms
    }
    mosquitto_disconnect(context->mosq);
    mosquitto_destroy(context->mosq);
    mosquitto_lib_cleanup();
    atomic_store(&context->mqtt_monitor.running, false);
    return NULL;
}


void copy_mqtt_thread_context(MqttThreadContext* dest, const MqttThreadContext* src) {
    if (!dest || !src) return;

    // First initialize the destination as empty
    memset(dest, 0, sizeof(MqttThreadContext));
    pthread_mutex_init(&dest->mutex, NULL);

    // Copy simple fields
    dest->config_base = src->config_base;  // Assuming Config is simple struct
    dest->config_additional = src->config_additional;  // Assuming CustomLoader is simple
    
    // Copy config paths with proper string duplication
    if (src->config_paths.base_config_path) {
        dest->config_paths.base_config_path = strdup(src->config_paths.base_config_path);
    }
    if (src->config_paths.custom_config_path) {
        dest->config_paths.custom_config_path = strdup(src->config_paths.custom_config_path);
    }

    // Initialize monitor structures (don't copy running state)
    dest->mqtt_monitor.last_activity = src->mqtt_monitor.last_activity;
    atomic_init(&dest->mqtt_monitor.running, false);
    atomic_init(&dest->mqtt_monitor.healthy, false);
    atomic_init(&dest->health_monitor.running, false);

    // DON'T copy these - they need to be created fresh:
    // - mosq (Mosquitto instance)
    // - thread_id (pthread_t)
    // - mutex (already initialized above)
}
