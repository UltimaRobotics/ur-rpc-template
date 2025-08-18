#include "ur-rpc-template.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <fstream>

using namespace UrRpc;

static bool running = true;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    running = false;
}

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "======================================" << std::endl;
        std::cout << "  C++ SSL/TLS RPC Client Example     " << std::endl;
        std::cout << "======================================" << std::endl;
        
        // Initialize library
        Library library;
        std::cout << "✅ UR-RPC library initialized" << std::endl;
        
        // Create SSL client configuration
        ClientConfig config;
        
        if (argc > 1) {
            config.loadFromFile(argv[1]);
            std::cout << "✅ Configuration loaded from: " << argv[1] << std::endl;
        } else {
            // Default SSL configuration
            config.setBroker("localhost", 1884)  // SSL broker port
                  .setClientId("cpp_ssl_client")
                  .setTimeouts(15, 45)  // Longer timeouts for SSL
                  .setReconnect(true, 10, 60);
            
            // Check if SSL certificates exist
            std::string ca_file = "../../ssl_certs/ca.crt";
            std::string cert_file = "../../ssl_certs/client.crt";
            std::string key_file = "../../ssl_certs/client.key";
            
            if (fileExists(ca_file)) {
                std::cout << "🔒 Setting up SSL/TLS with certificates..." << std::endl;
                config.setTLS(ca_file, cert_file, key_file)
                      .setTLSVersion("tlsv1.2")
                      .setTLSInsecure(false);  // Secure mode
                std::cout << "✅ SSL certificates configured" << std::endl;
            } else {
                std::cout << "⚠️  SSL certificates not found, using insecure mode" << std::endl;
                config.setTLS("")  // Empty CA file
                      .setTLSInsecure(true);  // Insecure mode for testing
            }
            
            std::cout << "✅ Using default SSL configuration" << std::endl;
        }
        
        // Create topic configuration
        TopicConfig topicConfig;
        topicConfig.setPrefixes("cpp_ssl_rpc", "secure_service")
                   .setSuffixes("request", "response", "notification");
        
        // Create client
        Client client(config, topicConfig);
        std::cout << "✅ SSL RPC client created" << std::endl;
        
        // Set up message handler
        client.setMessageHandler([](const std::string& topic, const std::string& payload) {
            std::cout << "🔒 Secure message received:" << std::endl;
            std::cout << "   Topic: " << topic << std::endl;
            std::cout << "   Payload: " << payload << std::endl;
            
            // Try to parse as JSON for better display
            try {
                JsonValue parsed(payload);
                auto msg_type = parsed.getString("type");
                auto sender = parsed.getString("sender");
                auto timestamp = parsed.getNumber("timestamp");
                
                if (msg_type) std::cout << "   Type: " << *msg_type << std::endl;
                if (sender) std::cout << "   Sender: " << *sender << std::endl;
                if (timestamp) {
                    std::cout << "   Timestamp: " << static_cast<uint64_t>(*timestamp) << std::endl;
                }
            } catch (const Exception&) {
                // Not JSON, already displayed the raw payload
            }
            std::cout << std::endl;
        });
        
        // Set up connection callback
        client.setConnectionCallback([](ConnectionStatus status) {
            std::cout << "🔗 SSL Connection status: " << connectionStatusToString(status) << std::endl;
            
            if (status == ConnectionStatus::Connected) {
                std::cout << "🔒 Secure SSL/TLS connection established!" << std::endl;
            } else if (status == ConnectionStatus::Error) {
                std::cout << "❌ SSL connection error - check certificates and broker configuration" << std::endl;
            }
        });
        
        // Connect and start client
        std::cout << "\n🚀 Connecting to SSL/TLS broker..." << std::endl;
        client.connect();
        client.start();
        
        // Wait for SSL connection (takes longer than regular connection)
        int attempts = 0;
        while (!client.isConnected() && attempts < 20 && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
            if (attempts % 4 == 0) {
                std::cout << "🔄 SSL handshake in progress... (" << attempts/2 << "s)" << std::endl;
            }
        }
        
        if (!client.isConnected()) {
            throw ConnectionException("Failed to establish SSL connection after 10 seconds");
        }
        
        std::cout << "✅ SSL/TLS connection established successfully!" << std::endl;
        
        // Start heartbeat for SSL connection monitoring
        try {
            client.startHeartbeat();
            std::cout << "💓 SSL heartbeat monitoring started" << std::endl;
        } catch (const Exception& e) {
            std::cout << "⚠️  Heartbeat start failed: " << e.what() << std::endl;
        }
        
        // Subscribe to secure topics
        client.subscribeTopic("cpp_ssl_rpc/secure_service/+");
        client.subscribeTopic("secure_notifications/+");
        client.subscribeTopic("encrypted_data/+");
        std::cout << "✅ Subscribed to secure topics" << std::endl;
        
        // Demonstrate secure messaging
        std::cout << "\n🔒 Testing secure SSL/TLS messaging..." << std::endl;
        
        // Publish encrypted authentication message
        JsonValue authMessage;
        authMessage.addString("type", "authentication");
        authMessage.addString("client_id", "cpp_ssl_client");
        authMessage.addString("auth_token", "secure_token_" + std::to_string(getTimestampMs()));
        authMessage.addString("encryption", "TLS_1.2_AES_256");
        authMessage.addNumber("timestamp", static_cast<double>(getTimestampMs()));
        
        client.publishMessage("cpp_ssl_rpc/secure_service/auth", authMessage.toString());
        std::cout << "🔐 Published authentication message over SSL" << std::endl;
        
        // Send secure notification
        JsonValue secureParams;
        secureParams.addString("event", "secure_session_started");
        secureParams.addString("client_type", "cpp_ssl");
        secureParams.addString("security_level", "high");
        secureParams.addNumber("session_id", getTimestampMs());
        
        client.sendNotification("secure_event", "secure_service", Authority::Admin, secureParams);
        std::cout << "🔔 Sent secure notification" << std::endl;
        
        // Demonstrate secure RPC calls
        std::cout << "\n🔧 Testing secure RPC calls..." << std::endl;
        
        // Test synchronous secure RPC
        try {
            Request secureRequest;
            secureRequest.setMethod("secure_ping", "secure_service")
                        .setAuthority(Authority::Admin)
                        .setTimeout(10000);  // Longer timeout for SSL
            
            JsonValue secureParams;
            secureParams.addString("message", "secure ping from SSL client");
            secureParams.addString("encryption_info", "TLS encrypted payload");
            secureParams.addNumber("timestamp", static_cast<double>(getTimestampMs()));
            secureRequest.setParams(secureParams);
            
            std::cout << "🔄 Sending secure RPC call..." << std::endl;
            Response response = client.callSync(secureRequest, 10000);
            
            if (response.isSuccess()) {
                std::cout << "✅ Secure RPC call successful:" << std::endl;
                std::cout << "   Result: " << response.getResult().toString() << std::endl;
                std::cout << "   Processing time: " << response.getProcessingTime() << "ms" << std::endl;
                std::cout << "   Transaction ID: " << response.getTransactionId() << std::endl;
            } else {
                std::cout << "❌ Secure RPC call failed: " << response.getErrorMessage() << std::endl;
            }
        } catch (const Exception& e) {
            std::cout << "⚠️  Secure RPC call error: " << e.what() << std::endl;
        }
        
        // Test asynchronous secure RPC
        std::cout << "\n🔄 Testing asynchronous secure RPC..." << std::endl;
        Request asyncSecureRequest;
        asyncSecureRequest.setMethod("process_encrypted_data", "secure_service")
                         .setAuthority(Authority::Admin)
                         .setTimeout(15000);
        
        JsonValue asyncParams;
        asyncParams.addString("data_type", "sensitive_information");
        asyncParams.addString("client_cert_fingerprint", "SSL_CLIENT_CERT_FP");
        asyncParams.addNumber("data_size", 1024);
        asyncParams.addBool("requires_encryption", true);
        asyncSecureRequest.setParams(asyncParams);
        
        client.callAsync(asyncSecureRequest, [](bool success, const JsonValue& result, const std::string& error_message, int error_code) {
            if (success) {
                std::cout << "✅ Async secure RPC successful:" << std::endl;
                std::cout << "   Result: " << result.toString() << std::endl;
            } else {
                std::cout << "❌ Async secure RPC failed: " << error_message << " (code: " << error_code << ")" << std::endl;
            }
        });
        
        // Main SSL monitoring loop
        std::cout << "\n🎧 SSL client running... Press Ctrl+C to stop" << std::endl;
        std::cout << "=================================================" << std::endl;
        
        int ssl_message_count = 0;
        while (running && client.isConnected()) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            
            ssl_message_count++;
            
            // Send periodic secure status updates
            JsonValue statusUpdate;
            statusUpdate.addString("type", "secure_status");
            statusUpdate.addString("client_id", "cpp_ssl_client");
            statusUpdate.addNumber("sequence", ssl_message_count);
            statusUpdate.addString("connection_status", "ssl_active");
            statusUpdate.addString("cipher_suite", "TLS_AES_256_GCM_SHA384");
            statusUpdate.addNumber("timestamp", static_cast<double>(getTimestampMs()));
            
            // Simulate some secure data
            JsonValue secureData;
            secureData.addString("sensor_reading", "encrypted_sensor_data_" + std::to_string(ssl_message_count));
            secureData.addNumber("value", 100 + (ssl_message_count % 50));
            secureData.addBool("verified", true);
            statusUpdate.addString("secure_payload", secureData.toString());
            
            client.publishMessage("encrypted_data/status", statusUpdate.toString());
            std::cout << "🔒 Secure status update #" << ssl_message_count << " sent over SSL" << std::endl;
            
            // Show SSL statistics every 3 updates
            if (ssl_message_count % 3 == 0) {
                try {
                    Statistics stats = client.getStatistics();
                    std::cout << "📊 SSL Statistics:" << std::endl;
                    std::cout << "   Encrypted messages sent: " << stats.messagesSent << std::endl;
                    std::cout << "   Encrypted messages received: " << stats.messagesReceived << std::endl;
                    std::cout << "   Secure requests sent: " << stats.requestsSent << std::endl;
                    std::cout << "   Secure responses received: " << stats.responsesReceived << std::endl;
                    std::cout << "   SSL errors: " << stats.errorsCount << std::endl;
                    std::cout << "   Connection uptime: " << stats.uptimeSeconds << "s" << std::endl;
                } catch (const Exception& e) {
                    std::cout << "⚠️  Failed to get SSL statistics: " << e.what() << std::endl;
                }
            }
            
            // Test SSL connection health
            if (ssl_message_count % 5 == 0) {
                std::cout << "🩺 SSL connection health check:" << std::endl;
                std::cout << "   Status: " << connectionStatusToString(client.getStatus()) << std::endl;
                std::cout << "   Connected: " << (client.isConnected() ? "Yes" : "No") << std::endl;
            }
        }
        
        // Cleanup
        std::cout << "\n🛑 Shutting down SSL client..." << std::endl;
        
        // Send secure shutdown notification
        JsonValue shutdownParams;
        shutdownParams.addString("event", "secure_session_ended");
        shutdownParams.addString("client_id", "cpp_ssl_client");
        shutdownParams.addNumber("total_secure_messages", ssl_message_count);
        shutdownParams.addNumber("session_duration", ssl_message_count * 15);  // seconds
        shutdownParams.addString("termination_reason", "clean_shutdown");
        shutdownParams.addNumber("shutdown_time", static_cast<double>(getTimestampMs()));
        
        client.sendNotification("secure_event", "secure_service", Authority::Admin, shutdownParams);
        
        // Stop heartbeat
        try {
            client.stopHeartbeat();
            std::cout << "💓 SSL heartbeat stopped" << std::endl;
        } catch (const Exception& e) {
            std::cout << "⚠️  Heartbeat stop failed: " << e.what() << std::endl;
        }
        
        client.stop();
        client.disconnect();
        std::cout << "✅ SSL client shutdown complete" << std::endl;
        
    } catch (const Exception& e) {
        std::cerr << "❌ SSL Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ Unexpected SSL error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "👋 C++ SSL Client finished successfully" << std::endl;
    return 0;
}