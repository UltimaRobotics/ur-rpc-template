#include "ur-rpc-template.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <map>
#include <vector>
#include <random>
#include <atomic>

using namespace UrRpc;

static std::atomic<bool> running(true);
static std::atomic<int> transaction_counter(0);

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    running = false;
}

class AdvancedRpcClient {
private:
    std::unique_ptr<Client> client_;
    std::string client_id_;
    std::map<std::string, uint64_t> pending_requests_;
    std::map<std::string, std::string> method_handlers_;
    std::mt19937 rng_;

public:
    AdvancedRpcClient(const std::string& client_id) 
        : client_id_(client_id), rng_(std::random_device{}()) {
        
        // Register method handlers
        method_handlers_["ping"] = "Basic ping response";
        method_handlers_["calculate"] = "Mathematical calculation";
        method_handlers_["data_process"] = "Data processing service";
        method_handlers_["status_check"] = "System status verification";
        method_handlers_["file_operation"] = "File system operation";
    }
    
    void initialize(const ClientConfig& config, const TopicConfig& topicConfig) {
        client_ = std::make_unique<Client>(config, topicConfig);
        
        // Advanced message handler with RPC processing
        client_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            handleAdvancedMessage(topic, payload);
        });
        
        // Connection callback with advanced reconnection logic
        client_->setConnectionCallback([this](ConnectionStatus status) {
            handleConnectionStatus(status);
        });
    }
    
    void start() {
        std::cout << "🚀 [" << client_id_ << "] Starting advanced RPC client..." << std::endl;
        client_->connect();
        client_->start();
        
        // Start heartbeat monitoring
        try {
            client_->startHeartbeat();
            std::cout << "💓 [" << client_id_ << "] Heartbeat monitoring active" << std::endl;
        } catch (const Exception& e) {
            std::cout << "⚠️  [" << client_id_ << "] Heartbeat start failed: " << e.what() << std::endl;
        }
        
        // Wait for connection with progress indication
        int attempts = 0;
        while (!client_->isConnected() && attempts < 15 && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
            if (attempts % 3 == 0) {
                std::cout << "🔄 [" << client_id_ << "] Connecting... (" << attempts/2 << "s)" << std::endl;
            }
        }
        
        if (!client_->isConnected()) {
            throw ConnectionException("Failed to connect after 7.5 seconds");
        }
        
        std::cout << "✅ [" << client_id_ << "] Advanced client connected and ready" << std::endl;
        setupSubscriptions();
    }
    
    void setupSubscriptions() {
        std::vector<std::string> subscriptions = {
            "cpp_rpc/advanced/" + client_id_ + "/+",
            "cpp_rpc/advanced/broadcast/+",
            "cpp_rpc/advanced/services/+",
            "cpp_rpc/advanced/requests/+",
            "cpp_rpc/advanced/responses/" + client_id_ + "/+",
            "advanced_notifications/+",
            "system_events/+"
        };
        
        for (const auto& topic : subscriptions) {
            client_->subscribeTopic(topic);
        }
        
        std::cout << "✅ [" << client_id_ << "] Subscribed to " << subscriptions.size() << " advanced topics" << std::endl;
    }
    
    void handleAdvancedMessage(const std::string& topic, const std::string& payload) {
        try {
            JsonValue message(payload);
            auto msg_type = message.getString("type");
            auto sender = message.getString("sender");
            auto method = message.getString("method");
            auto transaction_id = message.getString("transaction_id");
            
            if (!msg_type) return;
            
            std::cout << "📨 [" << client_id_ << "] " << *msg_type;
            if (sender) std::cout << " from " << *sender;
            if (method) std::cout << " method: " << *method;
            std::cout << std::endl;
            
            if (*msg_type == "rpc_request" && method && transaction_id) {
                handleRpcRequest(*method, *transaction_id, message, sender ? *sender : "unknown");
            } else if (*msg_type == "rpc_response" && transaction_id) {
                handleRpcResponse(*transaction_id, message);
            } else if (*msg_type == "notification") {
                handleNotification(message);
            } else if (*msg_type == "service_discovery") {
                handleServiceDiscovery(message, sender ? *sender : "unknown");
            } else if (*msg_type == "batch_request") {
                handleBatchRequest(message, sender ? *sender : "unknown");
            }
            
        } catch (const Exception& e) {
            std::cout << "⚠️  [" << client_id_ << "] Message parsing error: " << e.what() << std::endl;
        }
    }
    
    void handleConnectionStatus(ConnectionStatus status) {
        std::cout << "🔗 [" << client_id_ << "] Connection: " << connectionStatusToString(status) << std::endl;
        
        if (status == ConnectionStatus::Connected) {
            publishServiceAnnouncement();
        } else if (status == ConnectionStatus::Reconnecting) {
            std::cout << "🔄 [" << client_id_ << "] Attempting reconnection..." << std::endl;
        } else if (status == ConnectionStatus::Error) {
            std::cout << "❌ [" << client_id_ << "] Connection error detected" << std::endl;
        }
    }
    
    void handleRpcRequest(const std::string& method, const std::string& transaction_id, 
                         const JsonValue& request, const std::string& sender) {
        
        std::cout << "🔧 [" << client_id_ << "] Processing RPC: " << method << " from " << sender << std::endl;
        
        // Simulate processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (rng_() % 200)));
        
        JsonValue response;
        response.addString("type", "rpc_response");
        response.addString("sender", client_id_);
        response.addString("recipient", sender);
        response.addString("transaction_id", transaction_id);
        response.addString("method", method);
        response.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        // Generate method-specific response
        if (method_handlers_.find(method) != method_handlers_.end()) {
            response.addBool("success", true);
            
            JsonValue result;
            result.addString("handler", method_handlers_[method]);
            result.addString("processed_by", client_id_);
            
            if (method == "calculate") {
                auto param_a = request.getNumber("param_a");
                auto param_b = request.getNumber("param_b");
                auto operation = request.getString("operation");
                
                if (param_a && param_b && operation) {
                    double calc_result = 0;
                    if (*operation == "add") calc_result = *param_a + *param_b;
                    else if (*operation == "multiply") calc_result = *param_a * *param_b;
                    else if (*operation == "subtract") calc_result = *param_a - *param_b;
                    else calc_result = *param_a;
                    
                    result.addNumber("calculation_result", calc_result);
                    result.addString("operation_performed", *operation);
                }
            } else if (method == "status_check") {
                result.addString("status", "operational");
                result.addNumber("cpu_usage", 15 + (rng_() % 70));
                result.addNumber("memory_usage", 20 + (rng_() % 60));
                result.addNumber("uptime", getTimestampMs() / 1000);
            } else if (method == "data_process") {
                auto data_size = request.getNumber("data_size");
                result.addString("processing_status", "completed");
                if (data_size) {
                    result.addNumber("processed_bytes", *data_size);
                    result.addNumber("processing_time_ms", (*data_size / 1000) + (rng_() % 100));
                }
            }
            
            result.addNumber("processing_time", 50 + (rng_() % 200));
            response.addString("result", result.toString());
            
        } else {
            response.addBool("success", false);
            response.addString("error", "Unknown method: " + method);
            response.addNumber("error_code", 404);
        }
        
        // Send response
        std::string response_topic = "cpp_rpc/advanced/responses/" + sender + "/" + method;
        client_->publishMessage(response_topic, response.toString());
        
        std::cout << "✅ [" << client_id_ << "] Sent RPC response for " << method << std::endl;
    }
    
    void handleRpcResponse(const std::string& transaction_id, const JsonValue& response) {
        auto it = pending_requests_.find(transaction_id);
        if (it != pending_requests_.end()) {
            auto success = response.getBool("success");
            auto method = response.getString("method");
            auto processing_time = response.getNumber("processing_time");
            
            std::cout << "✅ [" << client_id_ << "] RPC response received:";
            if (method) std::cout << " " << *method;
            if (success) std::cout << " (" << (*success ? "success" : "failed") << ")";
            if (processing_time) std::cout << " in " << *processing_time << "ms";
            std::cout << std::endl;
            
            pending_requests_.erase(it);
        }
    }
    
    void handleNotification(const JsonValue& notification) {
        auto event = notification.getString("event");
        auto priority = notification.getString("priority");
        auto data = notification.getString("data");
        
        std::cout << "🔔 [" << client_id_ << "] Notification:";
        if (event) std::cout << " " << *event;
        if (priority) std::cout << " (priority: " << *priority << ")";
        std::cout << std::endl;
        
        if (data) {
            std::cout << "   Data: " << *data << std::endl;
        }
    }
    
    void handleServiceDiscovery(const JsonValue& message, const std::string& sender) {
        auto request_type = message.getString("request_type");
        
        if (request_type && *request_type == "service_list") {
            std::cout << "🔍 [" << client_id_ << "] Service discovery request from " << sender << std::endl;
            publishServiceInfo(sender);
        }
    }
    
    void handleBatchRequest(const JsonValue& message, const std::string& sender) {
        auto batch_id = message.getString("batch_id");
        auto request_count = message.getNumber("request_count");
        
        std::cout << "📦 [" << client_id_ << "] Batch request from " << sender;
        if (batch_id) std::cout << " (ID: " << *batch_id << ")";
        if (request_count) std::cout << " with " << *request_count << " requests";
        std::cout << std::endl;
        
        // Process batch (simplified)
        JsonValue batchResponse;
        batchResponse.addString("type", "batch_response");
        batchResponse.addString("sender", client_id_);
        batchResponse.addString("recipient", sender);
        if (batch_id) batchResponse.addString("batch_id", *batch_id);
        batchResponse.addBool("success", true);
        batchResponse.addString("status", "all_processed");
        batchResponse.addNumber("processed_count", request_count ? *request_count : 0);
        batchResponse.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        client_->publishMessage("cpp_rpc/advanced/responses/" + sender + "/batch", batchResponse.toString());
        std::cout << "✅ [" << client_id_ << "] Sent batch response" << std::endl;
    }
    
    void publishServiceAnnouncement() {
        JsonValue announcement;
        announcement.addString("type", "service_announcement");
        announcement.addString("service_id", client_id_);
        announcement.addString("version", "1.0.0");
        announcement.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        // List available methods
        std::string methods = "";
        for (const auto& pair : method_handlers_) {
            if (!methods.empty()) methods += ",";
            methods += pair.first;
        }
        announcement.addString("available_methods", methods);
        announcement.addString("capabilities", "rpc,notifications,batch_processing,service_discovery");
        
        client_->publishMessage("cpp_rpc/advanced/broadcast/service_announcement", announcement.toString());
        std::cout << "📢 [" << client_id_ << "] Published service announcement" << std::endl;
    }
    
    void publishServiceInfo(const std::string& requester) {
        JsonValue serviceInfo;
        serviceInfo.addString("type", "service_info");
        serviceInfo.addString("service_id", client_id_);
        serviceInfo.addString("requester", requester);
        
        // Detailed service information
        JsonValue capabilities;
        capabilities.addString("type", "service_capabilities");
        for (const auto& pair : method_handlers_) {
            capabilities.addString(pair.first, pair.second);
        }
        
        serviceInfo.addString("capabilities", capabilities.toString());
        serviceInfo.addString("status", "online");
        serviceInfo.addNumber("load", 10 + (rng_() % 80));
        serviceInfo.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        client_->publishMessage("cpp_rpc/advanced/services/" + requester, serviceInfo.toString());
        std::cout << "📋 [" << client_id_ << "] Sent service info to " << requester << std::endl;
    }
    
    void sendAdvancedRpcRequest(const std::string& target, const std::string& method, const JsonValue& params) {
        std::string transaction_id = Client::generateTransactionId();
        pending_requests_[transaction_id] = getTimestampMs();
        
        JsonValue request;
        request.addString("type", "rpc_request");
        request.addString("sender", client_id_);
        request.addString("recipient", target);
        request.addString("method", method);
        request.addString("transaction_id", transaction_id);
        request.addString("params", params.toString());
        request.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        request.addString("priority", "normal");
        request.addNumber("timeout_ms", 10000);
        
        std::string topic = "cpp_rpc/advanced/requests/" + method;
        client_->publishMessage(topic, request.toString());
        
        transaction_counter++;
        std::cout << "📤 [" << client_id_ << "] Sent RPC '" << method << "' to " << target 
                  << " (tx #" << transaction_counter.load() << ")" << std::endl;
    }
    
    void sendBatchRequest(const std::string& target, const std::vector<std::string>& methods) {
        std::string batch_id = Client::generateTransactionId();
        
        JsonValue batchRequest;
        batchRequest.addString("type", "batch_request");
        batchRequest.addString("sender", client_id_);
        batchRequest.addString("recipient", target);
        batchRequest.addString("batch_id", batch_id);
        batchRequest.addNumber("request_count", methods.size());
        batchRequest.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        std::string methods_str = "";
        for (const auto& method : methods) {
            if (!methods_str.empty()) methods_str += ",";
            methods_str += method;
        }
        batchRequest.addString("methods", methods_str);
        
        client_->publishMessage("cpp_rpc/advanced/" + target + "/batch", batchRequest.toString());
        std::cout << "📦 [" << client_id_ << "] Sent batch request to " << target 
                  << " (" << methods.size() << " methods)" << std::endl;
    }
    
    void sendAdvancedNotification(const std::string& event, const std::string& priority, const JsonValue& data) {
        JsonValue notification;
        notification.addString("type", "notification");
        notification.addString("sender", client_id_);
        notification.addString("event", event);
        notification.addString("priority", priority);
        notification.addString("data", data.toString());
        notification.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        client_->publishMessage("advanced_notifications/" + event, notification.toString());
        std::cout << "🔔 [" << client_id_ << "] Sent " << priority << " notification: " << event << std::endl;
    }
    
    void requestServiceDiscovery() {
        JsonValue discovery;
        discovery.addString("type", "service_discovery");
        discovery.addString("sender", client_id_);
        discovery.addString("request_type", "service_list");
        discovery.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        client_->publishMessage("cpp_rpc/advanced/broadcast/service_discovery", discovery.toString());
        std::cout << "🔍 [" << client_id_ << "] Requested service discovery" << std::endl;
    }
    
    void runAdvancedDemo() {
        std::cout << "\n🎯 [" << client_id_ << "] Starting advanced RPC demonstration..." << std::endl;
        
        int demo_step = 0;
        std::vector<std::string> demo_targets = {"cpp_advanced_server", "cpp_service_provider", "cpp_data_processor"};
        
        while (running && client_->isConnected()) {
            demo_step++;
            
            // Different demonstration activities
            if (demo_step % 4 == 1) {
                // Send advanced RPC requests
                std::string target = demo_targets[demo_step % demo_targets.size()];
                
                if (demo_step % 8 == 1) {
                    // Mathematical calculation
                    JsonValue calcParams;
                    calcParams.addNumber("param_a", 10 + (rng_() % 90));
                    calcParams.addNumber("param_b", 5 + (rng_() % 20));
                    calcParams.addString("operation", (demo_step % 16 == 1) ? "add" : "multiply");
                    sendAdvancedRpcRequest(target, "calculate", calcParams);
                    
                } else {
                    // Status check
                    JsonValue statusParams;
                    statusParams.addString("check_type", "full_system");
                    statusParams.addBool("include_metrics", true);
                    sendAdvancedRpcRequest(target, "status_check", statusParams);
                }
                
            } else if (demo_step % 6 == 0) {
                // Send batch request
                std::vector<std::string> batchMethods = {"ping", "status_check", "data_process"};
                std::string target = demo_targets[demo_step % demo_targets.size()];
                sendBatchRequest(target, batchMethods);
                
            } else if (demo_step % 5 == 0) {
                // Send advanced notification
                std::vector<std::string> events = {"system_alert", "data_update", "performance_metric"};
                std::vector<std::string> priorities = {"high", "normal", "low"};
                
                std::string event = events[demo_step % events.size()];
                std::string priority = priorities[demo_step % priorities.size()];
                
                JsonValue notificationData;
                notificationData.addString("source", client_id_);
                notificationData.addNumber("value", rng_() % 1000);
                notificationData.addString("unit", "units");
                
                sendAdvancedNotification(event, priority, notificationData);
                
            } else if (demo_step % 7 == 0) {
                // Request service discovery
                requestServiceDiscovery();
            }
            
            // Show advanced statistics every 10 steps
            if (demo_step % 10 == 0) {
                showAdvancedStatistics();
            }
            
            // Clean up old pending requests
            cleanupPendingRequests();
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    void showAdvancedStatistics() {
        try {
            Statistics stats = client_->getStatistics();
            std::cout << "📊 [" << client_id_ << "] Advanced Statistics:" << std::endl;
            std::cout << "   Messages sent: " << stats.messagesSent << std::endl;
            std::cout << "   Messages received: " << stats.messagesReceived << std::endl;
            std::cout << "   RPC requests sent: " << stats.requestsSent << std::endl;
            std::cout << "   RPC responses received: " << stats.responsesReceived << std::endl;
            std::cout << "   Notifications sent: " << stats.notificationsSent << std::endl;
            std::cout << "   Total transactions: " << transaction_counter.load() << std::endl;
            std::cout << "   Pending requests: " << pending_requests_.size() << std::endl;
            std::cout << "   Available methods: " << method_handlers_.size() << std::endl;
            std::cout << "   Connection uptime: " << stats.uptimeSeconds << "s" << std::endl;
            
        } catch (const Exception& e) {
            std::cout << "⚠️  [" << client_id_ << "] Statistics error: " << e.what() << std::endl;
        }
    }
    
    void cleanupPendingRequests() {
        uint64_t current_time = getTimestampMs();
        auto it = pending_requests_.begin();
        int cleaned = 0;
        
        while (it != pending_requests_.end()) {
            if (current_time - it->second > 30000) {  // 30 second timeout
                it = pending_requests_.erase(it);
                cleaned++;
            } else {
                ++it;
            }
        }
        
        if (cleaned > 0) {
            std::cout << "🧹 [" << client_id_ << "] Cleaned up " << cleaned << " expired requests" << std::endl;
        }
    }
    
    void shutdown() {
        if (!client_) return;
        
        std::cout << "🛑 [" << client_id_ << "] Shutting down advanced client..." << std::endl;
        
        // Send farewell notification
        JsonValue farewell;
        farewell.addString("service_shutdown", client_id_);
        farewell.addNumber("total_transactions", transaction_counter.load());
        farewell.addNumber("pending_requests", pending_requests_.size());
        farewell.addString("shutdown_reason", "clean_termination");
        farewell.addNumber("uptime_seconds", getTimestampMs() / 1000);
        
        sendAdvancedNotification("service_shutdown", "high", farewell);
        
        // Stop heartbeat
        try {
            client_->stopHeartbeat();
            std::cout << "💓 [" << client_id_ << "] Heartbeat stopped" << std::endl;
        } catch (const Exception& e) {
            std::cout << "⚠️  [" << client_id_ << "] Heartbeat stop error: " << e.what() << std::endl;
        }
        
        client_->stop();
        client_->disconnect();
        std::cout << "✅ [" << client_id_ << "] Advanced client shutdown complete" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "======================================" << std::endl;
        std::cout << "  C++ Advanced RPC Features Demo     " << std::endl;
        std::cout << "======================================" << std::endl;
        
        // Initialize library
        Library library;
        std::cout << "✅ UR-RPC library initialized" << std::endl;
        
        // Determine client identity
        std::string client_id = "cpp_advanced_client";
        if (argc > 2) {
            client_id = argv[2];
        }
        
        std::cout << "🆔 Advanced Client ID: " << client_id << std::endl;
        
        // Create configuration
        ClientConfig config;
        
        if (argc > 1) {
            config.loadFromFile(argv[1]);
            std::cout << "✅ Configuration loaded from: " << argv[1] << std::endl;
        } else {
            config.setBroker("localhost", 1883)
                  .setClientId(client_id)
                  .setTimeouts(15, 45)
                  .setReconnect(true, 5, 30)
                  .setHeartbeat("cpp_rpc/advanced/heartbeat", 20, 
                               "{\"client\":\"" + client_id + "\",\"status\":\"advanced_active\"}");
            std::cout << "✅ Using default advanced configuration" << std::endl;
        }
        
        // Create topic configuration
        TopicConfig topicConfig;
        topicConfig.setPrefixes("cpp_rpc", "advanced")
                   .setSuffixes("request", "response", "notification");
        
        // Create and initialize advanced client
        AdvancedRpcClient advancedClient(client_id);
        advancedClient.initialize(config, topicConfig);
        
        // Start client
        advancedClient.start();
        
        std::cout << "\n🎯 Advanced RPC features active... Press Ctrl+C to stop" << std::endl;
        std::cout << "============================================================" << std::endl;
        
        // Run advanced demonstration
        advancedClient.runAdvancedDemo();
        
        // Cleanup
        advancedClient.shutdown();
        
    } catch (const Exception& e) {
        std::cerr << "❌ Advanced Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ Unexpected advanced error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "👋 Advanced RPC Client finished successfully" << std::endl;
    return 0;
}