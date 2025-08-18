#!/usr/bin/env python3
"""
Basic Relay Subscriber Test Script

Subscribes to destination broker (port 1885) to verify that the basic_relay_client
C binary correctly forwards messages from source broker (port 1883).

Usage: python3 basic_relay_subscriber.py
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import argparse
from datetime import datetime

class BasicRelaySubscriber:
    def __init__(self, host="localhost", port=1885, verbose=False):
        self.host = host
        self.port = port
        self.verbose = verbose
        self.client = None
        self.connected = False
        self.messages_received = 0
        self.test_results = {}
        self.start_time = None
        
    def setup_client(self):
        """Setup MQTT client for subscribing to destination broker"""
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION1,
            client_id="basic_relay_subscriber"
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
            
            # Subscribe to expected forwarded topics based on basic_relay_config.json
            subscription_topics = [
                "forwarded/sensors/+",      # data/sensors/+ → forwarded/sensors/+
                "forwarded/system/+",       # events/system/+ → forwarded/system/+
                "relayed/commands/+"        # commands/+ → relayed/commands/+
            ]
            
            print("🔔 Subscribing to forwarded topics:")
            for topic in subscription_topics:
                result = client.subscribe(topic, qos=1)
                if result[0] == mqtt.MQTT_ERR_SUCCESS:
                    print(f"   ✅ {topic}")
                else:
                    print(f"   ❌ {topic} (failed)")
                    
            print()
            self.start_time = datetime.now()
            print("🎧 Listening for forwarded messages...")
            print("=" * 60)
        else:
            print(f"❌ Failed to connect to destination broker: {rc}")
            
    def on_message(self, client, userdata, msg):
        """Handle received messages from the relay"""
        self.messages_received += 1
        topic = msg.topic
        
        try:
            payload = msg.payload.decode('utf-8')
            timestamp = datetime.now().isoformat()
            
            print(f"📥 RELAY MESSAGE RECEIVED #{self.messages_received}")
            print(f"   Topic: {topic}")
            print(f"   Time: {timestamp}")
            
            if self.verbose:
                print(f"   QoS: {msg.qos}")
                print(f"   Retain: {msg.retain}")
                
            # Try to parse JSON payload
            try:
                data = json.loads(payload)
                test_id = data.get('test_id', 'unknown')
                
                print(f"   Test ID: {test_id}")
                
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
                    'data': data
                }
                
            except json.JSONDecodeError:
                print(f"   Payload (raw): {payload}")
                
            # Analyze relay correctness
            self.analyze_relay_correctness(topic, payload)
            
        except Exception as e:
            print(f"   ❌ Error processing message: {e}")
            
        print("   " + "-" * 56)
        print()
        
    def analyze_relay_correctness(self, topic, payload):
        """Analyze if the relay forwarding is working correctly"""
        relay_status = "✅ CORRECT"
        analysis = []
        
        # Check topic forwarding rules from basic_relay_config.json
        if topic.startswith("forwarded/sensors/"):
            expected_source = topic.replace("forwarded/sensors/", "data/sensors/")
            analysis.append(f"Sensor data relay: {expected_source} → {topic}")
            
        elif topic.startswith("forwarded/system/"):
            expected_source = topic.replace("forwarded/system/", "events/system/")
            analysis.append(f"System event relay: {expected_source} → {topic}")
            
        elif topic.startswith("relayed/commands/"):
            expected_source = topic.replace("relayed/commands/", "commands/")
            analysis.append(f"Command relay: {expected_source} → {topic}")
            
        else:
            relay_status = "❓ UNEXPECTED"
            analysis.append(f"Unexpected topic pattern: {topic}")
            
        print(f"   Relay Status: {relay_status}")
        for note in analysis:
            print(f"   Analysis: {note}")
            
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
        """Listen for messages for specified duration"""
        if not self.connected:
            print("❌ Not connected to broker")
            return False
            
        try:
            if duration:
                print(f"⏱️  Listening for {duration} seconds...")
                time.sleep(duration)
            else:
                print("🎧 Listening indefinitely... Press Ctrl+C to stop")
                while True:
                    time.sleep(1)
                    
        except KeyboardInterrupt:
            print("\n🛑 Listening stopped by user")
            
        return True
        
    def print_summary(self):
        """Print test summary"""
        if self.start_time:
            duration = datetime.now() - self.start_time
            print("\n" + "=" * 60)
            print("📊 BASIC RELAY TEST SUMMARY")
            print("=" * 60)
            print(f"Duration: {duration}")
            print(f"Messages received: {self.messages_received}")
            print(f"Unique test IDs: {len(self.test_results)}")
            
            if self.test_results:
                print("\n📋 Received Test Messages:")
                for test_id, result in self.test_results.items():
                    print(f"   ✅ {test_id}: {result['topic']}")
                    
                # Check expected message types
                expected_patterns = [
                    "forwarded/sensors/",
                    "forwarded/system/", 
                    "relayed/commands/"
                ]
                
                print("\n🔍 Relay Pattern Analysis:")
                for pattern in expected_patterns:
                    matching = [r for r in self.test_results.values() if r['topic'].startswith(pattern)]
                    status = "✅" if matching else "❌"
                    print(f"   {status} {pattern}*: {len(matching)} messages")
                    
            else:
                print("\n❌ No messages received - Check:")
                print("   1. Is basic_relay_client C binary running?")
                print("   2. Is the publisher sending messages?")
                print("   3. Are source and destination brokers running?")
                print("   4. Check relay configuration file")
                
            print("=" * 60)
            
    def disconnect(self):
        """Disconnect from the broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
def main():
    parser = argparse.ArgumentParser(description='Basic Relay Subscriber Test')
    parser.add_argument('--host', default='localhost', help='MQTT broker host')
    parser.add_argument('--port', type=int, default=1885, help='MQTT broker port')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--duration', '-d', type=int, help='Listen duration in seconds')
    
    args = parser.parse_args()
    
    subscriber = BasicRelaySubscriber(args.host, args.port, args.verbose)
    
    print("🎧 Basic Relay Subscriber Test")
    print("=" * 60)
    print(f"Target broker: {args.host}:{args.port}")
    print(f"Purpose: Verify C basic_relay_client binary forwarding")
    print("=" * 60)
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