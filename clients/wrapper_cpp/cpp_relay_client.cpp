#include "ur-rpc-template.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>

using namespace UrRpc;

static std::atomic<bool> running(true);

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "======================================" << std::endl;
        std::cout << "  C++ Relay Client Example           " << std::endl;
        std::cout << "======================================" << std::endl;
        
        // Initialize library
        Library library;
        std::cout << "✅ UR-RPC library initialized" << std::endl;
        
        // Create relay client configuration
        ClientConfig config;
        
        if (argc > 1) {
            config.loadFromFile(argv[1]);
            std::cout << "✅ Configuration loaded from: " << argv[1] << std::endl;
        } else {
            // Default relay configuration for multi-broker setup
            config.setBroker("localhost", 1883)  // Primary broker
                  .setClientId("cpp_relay_client")
                  .setTimeouts(10, 30)
                  .setReconnect(true, 5, 30);
            std::cout << "✅ Using default relay configuration" << std::endl;
        }
        
        // Create relay client
        RelayClient relayClient(config);
        std::cout << "✅ Relay client created" << std::endl;
        
        // Create a regular client for monitoring and control
        TopicConfig topicConfig;
        topicConfig.setPrefixes("cpp_relay", "monitor")
                   .setSuffixes("request", "response", "notification");
        
        Client monitorClient(config, topicConfig);
        std::cout << "✅ Monitor client created" << std::endl;
        
        // Set up monitor client message handler
        monitorClient.setMessageHandler([](const std::string& topic, const std::string& payload) {
            std::cout << "📡 Relay Monitor - Message received:" << std::endl;
            std::cout << "   Topic: " << topic << std::endl;
            std::cout << "   Payload: " << payload << std::endl;
            
            // Parse relay status messages
            try {
                JsonValue message(payload);
                auto msg_type = message.getString("type");
                auto status = message.getString("status");
                auto relay_count = message.getNumber("relay_count");
                
                if (msg_type && *msg_type == "relay_status") {
                    std::cout << "🔄 Relay Status Update:" << std::endl;
                    if (status) std::cout << "   Status: " << *status << std::endl;
                    if (relay_count) std::cout << "   Messages relayed: " << *relay_count << std::endl;
                }
            } catch (const Exception&) {
                // Not a status message, just log normally
            }
        });
        
        // Set up connection callback for monitor
        monitorClient.setConnectionCallback([](ConnectionStatus status) {
            std::cout << "🔗 Monitor connection: " << connectionStatusToString(status) << std::endl;
        });
        
        // Connect monitor client
        std::cout << "\n🚀 Connecting monitor client..." << std::endl;
        monitorClient.connect();
        monitorClient.start();
        
        // Wait for monitor connection
        int attempts = 0;
        while (!monitorClient.isConnected() && attempts < 10 && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
        }
        
        if (!monitorClient.isConnected()) {
            throw ConnectionException("Failed to connect monitor client");
        }
        
        std::cout << "✅ Monitor client connected" << std::endl;
        
        // Subscribe to relay monitoring topics
        monitorClient.subscribeTopic("relay/status/+");
        monitorClient.subscribeTopic("relay/control/+");
        monitorClient.subscribeTopic("forwarded/+");
        monitorClient.subscribeTopic("relayed/+");
        std::cout << "✅ Subscribed to relay monitoring topics" << std::endl;
        
        // Start relay client
        std::cout << "\n🔄 Starting relay client..." << std::endl;
        relayClient.start();
        std::cout << "✅ Relay client started" << std::endl;
        
        // Send relay initialization message
        JsonValue initMessage;
        initMessage.addString("type", "relay_init");
        initMessage.addString("relay_client", "cpp_relay_client");
        initMessage.addString("primary_broker", "localhost:1883");
        initMessage.addString("destination_brokers", "localhost:1885,localhost:1887");
        initMessage.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        monitorClient.publishMessage("relay/status/init", initMessage.toString());
        std::cout << "📡 Sent relay initialization message" << std::endl;
        
        // Demonstrate relay control
        std::cout << "\n🎛️  Testing relay control features..." << std::endl;
        
        // Test secondary connection control
        std::cout << "🔗 Testing secondary connection control..." << std::endl;
        
        // Initially set secondary connection as not ready
        RelayClient::setSecondaryConnectionReady(false);
        std::cout << "📡 Secondary connection marked as not ready" << std::endl;
        
        // Send some test messages to be relayed
        std::cout << "\n📨 Sending test messages for relay..." << std::endl;
        
        for (int i = 1; i <= 5; i++) {
            JsonValue testMessage;
            testMessage.addString("type", "test_relay_message");
            testMessage.addString("source", "cpp_relay_client");
            testMessage.addNumber("sequence", i);
            testMessage.addString("content", "Test message " + std::to_string(i) + " for relay processing");
            testMessage.addNumber("timestamp", static_cast<double>(getTimestampMs()));
            
            // Send to topics that should be relayed
            std::string topic = "data/sensors/temp_" + std::to_string(i);
            monitorClient.publishMessage(topic, testMessage.toString());
            
            std::cout << "📤 Sent message " << i << " to " << topic << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Wait a bit for initial relay attempts
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Now enable secondary connection
        std::cout << "\n🔗 Enabling secondary connection..." << std::endl;
        RelayClient::setSecondaryConnectionReady(true);
        
        // Try to connect secondary brokers
        try {
            relayClient.connectSecondaryBrokers();
            std::cout << "✅ Secondary brokers connection initiated" << std::endl;
        } catch (const Exception& e) {
            std::cout << "⚠️  Secondary broker connection failed: " << e.what() << std::endl;
        }
        
        // Send more test messages now that secondary is ready
        std::cout << "\n📨 Sending messages with secondary connection active..." << std::endl;
        
        for (int i = 6; i <= 10; i++) {
            JsonValue testMessage;
            testMessage.addString("type", "test_relay_message_secondary");
            testMessage.addString("source", "cpp_relay_client");
            testMessage.addNumber("sequence", i);
            testMessage.addString("content", "Test message " + std::to_string(i) + " with secondary active");
            testMessage.addBool("secondary_ready", RelayClient::isSecondaryConnectionReady());
            testMessage.addNumber("timestamp", static_cast<double>(getTimestampMs()));
            
            // Send to different relay topics
            std::vector<std::string> topics = {
                "data/sensors/humidity",
                "events/system/startup", 
                "commands/restart",
                "data/sensors/pressure",
                "events/system/alert"
            };
            
            std::string topic = topics[(i-6) % topics.size()];
            monitorClient.publishMessage(topic, testMessage.toString());
            
            std::cout << "📤 Sent message " << i << " to " << topic << " (secondary active)" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Main relay monitoring loop
        std::cout << "\n🎧 Relay client monitoring... Press Ctrl+C to stop" << std::endl;
        std::cout << "========================================================" << std::endl;
        
        int loop_count = 0;
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            loop_count++;
            
            // Send periodic relay status
            JsonValue statusMessage;
            statusMessage.addString("type", "relay_status");
            statusMessage.addString("relay_client", "cpp_relay_client");
            statusMessage.addString("status", "active");
            statusMessage.addNumber("loop_count", loop_count);
            statusMessage.addBool("secondary_ready", RelayClient::isSecondaryConnectionReady());
            statusMessage.addString("monitor_connection", connectionStatusToString(monitorClient.getStatus()));
            statusMessage.addNumber("timestamp", static_cast<double>(getTimestampMs()));
            
            monitorClient.publishMessage("relay/status/periodic", statusMessage.toString());
            std::cout << "📊 Relay status #" << loop_count << " sent" << std::endl;
            
            // Send test data every 3 loops
            if (loop_count % 3 == 0) {
                JsonValue periodicData;
                periodicData.addString("type", "periodic_relay_test");
                periodicData.addString("source", "cpp_relay_monitor");
                periodicData.addNumber("batch", loop_count / 3);
                periodicData.addString("test_data", "Periodic test data batch " + std::to_string(loop_count / 3));
                periodicData.addNumber("timestamp", static_cast<double>(getTimestampMs()));
                
                monitorClient.publishMessage("data/sensors/periodic", periodicData.toString());
                std::cout << "🔄 Sent periodic test data for relay" << std::endl;
            }
            
            // Show monitor statistics every 5 loops
            if (loop_count % 5 == 0) {
                try {
                    Statistics stats = monitorClient.getStatistics();
                    std::cout << "📊 Monitor Statistics:" << std::endl;
                    std::cout << "   Messages sent: " << stats.messagesSent << std::endl;
                    std::cout << "   Messages received: " << stats.messagesReceived << std::endl;
                    std::cout << "   Monitor uptime: " << stats.uptimeSeconds << "s" << std::endl;
                    std::cout << "   Secondary connection ready: " << (RelayClient::isSecondaryConnectionReady() ? "Yes" : "No") << std::endl;
                } catch (const Exception& e) {
                    std::cout << "⚠️  Failed to get statistics: " << e.what() << std::endl;
                }
            }
            
            // Test secondary connection toggle every 8 loops
            if (loop_count % 8 == 0) {
                bool current_state = RelayClient::isSecondaryConnectionReady();
                RelayClient::setSecondaryConnectionReady(!current_state);
                std::cout << "🔄 Toggled secondary connection: " << (current_state ? "disabled" : "enabled") << std::endl;
                
                // Send control message
                JsonValue controlMessage;
                controlMessage.addString("type", "relay_control");
                controlMessage.addString("action", "secondary_toggle");
                controlMessage.addBool("new_state", !current_state);
                controlMessage.addNumber("timestamp", static_cast<double>(getTimestampMs()));
                
                monitorClient.publishMessage("relay/control/secondary", controlMessage.toString());
            }
        }
        
        // Cleanup
        std::cout << "\n🛑 Shutting down relay client..." << std::endl;
        
        // Send shutdown message
        JsonValue shutdownMessage;
        shutdownMessage.addString("type", "relay_shutdown");
        shutdownMessage.addString("relay_client", "cpp_relay_client");
        shutdownMessage.addNumber("total_loops", loop_count);
        shutdownMessage.addString("shutdown_reason", "clean_termination");
        shutdownMessage.addNumber("shutdown_time", static_cast<double>(getTimestampMs()));
        
        monitorClient.publishMessage("relay/status/shutdown", shutdownMessage.toString());
        
        // Stop relay client
        relayClient.stop();
        std::cout << "✅ Relay client stopped" << std::endl;
        
        // Stop monitor client
        monitorClient.stop();
        monitorClient.disconnect();
        std::cout << "✅ Monitor client disconnected" << std::endl;
        
    } catch (const Exception& e) {
        std::cerr << "❌ Relay Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ Unexpected relay error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "👋 C++ Relay Client finished successfully" << std::endl;
    return 0;
}