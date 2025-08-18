#include "ur-rpc-template.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <random>

using namespace UrRpc;

static std::atomic<bool> running(true);
static std::atomic<int> message_counter(0);

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    running = false;
}

class BidirectionalClient {
private:
    std::unique_ptr<Client> client_;
    std::string client_id_;
    std::string partner_topic_;
    std::mt19937 rng_;
    
public:
    BidirectionalClient(const std::string& client_id, const std::string& partner_topic) 
        : client_id_(client_id), partner_topic_(partner_topic), rng_(std::random_device{}()) {}
    
    void initialize(const ClientConfig& config, const TopicConfig& topicConfig) {
        client_ = std::make_unique<Client>(config, topicConfig);
        
        // Set up message handler for bidirectional communication
        client_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            handleIncomingMessage(topic, payload);
        });
        
        // Set up connection callback
        client_->setConnectionCallback([this](ConnectionStatus status) {
            std::cout << "🔗 [" << client_id_ << "] Connection: " << connectionStatusToString(status) << std::endl;
            
            if (status == ConnectionStatus::Connected) {
                onConnected();
            }
        });
    }
    
    void start() {
        std::cout << "🚀 [" << client_id_ << "] Connecting to broker..." << std::endl;
        client_->connect();
        client_->start();
        
        // Wait for connection
        int attempts = 0;
        while (!client_->isConnected() && attempts < 10 && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
        }
        
        if (!client_->isConnected()) {
            throw ConnectionException("Failed to connect to broker");
        }
        
        std::cout << "✅ [" << client_id_ << "] Connected and ready" << std::endl;
    }
    
    void onConnected() {
        // Subscribe to our own topic and partner's topic
        client_->subscribeTopic("cpp_rpc/bidirectional/" + client_id_ + "/+");
        client_->subscribeTopic("cpp_rpc/bidirectional/" + partner_topic_ + "/+");
        client_->subscribeTopic("cpp_rpc/broadcast/+");
        
        std::cout << "✅ [" << client_id_ << "] Subscribed to communication topics" << std::endl;
        
        // Send introduction message
        sendIntroduction();
    }
    
    void handleIncomingMessage(const std::string& topic, const std::string& payload) {
        message_counter++;
        
        std::cout << "📨 [" << client_id_ << "] Message #" << message_counter << ":" << std::endl;
        std::cout << "   Topic: " << topic << std::endl;
        std::cout << "   Payload: " << payload << std::endl;
        
        // Parse the message
        try {
            JsonValue message(payload);
            auto msg_type = message.getString("type");
            auto sender = message.getString("sender");
            
            if (msg_type && sender && *sender != client_id_) {
                handleMessageByType(*msg_type, message, *sender);
            }
        } catch (const Exception& e) {
            std::cout << "⚠️  [" << client_id_ << "] Failed to parse message: " << e.what() << std::endl;
        }
    }
    
    void handleMessageByType(const std::string& type, const JsonValue& message, const std::string& sender) {
        if (type == "ping") {
            handlePing(message, sender);
        } else if (type == "pong") {
            handlePong(message, sender);
        } else if (type == "rpc_request") {
            handleRpcRequest(message, sender);
        } else if (type == "introduction") {
            handleIntroduction(message, sender);
        } else if (type == "data_exchange") {
            handleDataExchange(message, sender);
        } else {
            std::cout << "🤔 [" << client_id_ << "] Unknown message type: " << type << std::endl;
        }
    }
    
    void handlePing(const JsonValue& message, const std::string& sender) {
        auto ping_id = message.getNumber("ping_id");
        auto timestamp = message.getNumber("timestamp");
        
        std::cout << "🏓 [" << client_id_ << "] Received ping from " << sender;
        if (ping_id) std::cout << " (ID: " << *ping_id << ")";
        std::cout << std::endl;
        
        // Send pong response
        JsonValue pong;
        pong.addString("type", "pong");
        pong.addString("sender", client_id_);
        pong.addString("recipient", sender);
        if (ping_id) pong.addNumber("ping_id", *ping_id);
        if (timestamp) pong.addNumber("original_timestamp", *timestamp);
        pong.addNumber("pong_timestamp", static_cast<double>(getTimestampMs()));
        
        std::string topic = "cpp_rpc/bidirectional/" + sender + "/pong";
        client_->publishMessage(topic, pong.toString());
        
        std::cout << "🏓 [" << client_id_ << "] Sent pong to " << sender << std::endl;
    }
    
    void handlePong(const JsonValue& message, const std::string& sender) {
        auto ping_id = message.getNumber("ping_id");
        auto original_timestamp = message.getNumber("original_timestamp");
        auto pong_timestamp = message.getNumber("pong_timestamp");
        
        std::cout << "🎾 [" << client_id_ << "] Received pong from " << sender;
        if (ping_id) std::cout << " (ID: " << *ping_id << ")";
        
        if (original_timestamp && pong_timestamp) {
            double rtt = *pong_timestamp - *original_timestamp;
            std::cout << " RTT: " << rtt << "ms";
        }
        std::cout << std::endl;
    }
    
    void handleRpcRequest(const JsonValue& message, const std::string& sender) {
        auto method = message.getString("method");
        auto transaction_id = message.getString("transaction_id");
        
        std::cout << "🔧 [" << client_id_ << "] RPC request from " << sender;
        if (method) std::cout << " method: " << *method;
        std::cout << std::endl;
        
        // Simulate RPC processing
        JsonValue response;
        response.addString("type", "rpc_response");
        response.addString("sender", client_id_);
        response.addString("recipient", sender);
        if (transaction_id) response.addString("transaction_id", *transaction_id);
        response.addBool("success", true);
        
        JsonValue result;
        result.addString("status", "processed");
        result.addString("processed_by", client_id_);
        result.addNumber("processing_time", 100 + (rng_() % 400)); // 100-500ms simulated processing
        result.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        response.addString("result", result.toString());
        
        std::string topic = "cpp_rpc/bidirectional/" + sender + "/rpc_response";
        client_->publishMessage(topic, response.toString());
        
        std::cout << "✅ [" << client_id_ << "] Sent RPC response to " << sender << std::endl;
    }
    
    void handleIntroduction(const JsonValue& message, const std::string& sender) {
        auto version = message.getString("version");
        auto capabilities = message.getString("capabilities");
        
        std::cout << "👋 [" << client_id_ << "] Introduction from " << sender;
        if (version) std::cout << " (v" << *version << ")";
        std::cout << std::endl;
        
        if (capabilities) {
            std::cout << "   Capabilities: " << *capabilities << std::endl;
        }
    }
    
    void handleDataExchange(const JsonValue& message, const std::string& sender) {
        auto data_type = message.getString("data_type");
        auto sequence = message.getNumber("sequence");
        
        std::cout << "📊 [" << client_id_ << "] Data exchange from " << sender;
        if (data_type) std::cout << " type: " << *data_type;
        if (sequence) std::cout << " seq: " << *sequence;
        std::cout << std::endl;
    }
    
    void sendIntroduction() {
        JsonValue intro;
        intro.addString("type", "introduction");
        intro.addString("sender", client_id_);
        intro.addString("version", "1.0.0");
        intro.addString("capabilities", "ping,pong,rpc,data_exchange");
        intro.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        client_->publishMessage("cpp_rpc/broadcast/introduction", intro.toString());
        std::cout << "👋 [" << client_id_ << "] Sent introduction broadcast" << std::endl;
    }
    
    void sendPing(const std::string& target) {
        static int ping_counter = 0;
        ping_counter++;
        
        JsonValue ping;
        ping.addString("type", "ping");
        ping.addString("sender", client_id_);
        ping.addString("recipient", target);
        ping.addNumber("ping_id", ping_counter);
        ping.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        std::string topic = "cpp_rpc/bidirectional/" + target + "/ping";
        client_->publishMessage(topic, ping.toString());
        
        std::cout << "🏓 [" << client_id_ << "] Sent ping #" << ping_counter << " to " << target << std::endl;
    }
    
    void sendRpcRequest(const std::string& target, const std::string& method) {
        std::string transaction_id = Client::generateTransactionId();
        
        JsonValue rpc;
        rpc.addString("type", "rpc_request");
        rpc.addString("sender", client_id_);
        rpc.addString("recipient", target);
        rpc.addString("method", method);
        rpc.addString("transaction_id", transaction_id);
        rpc.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        JsonValue params;
        params.addString("request_data", "sample_data_from_" + client_id_);
        params.addNumber("random_value", rng_() % 1000);
        rpc.addString("params", params.toString());
        
        std::string topic = "cpp_rpc/bidirectional/" + target + "/rpc_request";
        client_->publishMessage(topic, rpc.toString());
        
        std::cout << "🔧 [" << client_id_ << "] Sent RPC request '" << method << "' to " << target << std::endl;
    }
    
    void sendDataExchange(const std::string& data_type) {
        static int sequence = 0;
        sequence++;
        
        JsonValue data;
        data.addString("type", "data_exchange");
        data.addString("sender", client_id_);
        data.addString("data_type", data_type);
        data.addNumber("sequence", sequence);
        data.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        // Add some sample data based on type
        if (data_type == "sensor_data") {
            data.addNumber("temperature", 20.0 + (rng_() % 200) / 10.0);
            data.addNumber("humidity", 30.0 + (rng_() % 400) / 10.0);
        } else if (data_type == "status") {
            data.addString("status", "operational");
            data.addNumber("cpu_usage", rng_() % 100);
            data.addNumber("memory_usage", rng_() % 100);
        }
        
        client_->publishMessage("cpp_rpc/broadcast/data", data.toString());
        std::cout << "📊 [" << client_id_ << "] Sent " << data_type << " data #" << sequence << std::endl;
    }
    
    void runMainLoop() {
        int loop_counter = 0;
        
        while (running && client_->isConnected()) {
            loop_counter++;
            
            // Different activities based on loop counter
            if (loop_counter % 3 == 1) {
                // Send ping to partner
                sendPing(partner_topic_);
            } else if (loop_counter % 5 == 0) {
                // Send RPC request
                sendRpcRequest(partner_topic_, "process_data");
            } else if (loop_counter % 7 == 0) {
                // Send data exchange
                sendDataExchange((loop_counter % 14 == 0) ? "sensor_data" : "status");
            }
            
            // Show statistics every 10 loops
            if (loop_counter % 10 == 0) {
                showStatistics();
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
    
    void showStatistics() {
        try {
            Statistics stats = client_->getStatistics();
            std::cout << "📊 [" << client_id_ << "] Statistics:" << std::endl;
            std::cout << "   Messages sent: " << stats.messagesSent << std::endl;
            std::cout << "   Messages received: " << stats.messagesReceived << std::endl;
            std::cout << "   Total messages processed: " << message_counter.load() << std::endl;
            std::cout << "   Connection status: " << connectionStatusToString(client_->getStatus()) << std::endl;
        } catch (const Exception& e) {
            std::cout << "⚠️  [" << client_id_ << "] Failed to get statistics: " << e.what() << std::endl;
        }
    }
    
    void shutdown() {
        if (!client_) return;
        
        std::cout << "🛑 [" << client_id_ << "] Shutting down..." << std::endl;
        
        // Send farewell message
        JsonValue farewell;
        farewell.addString("type", "farewell");
        farewell.addString("sender", client_id_);
        farewell.addNumber("total_messages", message_counter.load());
        farewell.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        client_->publishMessage("cpp_rpc/broadcast/farewell", farewell.toString());
        
        client_->stop();
        client_->disconnect();
        
        std::cout << "✅ [" << client_id_ << "] Shutdown complete" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "======================================" << std::endl;
        std::cout << "  C++ Bidirectional RPC Client       " << std::endl;
        std::cout << "======================================" << std::endl;
        
        // Initialize library
        Library library;
        std::cout << "✅ UR-RPC library initialized" << std::endl;
        
        // Determine client identity
        std::string client_id = "cpp_client_a";
        std::string partner_id = "cpp_client_b";
        
        if (argc > 2) {
            client_id = argv[2];
            partner_id = (client_id == "cpp_client_a") ? "cpp_client_b" : "cpp_client_a";
        }
        
        std::cout << "🆔 Client ID: " << client_id << " (Partner: " << partner_id << ")" << std::endl;
        
        // Create configuration
        ClientConfig config;
        
        if (argc > 1) {
            config.loadFromFile(argv[1]);
            std::cout << "✅ Configuration loaded from: " << argv[1] << std::endl;
        } else {
            config.setBroker("localhost", 1883)
                  .setClientId(client_id)
                  .setTimeouts(10, 30)
                  .setReconnect(true, 5, 30);
            std::cout << "✅ Using default configuration" << std::endl;
        }
        
        // Create topic configuration
        TopicConfig topicConfig;
        topicConfig.setPrefixes("cpp_rpc", "bidirectional")
                   .setSuffixes("request", "response", "notification");
        
        // Create and initialize bidirectional client
        BidirectionalClient biClient(client_id, partner_id);
        biClient.initialize(config, topicConfig);
        
        // Start client
        biClient.start();
        
        std::cout << "\n🎧 Bidirectional communication active... Press Ctrl+C to stop" << std::endl;
        std::cout << "============================================================" << std::endl;
        
        // Run main communication loop
        biClient.runMainLoop();
        
        // Cleanup
        biClient.shutdown();
        
    } catch (const Exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ Unexpected error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "👋 Bidirectional Client finished successfully" << std::endl;
    return 0;
}