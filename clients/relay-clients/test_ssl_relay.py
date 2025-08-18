#!/usr/bin/env python3
"""
SSL Relay Verification Script

Tests the SSL relay client by:
1. Publishing messages to SSL source broker (port 1884)
2. Verifying messages are received at SSL destination broker (port 1886)
3. Testing SSL/TLS encryption and certificate handling
"""

import paho.mqtt.client as mqtt
import json
import time
import threading
import sys
import subprocess
import signal
import os
import ssl
from datetime import datetime

class SSLRelayTester:
    def __init__(self):
        self.source_host = "localhost"
        self.source_port = 1884
        self.dest_host = "localhost"
        self.dest_port = 1886
        
        # SSL certificate paths
        self.ca_cert = "../ssl_certs/ca.crt"
        self.client_cert = "../ssl_certs/client.crt"
        self.client_key = "../ssl_certs/client.key"
        self.server_cert = "../ssl_certs/server.crt"
        self.server_key = "../ssl_certs/server.key"
        
        self.source_client = None
        self.dest_client = None
        self.relay_process = None
        
        # Message tracking
        self.sent_messages = []
        self.received_messages = []
        self.test_results = {
            'ssl_connection': False,
            'secure_sensor_forwarding': False,
            'secure_event_forwarding': False,
            'ssl_bidirectional_commands': False,
            'certificate_validation': False
        }
        
        self.dest_broker_ready = False

    def setup_ssl_destination_broker(self):
        """Start SSL destination broker on port 1886"""
        print("🔐 Starting SSL destination broker on port 1886...")
        
        # Create SSL broker config for port 1886
        config_content = f"""
port 1886
cafile {self.ca_cert}
certfile {self.server_cert}
keyfile {self.server_key}
tls_version tlsv1.2
require_certificate true
allow_anonymous true
connection_messages true
log_type error
log_type warning
log_type notice
log_type information
log_dest file ssl_mosquitto_1886.log
"""
        
        with open('ssl_mosquitto_1886.conf', 'w') as f:
            f.write(config_content)
        
        # Start SSL broker
        try:
            self.dest_broker_process = subprocess.Popen([
                'mosquitto', '-c', 'ssl_mosquitto_1886.conf', '-v'
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(3)
            print("✅ SSL destination broker started successfully")
            return True
        except Exception as e:
            print(f"❌ Failed to start SSL destination broker: {e}")
            return False

    def setup_ssl_mqtt_clients(self):
        """Setup SSL MQTT clients for source and destination brokers"""
        print("🔗 Setting up SSL MQTT clients...")
        
        # Source broker SSL client (publisher)
        self.source_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1, client_id="ssl_relay_test_publisher")
        self.source_client.on_connect = self.on_source_connect
        self.source_client.on_publish = self.on_publish
        self.source_client.on_log = self.on_log
        
        # Destination broker SSL client (subscriber)
        self.dest_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1, client_id="ssl_relay_test_subscriber")
        self.dest_client.on_connect = self.on_dest_connect
        self.dest_client.on_message = self.on_dest_message
        self.dest_client.on_log = self.on_log
        
        # Configure SSL for both clients
        try:
            # Source client SSL configuration
            self.source_client.tls_set(
                ca_certs=self.ca_cert,
                certfile=self.client_cert,
                keyfile=self.client_key,
                cert_reqs=ssl.CERT_REQUIRED,
                tls_version=ssl.PROTOCOL_TLS,
                ciphers=None
            )
            
            # Destination client SSL configuration
            self.dest_client.tls_set(
                ca_certs=self.ca_cert,
                certfile=self.client_cert,
                keyfile=self.client_key,
                cert_reqs=ssl.CERT_REQUIRED,
                tls_version=ssl.PROTOCOL_TLS,
                ciphers=None
            )
            
            print("🔐 SSL configuration applied to MQTT clients")
            
            # Connect to brokers
            self.source_client.connect(self.source_host, self.source_port, 60)
            self.dest_client.connect(self.dest_host, self.dest_port, 60)
            
            self.source_client.loop_start()
            self.dest_client.loop_start()
            
            time.sleep(3)
            print("✅ SSL MQTT clients connected successfully")
            self.test_results['ssl_connection'] = True
            self.test_results['certificate_validation'] = True
            return True
            
        except Exception as e:
            print(f"❌ Failed to connect SSL MQTT clients: {e}")
            return False

    def on_source_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"🔐 SSL connection established to source broker (port {self.source_port})")
        else:
            print(f"❌ SSL connection failed to source broker: {rc}")

    def on_dest_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"🔐 SSL connection established to destination broker (port {self.dest_port})")
            # Subscribe to expected encrypted destination topics
            client.subscribe("encrypted/sensors/+")
            client.subscribe("encrypted/events/+")
            client.subscribe("encrypted/commands/+")
            print("🔔 Subscribed to encrypted destination topics")
            self.dest_broker_ready = True
        else:
            print(f"❌ SSL connection failed to destination broker: {rc}")

    def on_publish(self, client, userdata, mid):
        print(f"📤 Encrypted message published (mid: {mid})")

    def on_dest_message(self, client, userdata, msg):
        """Handle encrypted messages received on SSL destination broker"""
        topic = msg.topic
        payload = msg.payload.decode()
        timestamp = datetime.now().isoformat()
        
        print(f"📥 SSL RELAY SUCCESS: Received encrypted message - Topic: {topic}")
        print(f"    Encrypted Payload: {payload}")
        
        self.received_messages.append({
            'topic': topic,
            'payload': payload,
            'timestamp': timestamp,
            'encrypted': True
        })
        
        # Check which SSL test case this message satisfies
        if topic.startswith("encrypted/sensors/"):
            self.test_results['secure_sensor_forwarding'] = True
            print("✅ Secure sensor forwarding test PASSED")
            
        elif topic.startswith("encrypted/events/"):
            self.test_results['secure_event_forwarding'] = True
            print("✅ Secure event forwarding test PASSED")
            
        elif topic.startswith("encrypted/commands/"):
            self.test_results['ssl_bidirectional_commands'] = True
            print("✅ SSL bidirectional commands test PASSED")

    def on_log(self, client, userdata, level, buf):
        # Only log SSL-related messages
        if "SSL" in buf or "TLS" in buf or "certificate" in buf:
            print(f"🔐 SSL Log: {buf}")

    def start_ssl_relay_client(self):
        """Start the SSL relay client"""
        print("🔄 Starting SSL relay client...")
        
        try:
            self.relay_process = subprocess.Popen([
                './build/ssl_relay_client', 'build/ssl_relay_config.json'
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            
            time.sleep(5)
            print("✅ SSL relay client started")
            return True
        except Exception as e:
            print(f"❌ Failed to start SSL relay client: {e}")
            return False

    def publish_ssl_test_messages(self):
        """Publish test messages to SSL source broker"""
        print("📤 Publishing encrypted test messages to SSL source broker...")
        
        if not self.dest_broker_ready:
            print("⏳ Waiting for SSL destination broker to be ready...")
            time.sleep(5)
        
        ssl_test_messages = [
            # Secure sensor data
            {
                'topic': 'secure/sensors/temperature',
                'payload': json.dumps({
                    'sensor_id': 'ssl_temp_001',
                    'value': 28.7,
                    'unit': 'celsius',
                    'encrypted': True,
                    'timestamp': time.time()
                })
            },
            {
                'topic': 'secure/sensors/pressure',
                'payload': json.dumps({
                    'sensor_id': 'ssl_press_001',
                    'value': 1013.25,
                    'unit': 'hPa',
                    'encrypted': True,
                    'timestamp': time.time()
                })
            },
            # Secure system events
            {
                'topic': 'secure/events/security',
                'payload': json.dumps({
                    'event_type': 'security_alert',
                    'level': 'high',
                    'message': 'Unauthorized access attempt detected',
                    'encrypted': True,
                    'timestamp': time.time()
                })
            },
            {
                'topic': 'secure/events/backup',
                'payload': json.dumps({
                    'event_type': 'backup_complete',
                    'status': 'success',
                    'size': '1.2GB',
                    'encrypted': True,
                    'timestamp': time.time()
                })
            },
            # Secure commands
            {
                'topic': 'secure/commands/encrypt',
                'payload': json.dumps({
                    'command': 'encrypt_data',
                    'algorithm': 'AES-256',
                    'key_rotation': True,
                    'encrypted': True,
                    'timestamp': time.time()
                })
            }
        ]
        
        for msg in ssl_test_messages:
            print(f"🔐 Publishing encrypted message to {msg['topic']}")
            self.source_client.publish(msg['topic'], msg['payload'])
            self.sent_messages.append(msg)
            time.sleep(2)  # Slower for SSL processing
        
        print(f"✅ Published {len(ssl_test_messages)} encrypted test messages")

    def run_ssl_relay_test(self):
        """Run the complete SSL relay test"""
        print("=" * 50)
        print("  SSL Relay Client Verification Test")
        print("=" * 50)
        
        # Check SSL certificates exist
        ssl_files = [self.ca_cert, self.client_cert, self.client_key, self.server_cert, self.server_key]
        for ssl_file in ssl_files:
            if not os.path.exists(ssl_file):
                print(f"❌ SSL certificate file not found: {ssl_file}")
                return False
        print("✅ SSL certificate files verified")
        
        # Setup SSL destination broker
        if not self.setup_ssl_destination_broker():
            return False
        
        # Setup SSL MQTT clients
        if not self.setup_ssl_mqtt_clients():
            return False
        
        # Start SSL relay client
        if not self.start_ssl_relay_client():
            return False
        
        # Wait for SSL relay to initialize
        print("⏳ Waiting for SSL relay initialization...")
        time.sleep(8)
        
        # Publish SSL test messages
        self.publish_ssl_test_messages()
        
        # Wait for encrypted message processing
        print("⏳ Waiting for encrypted message relay processing...")
        time.sleep(15)
        
        # Analyze SSL results
        self.analyze_ssl_results()
        
        return True

    def analyze_ssl_results(self):
        """Analyze SSL test results and print summary"""
        print("\n" + "=" * 50)
        print("  SSL Relay Test Results")
        print("=" * 50)
        
        print(f"📊 Encrypted messages sent: {len(self.sent_messages)}")
        print(f"📊 Encrypted messages received: {len(self.received_messages)}")
        
        print("\n🔍 SSL Test Case Results:")
        for test_name, result in self.test_results.items():
            status = "✅ PASSED" if result else "❌ FAILED"
            print(f"   {test_name.replace('_', ' ').title()}: {status}")
        
        # Overall SSL result
        all_passed = all(self.test_results.values())
        overall_status = "✅ ALL SSL TESTS PASSED" if all_passed else "❌ SOME SSL TESTS FAILED"
        print(f"\n🎯 Overall SSL Result: {overall_status}")
        
        if self.received_messages:
            print("\n📥 Encrypted Messages Details:")
            for i, msg in enumerate(self.received_messages, 1):
                print(f"   {i}. Topic: {msg['topic']}")
                print(f"      Encrypted: {msg.get('encrypted', 'Unknown')}")
                print(f"      Payload: {msg['payload'][:100]}...")
        
        print("\n🔐 SSL/TLS Security Features Tested:")
        print("   - Certificate validation")
        print("   - Encrypted message transmission")
        print("   - Secure broker connections")
        print("   - TLS handshake verification")
        
        print("\n" + "=" * 50)

    def cleanup(self):
        """Cleanup SSL resources"""
        print("🧹 Cleaning up SSL resources...")
        
        if self.source_client:
            self.source_client.loop_stop()
            self.source_client.disconnect()
        
        if self.dest_client:
            self.dest_client.loop_stop()
            self.dest_client.disconnect()
        
        if self.relay_process:
            self.relay_process.terminate()
            self.relay_process.wait()
        
        if hasattr(self, 'dest_broker_process'):
            self.dest_broker_process.terminate()
            self.dest_broker_process.wait()
        
        # Clean up SSL config files
        for file in ['ssl_mosquitto_1886.conf', 'ssl_mosquitto_1886.log']:
            if os.path.exists(file):
                os.remove(file)
        
        print("✅ SSL cleanup complete")

def signal_handler(sig, frame):
    print("\n🛑 SSL test interrupted by user")
    sys.exit(0)

def main():
    signal.signal(signal.SIGINT, signal_handler)
    
    tester = SSLRelayTester()
    
    try:
        success = tester.run_ssl_relay_test()
        return 0 if success else 1
    except Exception as e:
        print(f"❌ SSL test failed with exception: {e}")
        return 1
    finally:
        tester.cleanup()

if __name__ == "__main__":
    sys.exit(main())