#ifndef UR_RPC_TEMPLATE_HPP
#define UR_RPC_TEMPLATE_HPP

#include "ur-rpc-template.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <cJSON.h>

namespace UrRpc {

// Exception classes for better error handling
class Exception : public std::runtime_error {
public:
    explicit Exception(const std::string& message) : std::runtime_error(message) {}
    explicit Exception(ur_rpc_error_t error) : std::runtime_error(ur_rpc_error_string(error)) {}
};

class ConfigException : public Exception {
public:
    explicit ConfigException(const std::string& message) : Exception("Configuration error: " + message) {}
};

class ConnectionException : public Exception {
public:
    explicit ConnectionException(const std::string& message) : Exception("Connection error: " + message) {}
};

class TimeoutException : public Exception {
public:
    explicit TimeoutException(const std::string& message) : Exception("Timeout error: " + message) {}
};

// C++ wrapper classes

// Smart pointer wrapper for cJSON with automatic cleanup
class JsonValue {
private:
    cJSON* json_;
    bool owner_;

public:
    JsonValue() : json_(cJSON_CreateObject()), owner_(true) {}
    explicit JsonValue(cJSON* json, bool take_ownership = false) : json_(json), owner_(take_ownership) {}
    explicit JsonValue(const std::string& json_string) : owner_(true) {
        json_ = cJSON_Parse(json_string.c_str());
        if (!json_) {
            throw Exception("Invalid JSON string");
        }
    }
    
    ~JsonValue() {
        if (json_ && owner_) {
            cJSON_Delete(json_);
        }
    }
    
    // Move constructor and assignment
    JsonValue(JsonValue&& other) noexcept : json_(other.json_), owner_(other.owner_) {
        other.json_ = nullptr;
        other.owner_ = false;
    }
    
    JsonValue& operator=(JsonValue&& other) noexcept {
        if (this != &other) {
            if (json_ && owner_) {
                cJSON_Delete(json_);
            }
            json_ = other.json_;
            owner_ = other.owner_;
            other.json_ = nullptr;
            other.owner_ = false;
        }
        return *this;
    }
    
    // Delete copy constructor and assignment
    JsonValue(const JsonValue&) = delete;
    JsonValue& operator=(const JsonValue&) = delete;
    
    cJSON* get() const { return json_; }
    
    std::string toString() const {
        if (!json_) return "{}";
        char* str = cJSON_Print(json_);
        if (!str) return "{}";
        std::string result(str);
        free(str);
        return result;
    }
    
    void addString(const std::string& key, const std::string& value) {
        if (json_) {
            cJSON_AddStringToObject(json_, key.c_str(), value.c_str());
        }
    }
    
    void addNumber(const std::string& key, double value) {
        if (json_) {
            cJSON_AddNumberToObject(json_, key.c_str(), value);
        }
    }
    
    void addBool(const std::string& key, bool value) {
        if (json_) {
            cJSON_AddBoolToObject(json_, key.c_str(), value);
        }
    }
    
    std::optional<std::string> getString(const std::string& key) const {
        if (!json_) return std::nullopt;
        cJSON* item = cJSON_GetObjectItem(json_, key.c_str());
        if (!item || !cJSON_IsString(item)) return std::nullopt;
        return std::string(item->valuestring);
    }
    
    std::optional<double> getNumber(const std::string& key) const {
        if (!json_) return std::nullopt;
        cJSON* item = cJSON_GetObjectItem(json_, key.c_str());
        if (!item || !cJSON_IsNumber(item)) return std::nullopt;
        return item->valuedouble;
    }
    
    std::optional<bool> getBool(const std::string& key) const {
        if (!json_) return std::nullopt;
        cJSON* item = cJSON_GetObjectItem(json_, key.c_str());
        if (!item || !cJSON_IsBool(item)) return std::nullopt;
        return cJSON_IsTrue(item);
    }
};

// Authority enum wrapper
enum class Authority {
    Admin = UR_RPC_AUTHORITY_ADMIN,
    User = UR_RPC_AUTHORITY_USER,
    Guest = UR_RPC_AUTHORITY_GUEST,
    System = UR_RPC_AUTHORITY_SYSTEM
};

// Connection status enum wrapper
enum class ConnectionStatus {
    Disconnected = UR_RPC_CONN_DISCONNECTED,
    Connecting = UR_RPC_CONN_CONNECTING,
    Connected = UR_RPC_CONN_CONNECTED,
    Reconnecting = UR_RPC_CONN_RECONNECTING,
    Error = UR_RPC_CONN_ERROR
};

// Method type enum wrapper
enum class MethodType {
    RequestResponse = UR_RPC_METHOD_REQUEST_RESPONSE,
    RequestOnly = UR_RPC_METHOD_REQUEST_ONLY,
    Notification = UR_RPC_METHOD_NOTIFICATION
};

// Forward declarations
class Client;
class RelayClient;

// Callback function types
using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;
using ResponseHandler = std::function<void(bool success, const JsonValue& result, const std::string& error_message, int error_code)>;
using ConnectionCallback = std::function<void(ConnectionStatus status)>;

// Configuration class
class ClientConfig {
private:
    std::unique_ptr<ur_rpc_client_config_t, decltype(&ur_rpc_config_destroy)> config_;

public:
    ClientConfig() : config_(ur_rpc_config_create(), ur_rpc_config_destroy) {
        if (!config_) {
            throw ConfigException("Failed to create client configuration");
        }
    }
    
    ClientConfig& setBroker(const std::string& host, int port) {
        int result = ur_rpc_config_set_broker(config_.get(), host.c_str(), port);
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set broker configuration");
        }
        return *this;
    }
    
    ClientConfig& setCredentials(const std::string& username, const std::string& password) {
        int result = ur_rpc_config_set_credentials(config_.get(), username.c_str(), password.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set credentials");
        }
        return *this;
    }
    
    ClientConfig& setClientId(const std::string& client_id) {
        int result = ur_rpc_config_set_client_id(config_.get(), client_id.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set client ID");
        }
        return *this;
    }
    
    ClientConfig& setTLS(const std::string& ca_file, const std::string& cert_file = "", const std::string& key_file = "") {
        const char* ca_ptr = ca_file.empty() ? nullptr : ca_file.c_str();
        const char* cert_ptr = cert_file.empty() ? nullptr : cert_file.c_str();
        const char* key_ptr = key_file.empty() ? nullptr : key_file.c_str();
        
        int result = ur_rpc_config_set_tls(config_.get(), ca_ptr, cert_ptr, key_ptr);
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set TLS configuration");
        }
        return *this;
    }
    
    ClientConfig& setTLSVersion(const std::string& version) {
        int result = ur_rpc_config_set_tls_version(config_.get(), version.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set TLS version");
        }
        return *this;
    }
    
    ClientConfig& setTLSInsecure(bool insecure) {
        int result = ur_rpc_config_set_tls_insecure(config_.get(), insecure);
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set TLS insecure mode");
        }
        return *this;
    }
    
    ClientConfig& setTimeouts(int connect_timeout, int message_timeout) {
        int result = ur_rpc_config_set_timeouts(config_.get(), connect_timeout, message_timeout);
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set timeout configuration");
        }
        return *this;
    }
    
    ClientConfig& setReconnect(bool auto_reconnect, int min_delay, int max_delay) {
        int result = ur_rpc_config_set_reconnect(config_.get(), auto_reconnect, min_delay, max_delay);
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set reconnect configuration");
        }
        return *this;
    }
    
    ClientConfig& setHeartbeat(const std::string& topic, int interval_seconds, const std::string& payload) {
        int result = ur_rpc_config_set_heartbeat(config_.get(), topic.c_str(), interval_seconds, payload.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set heartbeat configuration");
        }
        return *this;
    }
    
    ClientConfig& loadFromFile(const std::string& filename) {
        int result = ur_rpc_config_load_from_file(config_.get(), filename.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to load configuration from file: " + filename);
        }
        return *this;
    }
    
    const ur_rpc_client_config_t* get() const { return config_.get(); }
};

// Topic configuration class
class TopicConfig {
private:
    std::unique_ptr<ur_rpc_topic_config_t, decltype(&ur_rpc_topic_config_destroy)> config_;

public:
    TopicConfig() : config_(ur_rpc_topic_config_create(), ur_rpc_topic_config_destroy) {
        if (!config_) {
            throw ConfigException("Failed to create topic configuration");
        }
    }
    
    TopicConfig& setPrefixes(const std::string& base_prefix, const std::string& service_prefix) {
        int result = ur_rpc_topic_config_set_prefixes(config_.get(), base_prefix.c_str(), service_prefix.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set topic prefixes");
        }
        return *this;
    }
    
    TopicConfig& setSuffixes(const std::string& request_suffix, const std::string& response_suffix, const std::string& notification_suffix) {
        int result = ur_rpc_topic_config_set_suffixes(config_.get(), request_suffix.c_str(), response_suffix.c_str(), notification_suffix.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw ConfigException("Failed to set topic suffixes");
        }
        return *this;
    }
    
    const ur_rpc_topic_config_t* get() const { return config_.get(); }
};

// Request class
class Request {
private:
    std::unique_ptr<ur_rpc_request_t, decltype(&ur_rpc_request_destroy)> request_;

public:
    Request() : request_(ur_rpc_request_create(), ur_rpc_request_destroy) {
        if (!request_) {
            throw Exception("Failed to create request");
        }
    }
    
    explicit Request(ur_rpc_request_t* request, bool take_ownership = true) 
        : request_(request, take_ownership ? ur_rpc_request_destroy : nullptr) {
        if (!request_) {
            throw Exception("Invalid request pointer");
        }
    }
    
    Request& setMethod(const std::string& method, const std::string& service) {
        int result = ur_rpc_request_set_method(request_.get(), method.c_str(), service.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to set request method");
        }
        return *this;
    }
    
    Request& setAuthority(Authority authority) {
        int result = ur_rpc_request_set_authority(request_.get(), static_cast<ur_rpc_authority_t>(authority));
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to set request authority");
        }
        return *this;
    }
    
    Request& setParams(const JsonValue& params) {
        int result = ur_rpc_request_set_params(request_.get(), params.get());
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to set request parameters");
        }
        return *this;
    }
    
    Request& setTimeout(int timeout_ms) {
        int result = ur_rpc_request_set_timeout(request_.get(), timeout_ms);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to set request timeout");
        }
        return *this;
    }
    
    const ur_rpc_request_t* get() const { return request_.get(); }
    const ur_rpc_request_t* getNativeHandle() const { return request_.get(); }
};

// Response class
class Response {
private:
    std::unique_ptr<ur_rpc_response_t, decltype(&ur_rpc_response_destroy)> response_;

public:
    explicit Response(ur_rpc_response_t* response, bool take_ownership = true) : response_(response, take_ownership ? ur_rpc_response_destroy : nullptr) {}
    
    bool isSuccess() const {
        return response_ && response_->success;
    }
    
    JsonValue getResult() const {
        if (response_ && response_->result) {
            return JsonValue(response_->result, false);
        }
        return JsonValue();
    }
    
    std::string getErrorMessage() const {
        if (response_ && response_->error_message) {
            return std::string(response_->error_message);
        }
        return "";
    }
    
    int getErrorCode() const {
        return response_ ? response_->error_code : 0;
    }
    
    uint64_t getTimestamp() const {
        return response_ ? response_->timestamp : 0;
    }
    
    uint64_t getProcessingTime() const {
        return response_ ? response_->processing_time_ms : 0;
    }
    
    std::string getTransactionId() const {
        if (response_ && response_->transaction_id) {
            return std::string(response_->transaction_id);
        }
        return "";
    }
    
    const ur_rpc_response_t* getNativeHandle() const {
        return response_.get();
    }
};

// Statistics structure
struct Statistics {
    uint64_t messagesSent = 0;
    uint64_t messagesReceived = 0;
    uint64_t requestsSent = 0;
    uint64_t responsesReceived = 0;
    uint64_t notificationsSent = 0;
    uint64_t errorsCount = 0;
    uint64_t connectionCount = 0;
    uint64_t uptimeSeconds = 0;
    time_t lastActivity = 0;
    
    Statistics() = default;
    
    explicit Statistics(const ur_rpc_statistics_t& stats) {
        messagesSent = stats.messages_sent;
        messagesReceived = stats.messages_received;
        requestsSent = stats.requests_sent;
        responsesReceived = stats.responses_received;
        notificationsSent = stats.notifications_sent;
        errorsCount = stats.errors_count;
        connectionCount = stats.connection_count;
        uptimeSeconds = stats.uptime_seconds;
        lastActivity = stats.last_activity;
    }
};

// Main RPC Client class
class Client {
private:
    ur_rpc_client_t* client_;
    MessageHandler message_handler_;
    std::map<std::string, ResponseHandler> pending_responses_;
    ConnectionCallback connection_callback_;
    
    static void message_callback_wrapper(const char* topic, const char* payload, size_t payload_len, void* user_data) {
        auto* client = static_cast<Client*>(user_data);
        if (client && client->message_handler_) {
            std::string topic_str(topic);
            std::string payload_str(payload, payload_len);
            client->message_handler_(topic_str, payload_str);
        }
    }
    
    static void response_callback_wrapper(const ur_rpc_response_t* response, void* user_data) {
        auto* callback_data = static_cast<std::pair<Client*, std::string>*>(user_data);
        if (callback_data && callback_data->first && response) {
            auto* client = callback_data->first;
            const std::string& transaction_id = callback_data->second;
            
            auto it = client->pending_responses_.find(transaction_id);
            if (it != client->pending_responses_.end()) {
                JsonValue result(response->result, false);
                std::string error_msg = response->error_message ? response->error_message : "";
                it->second(response->success, result, error_msg, response->error_code);
                client->pending_responses_.erase(it);
            }
            delete callback_data;
        }
    }
    
    static void connection_callback_wrapper(ur_rpc_connection_status_t status, void* user_data) {
        auto* client = static_cast<Client*>(user_data);
        if (client && client->connection_callback_) {
            client->connection_callback_(static_cast<ConnectionStatus>(status));
        }
    }

public:
    Client(const ClientConfig& config, const TopicConfig& topic_config) {
        client_ = ur_rpc_client_create(config.get(), topic_config.get());
        if (!client_) {
            throw Exception("Failed to create RPC client");
        }
    }
    
    ~Client() {
        if (client_) {
            ur_rpc_client_destroy(client_);
        }
    }
    
    // Delete copy constructor and assignment
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    // Move constructor and assignment
    Client(Client&& other) noexcept : client_(other.client_), message_handler_(std::move(other.message_handler_)),
                                      pending_responses_(std::move(other.pending_responses_)),
                                      connection_callback_(std::move(other.connection_callback_)) {
        other.client_ = nullptr;
    }
    
    Client& operator=(Client&& other) noexcept {
        if (this != &other) {
            if (client_) {
                ur_rpc_client_destroy(client_);
            }
            client_ = other.client_;
            message_handler_ = std::move(other.message_handler_);
            pending_responses_ = std::move(other.pending_responses_);
            connection_callback_ = std::move(other.connection_callback_);
            other.client_ = nullptr;
        }
        return *this;
    }
    
    void connect() {
        int result = ur_rpc_client_connect(client_);
        if (result != UR_RPC_SUCCESS) {
            throw ConnectionException("Failed to connect to broker");
        }
    }
    
    void disconnect() {
        int result = ur_rpc_client_disconnect(client_);
        if (result != UR_RPC_SUCCESS) {
            throw ConnectionException("Failed to disconnect from broker");
        }
    }
    
    void start() {
        int result = ur_rpc_client_start(client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to start client");
        }
    }
    
    void stop() {
        int result = ur_rpc_client_stop(client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to stop client");
        }
    }
    
    bool isConnected() const {
        return ur_rpc_client_is_connected(client_);
    }
    
    ConnectionStatus getStatus() const {
        return static_cast<ConnectionStatus>(ur_rpc_client_get_status(client_));
    }
    
    void setMessageHandler(MessageHandler handler) {
        message_handler_ = std::move(handler);
        ur_rpc_client_set_message_handler(client_, message_callback_wrapper, this);
    }
    
    void setConnectionCallback(ConnectionCallback callback) {
        connection_callback_ = std::move(callback);
        ur_rpc_client_set_connection_callback(client_, connection_callback_wrapper, this);
    }
    
    void callAsync(const Request& request, ResponseHandler callback) {
        std::string transaction_id = generateTransactionId();
        pending_responses_[transaction_id] = std::move(callback);
        
        auto* callback_data = new std::pair<Client*, std::string>(this, transaction_id);
        int result = ur_rpc_call_async(client_, request.get(), response_callback_wrapper, callback_data);
        if (result != UR_RPC_SUCCESS) {
            pending_responses_.erase(transaction_id);
            delete callback_data;
            throw Exception("Failed to send async request");
        }
    }
    
    Response callSync(const Request& request, int timeout_ms = 30000) {
        ur_rpc_response_t* response = nullptr;
        int result = ur_rpc_call_sync(client_, request.get(), &response, timeout_ms);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to send sync request");
        }
        return Response(response);
    }
    
    void sendNotification(const std::string& method, const std::string& service, Authority authority, const JsonValue& params) {
        int result = ur_rpc_send_notification(client_, method.c_str(), service.c_str(), static_cast<ur_rpc_authority_t>(authority), params.get());
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to send notification");
        }
    }
    
    void publishMessage(const std::string& topic, const std::string& payload) {
        int result = ur_rpc_publish_message(client_, topic.c_str(), payload.c_str(), payload.length());
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to publish message");
        }
    }
    
    void subscribeTopic(const std::string& topic) {
        int result = ur_rpc_subscribe_topic(client_, topic.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to subscribe to topic: " + topic);
        }
    }
    
    void unsubscribeTopic(const std::string& topic) {
        int result = ur_rpc_unsubscribe_topic(client_, topic.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to unsubscribe from topic: " + topic);
        }
    }
    
    std::string generateRequestTopic(const std::string& method, const std::string& service, const std::string& transaction_id) const {
        char* topic = ur_rpc_generate_request_topic(client_, method.c_str(), service.c_str(), transaction_id.c_str());
        if (!topic) {
            throw Exception("Failed to generate request topic");
        }
        std::string result(topic);
        ur_rpc_free_string(topic);
        return result;
    }
    
    std::string generateResponseTopic(const std::string& method, const std::string& service, const std::string& transaction_id) const {
        char* topic = ur_rpc_generate_response_topic(client_, method.c_str(), service.c_str(), transaction_id.c_str());
        if (!topic) {
            throw Exception("Failed to generate response topic");
        }
        std::string result(topic);
        ur_rpc_free_string(topic);
        return result;
    }
    
    std::string generateNotificationTopic(const std::string& method, const std::string& service) const {
        char* topic = ur_rpc_generate_notification_topic(client_, method.c_str(), service.c_str());
        if (!topic) {
            throw Exception("Failed to generate notification topic");
        }
        std::string result(topic);
        ur_rpc_free_string(topic);
        return result;
    }
    
    static std::string generateTransactionId() {
        char* id = ur_rpc_generate_transaction_id();
        if (!id) {
            throw Exception("Failed to generate transaction ID");
        }
        std::string result(id);
        ur_rpc_free_string(id);
        return result;
    }
    
    static bool validateTransactionId(const std::string& transaction_id) {
        return ur_rpc_validate_transaction_id(transaction_id.c_str());
    }
    
    Statistics getStatistics() const {
        ur_rpc_statistics_t stats;
        int result = ur_rpc_client_get_statistics(client_, &stats);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to get statistics");
        }
        return Statistics(stats);
    }
    
    void resetStatistics() {
        int result = ur_rpc_client_reset_statistics(client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to reset statistics");
        }
    }
    
    void startHeartbeat() {
        int result = ur_rpc_heartbeat_start(client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to start heartbeat");
        }
    }
    
    void stopHeartbeat() {
        int result = ur_rpc_heartbeat_stop(client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to stop heartbeat");
        }
    }
    
    ur_rpc_client_t* get() const { return client_; }
};

// Relay Client class
class RelayClient {
private:
    ur_rpc_relay_client_t* relay_client_;

public:
    explicit RelayClient(const ClientConfig& config) {
        relay_client_ = ur_rpc_relay_client_create(config.get());
        if (!relay_client_) {
            throw Exception("Failed to create relay client");
        }
    }
    
    ~RelayClient() {
        if (relay_client_) {
            ur_rpc_relay_client_destroy(relay_client_);
        }
    }
    
    // Delete copy constructor and assignment
    RelayClient(const RelayClient&) = delete;
    RelayClient& operator=(const RelayClient&) = delete;
    
    // Move constructor and assignment
    RelayClient(RelayClient&& other) noexcept : relay_client_(other.relay_client_) {
        other.relay_client_ = nullptr;
    }
    
    RelayClient& operator=(RelayClient&& other) noexcept {
        if (this != &other) {
            if (relay_client_) {
                ur_rpc_relay_client_destroy(relay_client_);
            }
            relay_client_ = other.relay_client_;
            other.relay_client_ = nullptr;
        }
        return *this;
    }
    
    void start() {
        int result = ur_rpc_relay_client_start(relay_client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to start relay client");
        }
    }
    
    void stop() {
        int result = ur_rpc_relay_client_stop(relay_client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to stop relay client");
        }
    }
    
    static void setSecondaryConnectionReady(bool ready) {
        ur_rpc_relay_set_secondary_connection_ready(ready);
    }
    
    static bool isSecondaryConnectionReady() {
        return ur_rpc_relay_is_secondary_connection_ready();
    }
    
    void connectSecondaryBrokers() {
        int result = ur_rpc_relay_connect_secondary_brokers(relay_client_);
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to connect secondary brokers");
        }
    }
};

// Library initialization class (RAII)
class Library {
public:
    Library() {
        int result = ur_rpc_init();
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to initialize UR-RPC library");
        }
    }
    
    ~Library() {
        ur_rpc_cleanup();
    }
    
    // Delete copy constructor and assignment
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    
    // Delete move constructor and assignment
    Library(Library&&) = delete;
    Library& operator=(Library&&) = delete;
};

// Utility functions
inline std::string authorityToString(Authority authority) {
    return std::string(ur_rpc_authority_to_string(static_cast<ur_rpc_authority_t>(authority)));
}

inline Authority authorityFromString(const std::string& authority_str) {
    return static_cast<Authority>(ur_rpc_authority_from_string(authority_str.c_str()));
}

inline std::string connectionStatusToString(ConnectionStatus status) {
    return std::string(ur_rpc_connection_status_to_string(static_cast<ur_rpc_connection_status_t>(status)));
}

inline uint64_t getTimestampMs() {
    return ur_rpc_get_timestamp_ms();
}

inline std::string getErrorString(int error_code) {
    const char* error_str = ur_rpc_error_string(static_cast<ur_rpc_error_t>(error_code));
    return error_str ? std::string(error_str) : "Unknown error";
}

// TopicList class for managing topic lists
class TopicList {
private:
    ur_rpc_topic_list_t list_;
    
public:
    TopicList() {
        if (ur_rpc_topic_list_init(&list_) != UR_RPC_SUCCESS) {
            throw Exception("Failed to initialize topic list");
        }
    }
    
    ~TopicList() {
        ur_rpc_topic_list_cleanup(&list_);
    }
    
    // Delete copy constructor and assignment
    TopicList(const TopicList&) = delete;
    TopicList& operator=(const TopicList&) = delete;
    
    // Move constructor and assignment
    TopicList(TopicList&& other) noexcept {
        list_ = other.list_;
        other.list_.topics = nullptr;
        other.list_.count = 0;
        other.list_.capacity = 0;
    }
    
    TopicList& operator=(TopicList&& other) noexcept {
        if (this != &other) {
            ur_rpc_topic_list_cleanup(&list_);
            list_ = other.list_;
            other.list_.topics = nullptr;
            other.list_.count = 0;
            other.list_.capacity = 0;
        }
        return *this;
    }
    
    void addTopic(const std::string& topic) {
        int result = ur_rpc_topic_list_add(&list_, topic.c_str());
        if (result != UR_RPC_SUCCESS) {
            throw Exception("Failed to add topic: " + topic);
        }
    }
    
    int getCount() const {
        return list_.count;
    }
    
    std::vector<std::string> getTopics() const {
        std::vector<std::string> topics;
        for (int i = 0; i < list_.count; ++i) {
            if (list_.topics[i]) {
                topics.emplace_back(list_.topics[i]);
            }
        }
        return topics;
    }
    
    const ur_rpc_topic_list_t* getNativeHandle() const {
        return &list_;
    }
};

// Transaction ID utilities
inline std::string generateTransactionId() {
    char* id = ur_rpc_generate_transaction_id();
    if (!id) {
        throw Exception("Failed to generate transaction ID");
    }
    std::string result(id);
    ur_rpc_free_string(id);
    return result;
}

inline bool validateTransactionId(const std::string& transaction_id) {
    return ur_rpc_validate_transaction_id(transaction_id.c_str());
}

// JSON conversion utilities
inline std::string requestToJson(const Request& request) {
    char* json_str = ur_rpc_request_to_json(request.getNativeHandle());
    if (!json_str) {
        throw Exception("Failed to convert request to JSON");
    }
    std::string result(json_str);
    ur_rpc_free_string(json_str);
    return result;
}

inline Request requestFromJson(const std::string& json_str) {
    ur_rpc_request_t* request = ur_rpc_request_from_json(json_str.c_str());
    if (!request) {
        throw Exception("Failed to parse request from JSON");
    }
    return Request(request, true);
}

inline std::string responseToJson(const Response& response) {
    char* json_str = ur_rpc_response_to_json(response.getNativeHandle());
    if (!json_str) {
        throw Exception("Failed to convert response to JSON");
    }
    std::string result(json_str);
    ur_rpc_free_string(json_str);
    return result;
}

inline Response responseFromJson(const std::string& json_str) {
    ur_rpc_response_t* response = ur_rpc_response_from_json(json_str.c_str());
    if (!response) {
        throw Exception("Failed to parse response from JSON");
    }
    return Response(response, true);
}

} // namespace UrRpc

#endif // UR_RPC_TEMPLATE_HPP