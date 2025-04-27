#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mosquitto.h>
#include <cJSON.h>
#include <stdatomic.h>

typedef enum{
    os_update,
    generic_config_update,
    target_specific_config_update
}update_types;

typedef struct {
    char* process_id;
    update_types update_type;
    char* update_value;
}config_update;


// Structure to hold base configuration
typedef struct {
    char* process_id;          // Process ID (string)
    char* broker_url;          // MQTT broker URL
    int broker_port;           // MQTT broker port
    char* heartbeat_topic;     // Heartbeat topic
    char* response_topic;      // Response topic
    char* query_topic;         // Query topic for process status
    char* ip_query_topic;      // Query topic for IP availability
    char* module_update_topic; // Topic for internal mqtt client new attributes  
    int heartbeat_interval;    // Heartbeat interval in milliseconds
    int heartbeat_timeout;     // Heartbeat timeout in milliseconds
} Config;

// Structure to hold additional topics
typedef struct {
    int topics_num;            // Number of topics 
    char** topics;             // List of the topics strings 
} AddedTopics;

// Structure to hold custom loader configuration
typedef struct {
    AddedTopics json_added_pubs; // Additional topics to publish to
    AddedTopics json_added_subs; // Additional topics to listen to 
} CustomLoader;

typedef struct{
    char* base_config_path;
    char* custom_config_path;
}config_files;

typedef struct {
    pthread_t thread_id;
    atomic_bool running;
    atomic_bool healthy;
    time_t last_activity;
} ThreadMonitor;

// MQTT thread context structure
typedef struct {
    struct mosquitto* mosq;
    Config config_base;
    CustomLoader config_additional;
    config_files config_paths;
    pthread_mutex_t mutex;
    ThreadMonitor mqtt_monitor;
    ThreadMonitor health_monitor;
} MqttThreadContext;

extern MqttThreadContext* context; 

// Function prototypes
Config parse_base_config(const char* filename);
CustomLoader parse_custom_topics(const char* filename);
void free_base_config(Config* config);
void free_custom_topics(CustomLoader* custom);
void* mqtt_thread_func(void* arg);
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);
bool query_process_status(MqttThreadContext* context, const char* process_id) ;
bool query_ip_availability(MqttThreadContext* context, const char* ip);
void publish_to_custom_topic(const char* topic, const char* message);
// template for multi clients void publish_to_custom_topic(MqttThreadContext* context, const char* topic, const char* message);


#ifdef __GNUC__
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

// Declare the weak function
void* health_monitor_func(void* arg);
WEAK void mqtt_thread_runner(const char* base_config_file, const char* custom_topics_file);


typedef struct {
    char* dev_path;
    bool enable;
}DeviceState;

void serialize_device_state(const DeviceState* state,char* json_str,char* distop);