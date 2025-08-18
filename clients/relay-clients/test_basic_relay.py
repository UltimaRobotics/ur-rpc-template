#!/usr/bin/env python3
"""
Basic Relay Verification Script

Tests the basic relay client by:
1. Publishing messages to source broker (port 1883)
2. Verifying messages are received at destination broker (port 1885)
3. Testing bidirectional relay functionality
"""

import paho.mqtt.client as mqtt
import json
import time
import threading
import sys
import subprocess
import signal
import os
from datetime import datetime

class BasicRelayTester:
    def __init__(self):
        self.source_host = "localhost"
        self.source_port = 1883
        self.dest_host = "localhost"
        self.dest_port = 1885
        
        self.source_client = None
        self.dest_client = None
        self.relay_process = None
        
        # Message tracking
        self.sent_messages = []
        self.received_messages = []
        self.test_results = {
            'sensor_forwarding': False,
            'event_forwarding': False,
            'bidirectional_commands': False,
            'prefix_handling': False
        }
        
        self.dest_broker_ready = False
        self.test_complete = False

    def setup_destination_broker(self):
        """Start destination broker on port 1885"""
        print("🚀 Starting destination broker on port 1885...")
        
        # Create broker config for port 1885
        config_content = """
port 1885
allow_anonymous true
connection_messages true
log_type error
log_type warning
log_type notice
log_type information
log_dest file mosquitto_1885.log
"""
        
        with open('mosquitto_1885.conf', 'w') as f:
            f.write(config_content)
        
        # Start broker
        try:
            self.dest_broker_process = subprocess.Popen([
                'mosquitto', '-c', 'mosquitto_1885.conf', '-v'
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(2)
            print("✅ Destination broker started successfully")
            return True
        except Exception as e:
            print(f"❌ Failed to start destination broker: {e}")
            return False

    def setup_mqtt_clients(self):
        """Setup MQTT clients for source and destination brokers"""
        print("🔗 Setting up MQTT clients...")
        
        # Source broker client (publisher)
        self.source_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1, client_id="relay_test_publisher")
        self.source_client.on_connect = self.on_source_connect
        self.source_client.on_publish = self.on_publish
        
        # Destination broker client (subscriber)
        self.dest_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1, client_id="relay_test_subscriber")
        self.dest_client.on_connect = self.on_dest_connect
        self.dest_client.on_message = self.on_dest_message
        
        try:
            self.source_client.connect(self.source_host, self.source_port, 60)
            self.dest_client.connect(self.dest_host, self.dest_port, 60)
            
            self.source_client.loop_start()
            self.dest_client.loop_start()
            
            time.sleep(2)
            print("✅ MQTT clients connected successfully")
            return True
        except Exception as e:
            print(f"❌ Failed to connect MQTT clients: {e}")
            return False

    def on_source_connect(self, client, userdata, flags, rc):
        print(f"📡 Connected to source broker (port {self.source_port})")

    def on_dest_connect(self, client, userdata, flags, rc):
        print(f"📡 Connected to destination broker (port {self.dest_port})")
        # Subscribe to expected destination topics
        client.subscribe("forwarded/sensors/+")
        client.subscribe("forwarded/system/+")
        client.subscribe("relayed/commands/+")
        print("🔔 Subscribed to destination topics")
        self.dest_broker_ready = True

    def on_publish(self, client, userdata, mid):
        print(f"📤 Message published (mid: {mid})")

    def on_dest_message(self, client, userdata, msg):
        """Handle messages received on destination broker"""
        topic = msg.topic
        payload = msg.payload.decode()
        timestamp = datetime.now().isoformat()
        
        print(f"📥 RELAY SUCCESS: Received on destination - Topic: {topic}")
        print(f"    Payload: {payload}")
        
        self.received_messages.append({
            'topic': topic,
            'payload': payload,
            'timestamp': timestamp
        })
        
        # Check which test case this message satisfies
        if topic.startswith("forwarded/sensors/"):
            self.test_results['sensor_forwarding'] = True
            print("✅ Sensor forwarding test PASSED")
            
        elif topic.startswith("forwarded/system/"):
            self.test_results['event_forwarding'] = True
            print("✅ Event forwarding test PASSED")
            
        elif topic.startswith("relayed/commands/"):
            self.test_results['bidirectional_commands'] = True
            print("✅ Bidirectional commands test PASSED")

    def start_relay_client(self):
        """Start the basic relay client"""
        print("🔄 Starting basic relay client...")
        
        try:
            self.relay_process = subprocess.Popen([
                './build/basic_relay_client', 'build/basic_relay_config.json'
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            
            time.sleep(3)
            print("✅ Basic relay client started")
            return True
        except Exception as e:
            print(f"❌ Failed to start relay client: {e}")
            return False

    def publish_test_messages(self):
        """Publish test messages to source broker"""
        print("📤 Publishing test messages to source broker...")
        
        if not self.dest_broker_ready:
            print("⏳ Waiting for destination broker to be ready...")
            time.sleep(3)
        
        test_messages = [
            # Sensor data (should be forwarded to forwarded/sensors/+)
            {
                'topic': 'data/sensors/temperature',
                'payload': json.dumps({
                    'sensor_id': 'temp_001',
                    'value': 25.5,
                    'unit': 'celsius',
                    'timestamp': time.time()
                })
            },
            {
                'topic': 'data/sensors/humidity',
                'payload': json.dumps({
                    'sensor_id': 'hum_001', 
                    'value': 65.0,
                    'unit': 'percent',
                    'timestamp': time.time()
                })
            },
            # System events (should be forwarded to forwarded/system/+)
            {
                'topic': 'events/system/startup',
                'payload': json.dumps({
                    'event_type': 'system_startup',
                    'message': 'System started successfully',
                    'timestamp': time.time()
                })
            },
            {
                'topic': 'events/system/alert',
                'payload': json.dumps({
                    'event_type': 'system_alert',
                    'level': 'warning',
                    'message': 'High CPU usage detected',
                    'timestamp': time.time()
                })
            },
            # Commands (should be relayed bidirectionally)
            {
                'topic': 'commands/restart',
                'payload': json.dumps({
                    'command': 'restart_service',
                    'service': 'web_server',
                    'timestamp': time.time()
                })
            }
        ]
        
        for msg in test_messages:
            print(f"📡 Publishing to {msg['topic']}")
            self.source_client.publish(msg['topic'], msg['payload'])
            self.sent_messages.append(msg)
            time.sleep(1)
        
        print(f"✅ Published {len(test_messages)} test messages")

    def run_relay_test(self):
        """Run the complete relay test"""
        print("=" * 50)
        print("  Basic Relay Client Verification Test")
        print("=" * 50)
        
        # Setup destination broker
        if not self.setup_destination_broker():
            return False
        
        # Setup MQTT clients
        if not self.setup_mqtt_clients():
            return False
        
        # Start relay client
        if not self.start_relay_client():
            return False
        
        # Wait for relay to initialize
        print("⏳ Waiting for relay initialization...")
        time.sleep(5)
        
        # Publish test messages
        self.publish_test_messages()
        
        # Wait for message processing
        print("⏳ Waiting for message relay processing...")
        time.sleep(10)
        
        # Analyze results
        self.analyze_results()
        
        return True

    def analyze_results(self):
        """Analyze test results and print summary"""
        print("\n" + "=" * 50)
        print("  Basic Relay Test Results")
        print("=" * 50)
        
        print(f"📊 Messages sent: {len(self.sent_messages)}")
        print(f"📊 Messages received: {len(self.received_messages)}")
        
        print("\n🔍 Test Case Results:")
        for test_name, result in self.test_results.items():
            status = "✅ PASSED" if result else "❌ FAILED"
            print(f"   {test_name.replace('_', ' ').title()}: {status}")
        
        # Overall result
        all_passed = all(self.test_results.values())
        overall_status = "✅ ALL TESTS PASSED" if all_passed else "❌ SOME TESTS FAILED"
        print(f"\n🎯 Overall Result: {overall_status}")
        
        if self.received_messages:
            print("\n📥 Received Messages Details:")
            for i, msg in enumerate(self.received_messages, 1):
                print(f"   {i}. Topic: {msg['topic']}")
                print(f"      Payload: {msg['payload'][:100]}...")
        
        print("\n" + "=" * 50)

    def cleanup(self):
        """Cleanup resources"""
        print("🧹 Cleaning up...")
        
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
        
        # Clean up config files
        for file in ['mosquitto_1885.conf', 'mosquitto_1885.log']:
            if os.path.exists(file):
                os.remove(file)
        
        print("✅ Cleanup complete")

def signal_handler(sig, frame):
    print("\n🛑 Test interrupted by user")
    sys.exit(0)

def main():
    signal.signal(signal.SIGINT, signal_handler)
    
    tester = BasicRelayTester()
    
    try:
        success = tester.run_relay_test()
        return 0 if success else 1
    except Exception as e:
        print(f"❌ Test failed with exception: {e}")
        return 1
    finally:
        tester.cleanup()

if __name__ == "__main__":
    sys.exit(main())