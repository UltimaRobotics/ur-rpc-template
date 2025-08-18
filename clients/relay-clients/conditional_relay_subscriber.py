#!/usr/bin/env python3
"""
Conditional Relay Subscriber Test Script

Subscribes to destination broker (port 1887) to verify that the conditional_relay_client
C binary correctly applies filtering logic and forwards only qualifying messages.

Usage: python3 conditional_relay_subscriber.py
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import argparse
from datetime import datetime

class ConditionalRelaySubscriber:
    def __init__(self, host="localhost", port=1887, verbose=False):
        self.host = host
        self.port = port
        self.verbose = verbose
        self.client = None
        self.connected = False
        self.messages_received = 0
        self.test_results = {}
        self.start_time = None
        self.filtering_stats = {
            'high_priority_received': 0,
            'low_priority_received': 0,
            'debug_type_received': 0,
            'old_messages_received': 0,
            'normal_messages_received': 0,
            'unexpected_messages': 0
        }
        
    def setup_client(self):
        """Setup MQTT client for subscribing to destination broker"""
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION1,
            client_id="conditional_relay_subscriber"
        )
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        if self.verbose:
            self.client.on_log = self.on_log
            
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"✅ Connected to destination broker at {self.host}:{self.port}")
            
            # Subscribe to expected filtered topics based on conditional_relay_config.json
            subscription_topics = [
                "filtered/sensors/+",       # smart/sensors/+ → filtered/sensors/+
                "filtered/events/+",        # smart/events/+ → filtered/events/+
                "filtered/priority/+",      # smart/high_priority/+ → filtered/priority/+
                "filtered/commands/+"       # smart/commands/+ → filtered/commands/+
            ]
            
            print("🔔 Subscribing to filtered destination topics:")
            for topic in subscription_topics:
                result = client.subscribe(topic, qos=1)
                if result[0] == mqtt.MQTT_ERR_SUCCESS:
                    print(f"   🧠 {topic}")
                else:
                    print(f"   ❌ {topic} (failed)")
                    
            print()
            self.start_time = datetime.now()
            print("🎧 Listening for filtered messages...")
            print("=" * 70)
        else:
            print(f"❌ Failed to connect to destination broker: {rc}")
            
    def on_message(self, client, userdata, msg):
        """Handle filtered messages received from the conditional relay"""
        self.messages_received += 1
        topic = msg.topic
        
        try:
            payload = msg.payload.decode('utf-8')
            timestamp = datetime.now().isoformat()
            
            print(f"🧠 FILTERED MESSAGE RECEIVED #{self.messages_received}")
            print(f"   Topic: {topic}")
            print(f"   Time: {timestamp}")
            
            if self.verbose:
                print(f"   QoS: {msg.qos}")
                print(f"   Retain: {msg.retain}")
                
            # Try to parse JSON payload
            try:
                data = json.loads(payload)
                test_id = data.get('test_id', 'unknown')
                priority = data.get('priority', 'unknown')
                msg_type = data.get('type', 'unknown')
                msg_timestamp = data.get('timestamp', 0)
                
                print(f"   Test ID: {test_id}")
                print(f"   Priority: {priority}")
                print(f"   Type: {msg_type}")
                
                # Extract useful fields
                if 'sensor_id' in data:
                    print(f"   Sensor: {data['sensor_id']} = {data.get('value', 'N/A')} {data.get('unit', '')}")
                elif 'event_type' in data:
                    print(f"   Event: {data['event_type']} - {data.get('message', 'N/A')}")
                elif 'command' in data:
                    print(f"   Command: {data['command']} on {data.get('service', 'N/A')}")
                    
                # Track test results
                self.test_results[test_id] = {
                    'topic': topic,
                    'timestamp': timestamp,
                    'data': data,
                    'priority': priority,
                    'type': msg_type,
                    'message_timestamp': msg_timestamp
                }
                
                # Update filtering statistics
                self.update_filtering_stats(data)
                
            except json.JSONDecodeError:
                print(f"   Payload (raw): {payload}")
                
            # Analyze conditional relay correctness
            self.analyze_conditional_relay_correctness(topic, payload)
            
        except Exception as e:
            print(f"   ❌ Error processing filtered message: {e}")
            
        print("   " + "-" * 66)
        print()
        
    def update_filtering_stats(self, data):
        """Update filtering statistics based on received message"""
        priority = data.get('priority', '').lower()
        msg_type = data.get('type', '').lower()
        msg_timestamp = data.get('timestamp', 0)
        current_time = int(time.time())
        
        if priority == 'high':
            self.filtering_stats['high_priority_received'] += 1
        elif priority == 'low':
            self.filtering_stats['low_priority_received'] += 1
            
        if msg_type == 'debug':
            self.filtering_stats['debug_type_received'] += 1
            
        # Check if message is old (>5 minutes)
        if current_time - msg_timestamp > 300:
            self.filtering_stats['old_messages_received'] += 1
        else:
            self.filtering_stats['normal_messages_received'] += 1
        
    def analyze_conditional_relay_correctness(self, topic, payload):
        """Analyze if the conditional relay filtering is working correctly"""
        relay_status = "🧠 FILTERED CORRECTLY"
        analysis = []
        warnings = []
        
        # Check topic forwarding rules from conditional_relay_config.json
        if topic.startswith("filtered/sensors/"):
            expected_source = topic.replace("filtered/sensors/", "smart/sensors/")
            analysis.append(f"Sensor data relay: {expected_source} → {topic}")
            
        elif topic.startswith("filtered/events/"):
            expected_source = topic.replace("filtered/events/", "smart/events/")
            analysis.append(f"Event relay: {expected_source} → {topic}")
            
        elif topic.startswith("filtered/priority/"):
            expected_source = topic.replace("filtered/priority/", "smart/high_priority/")
            analysis.append(f"Priority relay: {expected_source} → {topic}")
            
        elif topic.startswith("filtered/commands/"):
            expected_source = topic.replace("filtered/commands/", "smart/commands/")
            analysis.append(f"Command relay: {expected_source} → {topic}")
            
        else:
            relay_status = "❓ UNEXPECTED"
            analysis.append(f"Unexpected filtered topic pattern: {topic}")
            self.filtering_stats['unexpected_messages'] += 1
            
        # Analyze payload for filtering violations
        try:
            data = json.loads(payload)
            priority = data.get('priority', '').lower()
            msg_type = data.get('type', '').lower()
            msg_timestamp = data.get('timestamp', 0)
            current_time = int(time.time())
            
            # Check for filtering violations
            if priority == 'low':
                warnings.append("⚠️  Low priority message passed filter (should be blocked)")
                relay_status = "❌ FILTER FAILED"
                
            if msg_type == 'debug':
                warnings.append("⚠️  Debug message passed filter (should be blocked)")
                relay_status = "❌ FILTER FAILED"
                
            if current_time - msg_timestamp > 300:
                warnings.append("⚠️  Old message passed filter (should be blocked)")
                relay_status = "❌ FILTER FAILED"
                
            if not priority or priority == 'unknown':
                warnings.append("⚠️  Message without priority passed filter (should be blocked)")
                relay_status = "❌ FILTER FAILED"
                
        except json.JSONDecodeError:
            warnings.append("⚠️  Non-JSON message received")
            
        print(f"   Relay Status: {relay_status}")
        for note in analysis:
            print(f"   Analysis: {note}")
        for warning in warnings:
            print(f"   {warning}")
            
    def on_disconnect(self, client, userdata, rc):
        self.connected = False
        print(f"📡 Disconnected from destination broker")
        
    def on_log(self, client, userdata, level, buf):
        print(f"🔍 Log: {buf}")
        
    def connect(self):
        """Connect to the destination broker"""
        try:
            self.client.connect(self.host, self.port, 60)
            self.client.loop_start()
            
            # Wait for connection
            timeout = 10
            while not self.connected and timeout > 0:
                time.sleep(0.5)
                timeout -= 0.5
                
            return self.connected
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
            
    def listen(self, duration=None):
        """Listen for filtered messages for specified duration"""
        if not self.connected:
            print("❌ Not connected to broker")
            return False
            
        try:
            if duration:
                print(f"⏱️  Listening for filtered messages for {duration} seconds...")
                time.sleep(duration)
            else:
                print("🎧 Listening for filtered messages indefinitely... Press Ctrl+C to stop")
                while True:
                    time.sleep(1)
                    
        except KeyboardInterrupt:
            print("\n🛑 Filtered listening stopped by user")
            
        return True
        
    def print_summary(self):
        """Print conditional relay test summary"""
        if self.start_time:
            duration = datetime.now() - self.start_time
            print("\n" + "=" * 70)
            print("🧠 CONDITIONAL RELAY TEST SUMMARY")
            print("=" * 70)
            print(f"Duration: {duration}")
            print(f"Filtered messages received: {self.messages_received}")
            print(f"Unique test IDs: {len(self.test_results)}")
            
            if self.test_results:
                print("\n📋 Received Filtered Test Messages:")
                for test_id, result in self.test_results.items():
                    priority = result.get('priority', 'unknown')
                    msg_type = result.get('type', 'unknown')
                    print(f"   ✅ {test_id}: {result['topic']} (P:{priority}, T:{msg_type})")
                    
                # Check expected filtered message types
                expected_patterns = [
                    "filtered/sensors/",
                    "filtered/events/",
                    "filtered/priority/",
                    "filtered/commands/"
                ]
                
                print("\n🔍 Conditional Relay Pattern Analysis:")
                for pattern in expected_patterns:
                    matching = [r for r in self.test_results.values() if r['topic'].startswith(pattern)]
                    status = "🧠" if matching else "❌"
                    print(f"   {status} {pattern}*: {len(matching)} messages")
                    
                # Filtering effectiveness analysis
                print("\n🔍 Filtering Logic Analysis:")
                print(f"   High priority received: {self.filtering_stats['high_priority_received']} (✅ should pass)")
                print(f"   Low priority received: {self.filtering_stats['low_priority_received']} (❌ should be filtered)")
                print(f"   Debug type received: {self.filtering_stats['debug_type_received']} (❌ should be filtered)")
                print(f"   Old messages received: {self.filtering_stats['old_messages_received']} (❌ should be filtered)")
                print(f"   Normal messages received: {self.filtering_stats['normal_messages_received']} (✅ should pass)")
                print(f"   Unexpected messages: {self.filtering_stats['unexpected_messages']}")
                
                # Overall filtering assessment
                filter_violations = (
                    self.filtering_stats['low_priority_received'] +
                    self.filtering_stats['debug_type_received'] +
                    self.filtering_stats['old_messages_received']
                )
                
                print(f"\n🎯 Filtering Effectiveness:")
                if filter_violations == 0:
                    print("   ✅ All filtering rules working correctly")
                    print("   ✅ No unwanted messages passed through")
                else:
                    print(f"   ❌ {filter_violations} messages bypassed filtering rules")
                    print("   ❌ Filtering logic needs investigation")
                    
            else:
                print("\n❌ No filtered messages received - Check:")
                print("   1. Is conditional_relay_client C binary running?")
                print("   2. Is the publisher sending messages with various priorities/types?")
                print("   3. Are source and destination brokers running?")
                print("   4. Check conditional relay configuration file")
                print("   5. Verify filtering logic is enabled in C binary")
                
            print("=" * 70)
            
    def disconnect(self):
        """Disconnect from the broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
def main():
    parser = argparse.ArgumentParser(description='Conditional Relay Subscriber Test')
    parser.add_argument('--host', default='localhost', help='MQTT broker host')
    parser.add_argument('--port', type=int, default=1887, help='MQTT broker port')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--duration', '-d', type=int, help='Listen duration in seconds')
    
    args = parser.parse_args()
    
    subscriber = ConditionalRelaySubscriber(args.host, args.port, args.verbose)
    
    print("🧠 Conditional Relay Subscriber Test")
    print("=" * 70)
    print(f"Target broker: {args.host}:{args.port}")
    print(f"Purpose: Verify C conditional_relay_client binary filtering logic")
    print("=" * 70)
    print()
    
    try:
        subscriber.setup_client()
        
        if not subscriber.connect():
            print("❌ Failed to connect to broker")
            return 1
            
        subscriber.listen(args.duration)
        
        return 0
        
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        subscriber.print_summary()
        subscriber.disconnect()

if __name__ == "__main__":
    sys.exit(main())