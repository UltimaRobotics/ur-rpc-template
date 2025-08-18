#include "ur-rpc-template.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

using namespace UrRpc;

static bool running = true;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "======================================" << std::endl;
        std::cout << "  C++ Basic RPC Client Example       " << std::endl;
        std::cout << "======================================" << std::endl;
        
        // Initialize the UR-RPC library
        Library library;
        std::cout << "✅ UR-RPC library initialized" << std::endl;
        
        // Create client configuration
        ClientConfig config;
        
        if (argc > 1) {
            // Load configuration from file
            config.loadFromFile(argv[1]);
            std::cout << "✅ Configuration loaded from: " << argv[1] << std::endl;
        } else {
            // Use default configuration
            config.setBroker("localhost", 1883)
                  .setClientId("cpp_basic_client")
                  .setTimeouts(10, 30)
                  .setReconnect(true, 5, 30);
            std::cout << "✅ Using default configuration" << std::endl;
        }
        
        // Create topic configuration
        TopicConfig topicConfig;
        topicConfig.setPrefixes("cpp_rpc", "test_service")
                   .setSuffixes("request", "response", "notification");
        
        // Create client
        Client client(config, topicConfig);
        std::cout << "✅ RPC client created" << std::endl;
        
        // Set up message handler
        client.setMessageHandler([](const std::string& topic, const std::string& payload) {
            std::cout << "📨 Message received:" << std::endl;
            std::cout << "   Topic: " << topic << std::endl;
            std::cout << "   Payload: " << payload << std::endl;
        });
        
        // Set up connection callback
        client.setConnectionCallback([](ConnectionStatus status) {
            std::cout << "🔗 Connection status: " << connectionStatusToString(status) << std::endl;
        });
        
        // Connect and start client
        std::cout << "\n🚀 Connecting to MQTT broker..." << std::endl;
        client.connect();
        client.start();
        
        // Wait for connection
        int attempts = 0;
        while (!client.isConnected() && attempts < 10 && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
        }
        
        if (!client.isConnected()) {
            throw ConnectionException("Failed to connect to broker after 5 seconds");
        }
        
        std::cout << "✅ Connected to broker" << std::endl;
        
        // Subscribe to some topics
        client.subscribeTopic("cpp_rpc/test_service/+");
        client.subscribeTopic("notifications/+");
        std::cout << "✅ Subscribed to topics" << std::endl;
        
        // Demonstrate basic messaging
        std::cout << "\n📡 Testing basic messaging..." << std::endl;
        
        // Publish a simple message
        JsonValue testMessage;
        testMessage.addString("message", "Hello from C++ client!");
        testMessage.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        testMessage.addString("client_type", "cpp_basic");
        
        client.publishMessage("cpp_rpc/test_service/hello", testMessage.toString());
        std::cout << "✅ Published test message" << std::endl;
        
        // Send a notification
        JsonValue notificationParams;
        notificationParams.addString("event", "client_started");
        notificationParams.addString("client_id", "cpp_basic_client");
        notificationParams.addNumber("start_time", static_cast<double>(getTimestampMs()));
        
        client.sendNotification("client_event", "test_service", Authority::User, notificationParams);
        std::cout << "✅ Sent startup notification" << std::endl;
        
        // Demonstrate synchronous RPC call
        std::cout << "\n🔄 Testing synchronous RPC..." << std::endl;
        try {
            Request request;
            request.setMethod("ping", "test_service")
                   .setAuthority(Authority::User)
                   .setTimeout(5000);
            
            JsonValue params;
            params.addString("message", "ping from cpp client");
            params.addNumber("timestamp", static_cast<double>(getTimestampMs()));
            request.setParams(params);
            
            Response response = client.callSync(request, 5000);
            
            if (response.isSuccess()) {
                std::cout << "✅ Sync RPC call successful:" << std::endl;
                std::cout << "   Result: " << response.getResult().toString() << std::endl;
                std::cout << "   Processing time: " << response.getProcessingTime() << "ms" << std::endl;
            } else {
                std::cout << "❌ Sync RPC call failed: " << response.getErrorMessage() << std::endl;
            }
        } catch (const Exception& e) {
            std::cout << "⚠️  RPC call timeout or error: " << e.what() << std::endl;
        }
        
        // Demonstrate asynchronous RPC call
        std::cout << "\n🔄 Testing asynchronous RPC..." << std::endl;
        Request asyncRequest;
        asyncRequest.setMethod("status", "test_service")
                    .setAuthority(Authority::User)
                    .setTimeout(5000);
        
        JsonValue asyncParams;
        asyncParams.addString("request_type", "async_status");
        asyncParams.addString("client_id", "cpp_basic_client");
        asyncRequest.setParams(asyncParams);
        
        client.callAsync(asyncRequest, [](bool success, const JsonValue& result, const std::string& error_message, int error_code) {
            if (success) {
                std::cout << "✅ Async RPC call successful:" << std::endl;
                std::cout << "   Result: " << result.toString() << std::endl;
            } else {
                std::cout << "❌ Async RPC call failed: " << error_message << " (code: " << error_code << ")" << std::endl;
            }
        });
        
        // Run main loop
        std::cout << "\n🎧 Client running... Press Ctrl+C to stop" << std::endl;
        std::cout << "============================================" << std::endl;
        
        int message_count = 0;
        while (running && client.isConnected()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            // Send periodic heartbeat message
            message_count++;
            JsonValue heartbeat;
            heartbeat.addString("type", "heartbeat");
            heartbeat.addNumber("sequence", message_count);
            heartbeat.addNumber("timestamp", static_cast<double>(getTimestampMs()));
            heartbeat.addString("status", "running");
            
            client.publishMessage("cpp_rpc/test_service/heartbeat", heartbeat.toString());
            std::cout << "💓 Heartbeat " << message_count << " sent" << std::endl;
            
            // Show statistics every 5 heartbeats
            if (message_count % 5 == 0) {
                try {
                    Statistics stats = client.getStatistics();
                    std::cout << "📊 Statistics:" << std::endl;
                    std::cout << "   Messages sent: " << stats.messagesSent << std::endl;
                    std::cout << "   Messages received: " << stats.messagesReceived << std::endl;
                    std::cout << "   Requests sent: " << stats.requestsSent << std::endl;
                    std::cout << "   Responses received: " << stats.responsesReceived << std::endl;
                    std::cout << "   Errors: " << stats.errorsCount << std::endl;
                } catch (const Exception& e) {
                    std::cout << "⚠️  Failed to get statistics: " << e.what() << std::endl;
                }
            }
        }
        
        // Cleanup
        std::cout << "\n🛑 Shutting down client..." << std::endl;
        
        // Send shutdown notification
        JsonValue shutdownParams;
        shutdownParams.addString("event", "client_shutdown");
        shutdownParams.addString("client_id", "cpp_basic_client");
        shutdownParams.addNumber("messages_sent", message_count);
        shutdownParams.addNumber("shutdown_time", static_cast<double>(getTimestampMs()));
        
        client.sendNotification("client_event", "test_service", Authority::User, shutdownParams);
        
        client.stop();
        client.disconnect();
        std::cout << "✅ Client shutdown complete" << std::endl;
        
    } catch (const Exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ Unexpected error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "👋 C++ Basic Client finished successfully" << std::endl;
    return 0;
}