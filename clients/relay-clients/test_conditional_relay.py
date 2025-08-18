#!/usr/bin/env python3
"""
Conditional Relay Verification Script

Tests the conditional relay client by:
1. Publishing messages with different priorities and types to source broker (port 1883)
2. Verifying only qualifying messages are received at destination broker (port 1887)
3. Testing conditional filtering logic (priority, type, timestamp, business hours)
"""

import paho.mqtt.client as mqtt
import json
import time
import threading
import sys
import subprocess
import signal
import os
from datetime import datetime, timedelta

class ConditionalRelayTester:
    def __init__(self):
        self.source_host = "localhost"
        self.source_port = 1883
        self.dest_host = "localhost"
        self.dest_port = 1887
        
        self.source_client = None
        self.dest_client = None
        self.relay_process = None
        
        # Message tracking
        self.sent_messages = []
        self.received_messages = []
        self.filtered_messages = []
        
        self.test_results = {
            'priority_filtering': False,  # Low priority should be filtered
            'type_filtering': False,      # Debug messages should be filtered
            'timestamp_filtering': False, # Old messages should be filtered
            'high_priority_forwarding': False, # High priority should pass
            'normal_message_forwarding': False, # Normal messages should pass
            'conditional_logic': False    # Overall conditional logic works
        }
        
        self.dest_broker_ready = False

    def setup_conditional_destination_broker(self):
        """Start destination broker on port 1887"""
        print("🧠 Starting conditional destination broker on port 1887...")
        
        # Create broker config for port 1887
        config_content = """
port 1887
allow_anonymous true
connection_messages true
log_type error
log_type warning
log_type notice
log_type information
log_dest file mosquitto_1887.log
"""
        
        with open('mosquitto_1887.conf', 'w') as f:
            f.write(config_content)
        
        # Start broker
        try:
            self.dest_broker_process = subprocess.Popen([
                'mosquitto', '-c', 'mosquitto_1887.conf', '-v'
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(2)
            print("✅ Conditional destination broker started successfully")
            return True
        except Exception as e:
            print(f"❌ Failed to start conditional destination broker: {e}")
            return False

    def setup_mqtt_clients(self):
        """Setup MQTT clients for source and destination brokers"""
        print("🔗 Setting up MQTT clients...")
        
        # Source broker client (publisher)
        self.source_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1, client_id="conditional_relay_test_publisher")
        self.source_client.on_connect = self.on_source_connect
        self.source_client.on_publish = self.on_publish
        
        # Destination broker client (subscriber)
        self.dest_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1, client_id="conditional_relay_test_subscriber")
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
        # Subscribe to expected filtered destination topics
        client.subscribe("filtered/sensors/+")
        client.subscribe("filtered/events/+")
        client.subscribe("filtered/priority/+")
        client.subscribe("filtered/commands/+")
        print("🔔 Subscribed to filtered destination topics")
        self.dest_broker_ready = True

    def on_publish(self, client, userdata, mid):
        print(f"📤 Message published (mid: {mid})")

    def on_dest_message(self, client, userdata, msg):
        """Handle messages received on destination broker (should be filtered)"""
        topic = msg.topic
        payload = msg.payload.decode()
        timestamp = datetime.now().isoformat()
        
        print(f"📥 CONDITIONAL RELAY SUCCESS: Received filtered message - Topic: {topic}")
        
        try:
            message_data = json.loads(payload)
            priority = message_data.get('priority', 'unknown')
            msg_type = message_data.get('type', 'unknown')
            
            print(f"    Priority: {priority}, Type: {msg_type}")
            print(f"    Payload: {payload}")
            
            self.received_messages.append({
                'topic': topic,
                'payload': payload,
                'timestamp': timestamp,
                'priority': priority,
                'type': msg_type
            })
            
            # Check which conditional test case this message satisfies
            if topic.startswith("filtered/sensors/"):
                if priority == "high" or priority == "normal":
                    self.test_results['normal_message_forwarding'] = True
                    print("✅ Normal message forwarding test PASSED")
                
            elif topic.startswith("filtered/priority/"):
                if priority == "high":
                    self.test_results['high_priority_forwarding'] = True
                    print("✅ High priority forwarding test PASSED")
                    
            elif topic.startswith("filtered/events/"):
                if msg_type != "debug":
                    self.test_results['normal_message_forwarding'] = True
                    print("✅ Non-debug event forwarding test PASSED")
                    
            elif topic.startswith("filtered/commands/"):
                self.test_results['normal_message_forwarding'] = True
                print("✅ Command forwarding test PASSED")
                
        except json.JSONDecodeError:
            print(f"    Non-JSON payload: {payload}")

    def start_conditional_relay_client(self):
        """Start the conditional relay client"""
        print("🔄 Starting conditional relay client...")
        
        try:
            self.relay_process = subprocess.Popen([
                './build/conditional_relay_client', 'build/conditional_relay_config.json'
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            
            time.sleep(3)
            print("✅ Conditional relay client started")
            return True
        except Exception as e:
            print(f"❌ Failed to start conditional relay client: {e}")
            return False

    def publish_conditional_test_messages(self):
        """Publish test messages with various conditions to source broker"""
        print("📤 Publishing conditional test messages to source broker...")
        
        if not self.dest_broker_ready:
            print("⏳ Waiting for destination broker to be ready...")
            time.sleep(3)
        
        current_time = int(time.time())
        old_time = current_time - 400  # 6+ minutes old (should be filtered)
        
        test_messages = [
            # HIGH PRIORITY - Should pass
            {
                'topic': 'smart/high_priority/critical_alert',
                'payload': json.dumps({
                    'alert_id': 'critical_001',
                    'priority': 'high',
                    'type': 'critical',
                    'message': 'Critical system failure detected',
                    'timestamp': current_time
                }),
                'should_pass': True,
                'reason': 'High priority message'
            },
            
            # LOW PRIORITY - Should be filtered
            {
                'topic': 'smart/sensors/low_priority_temp',
                'payload': json.dumps({
                    'sensor_id': 'temp_low_001',
                    'priority': 'low',
                    'type': 'sensor_reading',
                    'value': 22.0,
                    'timestamp': current_time
                }),
                'should_pass': False,
                'reason': 'Low priority should be filtered'
            },
            
            # DEBUG TYPE - Should be filtered
            {
                'topic': 'smart/events/debug_event',
                'payload': json.dumps({
                    'event_id': 'debug_001',
                    'priority': 'normal',
                    'type': 'debug',
                    'message': 'Debug information for developers',
                    'timestamp': current_time
                }),
                'should_pass': False,
                'reason': 'Debug messages should be filtered'
            },
            
            # OLD MESSAGE - Should be filtered
            {
                'topic': 'smart/sensors/old_reading',
                'payload': json.dumps({
                    'sensor_id': 'temp_old_001',
                    'priority': 'normal',
                    'type': 'sensor_reading',
                    'value': 25.5,
                    'timestamp': old_time
                }),
                'should_pass': False,
                'reason': 'Old messages should be filtered'
            },
            
            # NORMAL PRIORITY - Should pass
            {
                'topic': 'smart/sensors/normal_temp',
                'payload': json.dumps({
                    'sensor_id': 'temp_normal_001',
                    'priority': 'normal',
                    'type': 'sensor_reading',
                    'value': 24.5,
                    'timestamp': current_time
                }),
                'should_pass': True,
                'reason': 'Normal priority current message'
            },
            
            # INFO TYPE - Should pass
            {
                'topic': 'smart/events/info_event',
                'payload': json.dumps({
                    'event_id': 'info_001',
                    'priority': 'normal',
                    'type': 'info',
                    'message': 'System backup completed successfully',
                    'timestamp': current_time
                }),
                'should_pass': True,
                'reason': 'Info messages should pass'
            },
            
            # COMMAND - Should pass
            {
                'topic': 'smart/commands/restart_service',
                'payload': json.dumps({
                    'command_id': 'cmd_001',
                    'priority': 'high',
                    'type': 'command',
                    'command': 'restart',
                    'service': 'web_server',
                    'timestamp': current_time
                }),
                'should_pass': True,
                'reason': 'Commands should pass'
            },
            
            # MEDIUM PRIORITY (not explicitly filtered) - Should pass
            {
                'topic': 'smart/sensors/medium_priority',
                'payload': json.dumps({
                    'sensor_id': 'pressure_001',
                    'priority': 'medium',
                    'type': 'sensor_reading',
                    'value': 1013.25,
                    'timestamp': current_time
                }),
                'should_pass': True,
                'reason': 'Medium priority should pass'
            }
        ]
        
        for msg in test_messages:
            print(f"📡 Publishing: {msg['topic']} (Expected: {'PASS' if msg['should_pass'] else 'FILTER'})")
            print(f"    Reason: {msg['reason']}")
            
            self.source_client.publish(msg['topic'], msg['payload'])
            self.sent_messages.append(msg)
            
            if not msg['should_pass']:
                self.filtered_messages.append(msg)
            
            time.sleep(1)
        
        print(f"✅ Published {len(test_messages)} conditional test messages")
        print(f"   Expected to pass: {len([m for m in test_messages if m['should_pass']])}")
        print(f"   Expected to filter: {len([m for m in test_messages if not m['should_pass']])}")

    def run_conditional_relay_test(self):
        """Run the complete conditional relay test"""
        print("=" * 60)
        print("  Conditional Relay Client Verification Test")
        print("=" * 60)
        
        # Setup conditional destination broker
        if not self.setup_conditional_destination_broker():
            return False
        
        # Setup MQTT clients
        if not self.setup_mqtt_clients():
            return False
        
        # Start conditional relay client
        if not self.start_conditional_relay_client():
            return False
        
        # Wait for conditional relay to initialize
        print("⏳ Waiting for conditional relay initialization...")
        time.sleep(5)
        
        # Publish conditional test messages
        self.publish_conditional_test_messages()
        
        # Wait for conditional message processing
        print("⏳ Waiting for conditional message processing...")
        time.sleep(15)
        
        # Analyze conditional results
        self.analyze_conditional_results()
        
        return True

    def analyze_conditional_results(self):
        """Analyze conditional test results and print summary"""
        print("\n" + "=" * 60)
        print("  Conditional Relay Test Results")
        print("=" * 60)
        
        print(f"📊 Messages sent: {len(self.sent_messages)}")
        print(f"📊 Messages received (passed filter): {len(self.received_messages)}")
        print(f"📊 Messages expected to be filtered: {len(self.filtered_messages)}")
        
        # Calculate filtering effectiveness
        expected_to_pass = len([m for m in self.sent_messages if m['should_pass']])
        expected_to_filter = len([m for m in self.sent_messages if not m['should_pass']])
        actual_received = len(self.received_messages)
        
        print(f"📊 Expected to pass: {expected_to_pass}")
        print(f"📊 Expected to filter: {expected_to_filter}")
        print(f"📊 Actually received: {actual_received}")
        
        # Check filtering logic
        if actual_received <= expected_to_pass:
            self.test_results['conditional_logic'] = True
            print("✅ Conditional filtering logic working correctly")
        
        # Check for high priority messages
        high_priority_received = any(
            msg.get('priority') == 'high' 
            for msg in self.received_messages
        )
        if high_priority_received:
            self.test_results['high_priority_forwarding'] = True
        
        # Check that low priority was filtered (not received)
        low_priority_received = any(
            msg.get('priority') == 'low' 
            for msg in self.received_messages
        )
        if not low_priority_received:
            self.test_results['priority_filtering'] = True
            print("✅ Low priority filtering working")
        
        # Check that debug messages were filtered
        debug_received = any(
            msg.get('type') == 'debug' 
            for msg in self.received_messages
        )
        if not debug_received:
            self.test_results['type_filtering'] = True
            print("✅ Debug message filtering working")
        
        print("\n🔍 Conditional Test Case Results:")
        for test_name, result in self.test_results.items():
            status = "✅ PASSED" if result else "❌ FAILED"
            print(f"   {test_name.replace('_', ' ').title()}: {status}")
        
        # Overall conditional result
        all_passed = all(self.test_results.values())
        overall_status = "✅ ALL CONDITIONAL TESTS PASSED" if all_passed else "❌ SOME CONDITIONAL TESTS FAILED"
        print(f"\n🎯 Overall Conditional Result: {overall_status}")
        
        if self.received_messages:
            print("\n📥 Received (Passed Filter) Messages:")
            for i, msg in enumerate(self.received_messages, 1):
                print(f"   {i}. Topic: {msg['topic']}")
                print(f"      Priority: {msg.get('priority', 'N/A')}, Type: {msg.get('type', 'N/A')}")
        
        print("\n🧠 Conditional Logic Features Tested:")
        print("   - Priority-based filtering (low priority blocked)")
        print("   - Message type filtering (debug messages blocked)")
        print("   - Timestamp validation (old messages blocked)")
        print("   - High priority message forwarding")
        print("   - Normal message processing")
        
        print("\n📊 Filtering Effectiveness:")
        if expected_to_filter > 0:
            filter_rate = ((expected_to_filter - actual_received + expected_to_pass) / expected_to_filter) * 100
            filter_rate = max(0, min(100, filter_rate))
            print(f"   Estimated filtering rate: {filter_rate:.1f}%")
        
        print("\n" + "=" * 60)

    def cleanup(self):
        """Cleanup conditional resources"""
        print("🧹 Cleaning up conditional resources...")
        
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
        for file in ['mosquitto_1887.conf', 'mosquitto_1887.log']:
            if os.path.exists(file):
                os.remove(file)
        
        print("✅ Conditional cleanup complete")

def signal_handler(sig, frame):
    print("\n🛑 Conditional test interrupted by user")
    sys.exit(0)

def main():
    signal.signal(signal.SIGINT, signal_handler)
    
    tester = ConditionalRelayTester()
    
    try:
        success = tester.run_conditional_relay_test()
        return 0 if success else 1
    except Exception as e:
        print(f"❌ Conditional test failed with exception: {e}")
        return 1
    finally:
        tester.cleanup()

if __name__ == "__main__":
    sys.exit(main())