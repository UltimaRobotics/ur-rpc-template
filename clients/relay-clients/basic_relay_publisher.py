#!/usr/bin/env python3
"""
Basic Relay Publisher Test Script

Publishes test messages to source broker (port 1883) to verify that the basic_relay_client
C binary correctly forwards messages to destination broker (port 1885).

Usage: python3 basic_relay_publisher.py
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import argparse
from datetime import datetime

class BasicRelayPublisher:
    def __init__(self, host="localhost", port=1883, verbose=False):
        self.host = host
        self.port = port
        self.verbose = verbose
        self.client = None
        self.connected = False
        self.messages_published = 0
        
    def setup_client(self):
        """Setup MQTT client for publishing to source broker"""
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION1, 
            client_id="basic_relay_publisher"
        )
        self.client.on_connect = self.on_connect
        self.client.on_publish = self.on_publish
        self.client.on_disconnect = self.on_disconnect
        
        if self.verbose:
            self.client.on_log = self.on_log
            
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"✅ Connected to source broker at {self.host}:{self.port}")
        else:
            print(f"❌ Failed to connect to source broker: {rc}")
            
    def on_publish(self, client, userdata, mid):
        self.messages_published += 1
        if self.verbose:
            print(f"📤 Message published (mid: {mid})")
            
    def on_disconnect(self, client, userdata, rc):
        self.connected = False
        print(f"📡 Disconnected from source broker")
        
    def on_log(self, client, userdata, level, buf):
        print(f"🔍 Log: {buf}")
        
    def connect(self):
        """Connect to the source broker"""
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
            
    def publish_test_messages(self):
        """Publish test messages according to basic_relay_config.json rules"""
        if not self.connected:
            print("❌ Not connected to broker")
            return False
            
        print("📤 Publishing test messages for Basic Relay verification...")
        print("=" * 60)
        
        # Test messages based on basic_relay_config.json rules:
        # 1. data/sensors/+ → forwarded/sensors/+
        # 2. events/system/+ → forwarded/system/+  
        # 3. commands/+ → relayed/commands/+ (bidirectional)
        
        test_messages = [
            # Sensor data messages (should be forwarded to forwarded/sensors/+)
            {
                'topic': 'data/sensors/temperature',
                'payload': json.dumps({
                    'sensor_id': 'temp_001',
                    'value': 25.5,
                    'unit': 'celsius',
                    'timestamp': time.time(),
                    'test_id': 'basic_sensor_temp'
                }),
                'expected_destination': 'forwarded/sensors/temperature'
            },
            {
                'topic': 'data/sensors/humidity',
                'payload': json.dumps({
                    'sensor_id': 'hum_001',
                    'value': 65.2,
                    'unit': 'percent',
                    'timestamp': time.time(),
                    'test_id': 'basic_sensor_hum'
                }),
                'expected_destination': 'forwarded/sensors/humidity'
            },
            {
                'topic': 'data/sensors/pressure',
                'payload': json.dumps({
                    'sensor_id': 'press_001',
                    'value': 1013.25,
                    'unit': 'hPa',
                    'timestamp': time.time(),
                    'test_id': 'basic_sensor_press'
                }),
                'expected_destination': 'forwarded/sensors/pressure'
            },
            
            # System events (should be forwarded to forwarded/system/+)
            {
                'topic': 'events/system/startup',
                'payload': json.dumps({
                    'event_type': 'system_startup',
                    'message': 'System started successfully',
                    'timestamp': time.time(),
                    'test_id': 'basic_event_startup'
                }),
                'expected_destination': 'forwarded/system/startup'
            },
            {
                'topic': 'events/system/alert',
                'payload': json.dumps({
                    'event_type': 'system_alert',
                    'level': 'warning',
                    'message': 'High memory usage detected',
                    'timestamp': time.time(),
                    'test_id': 'basic_event_alert'
                }),
                'expected_destination': 'forwarded/system/alert'
            },
            {
                'topic': 'events/system/shutdown',
                'payload': json.dumps({
                    'event_type': 'system_shutdown',
                    'message': 'Graceful shutdown initiated',
                    'timestamp': time.time(),
                    'test_id': 'basic_event_shutdown'
                }),
                'expected_destination': 'forwarded/system/shutdown'
            },
            
            # Commands (should be relayed to relayed/commands/+ - bidirectional)
            {
                'topic': 'commands/restart',
                'payload': json.dumps({
                    'command': 'restart_service',
                    'service': 'web_server',
                    'timestamp': time.time(),
                    'test_id': 'basic_cmd_restart'
                }),
                'expected_destination': 'relayed/commands/restart'
            },
            {
                'topic': 'commands/status',
                'payload': json.dumps({
                    'command': 'get_status',
                    'service': 'database',
                    'timestamp': time.time(),
                    'test_id': 'basic_cmd_status'
                }),
                'expected_destination': 'relayed/commands/status'
            },
            {
                'topic': 'commands/config',
                'payload': json.dumps({
                    'command': 'update_config',
                    'config_file': 'app.conf',
                    'timestamp': time.time(),
                    'test_id': 'basic_cmd_config'
                }),
                'expected_destination': 'relayed/commands/config'
            }
        ]
        
        print(f"Publishing {len(test_messages)} test messages...")
        print()
        
        for i, msg in enumerate(test_messages, 1):
            print(f"[{i}/{len(test_messages)}] Publishing to: {msg['topic']}")
            print(f"                Expected at: {msg['expected_destination']}")
            
            if self.verbose:
                payload_preview = msg['payload'][:100] + "..." if len(msg['payload']) > 100 else msg['payload']
                print(f"                Payload: {payload_preview}")
            
            result = self.client.publish(msg['topic'], msg['payload'], qos=1)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"                Status: ✅ Published successfully")
            else:
                print(f"                Status: ❌ Publish failed (rc: {result.rc})")
                
            print()
            time.sleep(1)  # Delay between messages
            
        print("=" * 60)
        print(f"✅ Completed publishing {len(test_messages)} test messages")
        print(f"📊 Total messages published: {self.messages_published}")
        print()
        print("📋 Next steps:")
        print("   1. Ensure the basic_relay_client C binary is running")
        print("   2. Run basic_relay_subscriber.py to verify message forwarding")
        print("   3. Check that messages appear on destination broker (port 1885)")
        
        return True
        
    def disconnect(self):
        """Disconnect from the broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
def main():
    parser = argparse.ArgumentParser(description='Basic Relay Publisher Test')
    parser.add_argument('--host', default='localhost', help='MQTT broker host')
    parser.add_argument('--port', type=int, default=1883, help='MQTT broker port')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--continuous', '-c', action='store_true', help='Publish messages continuously')
    parser.add_argument('--interval', type=int, default=10, help='Interval between message sets (seconds)')
    
    args = parser.parse_args()
    
    publisher = BasicRelayPublisher(args.host, args.port, args.verbose)
    
    print("🚀 Basic Relay Publisher Test")
    print("=" * 60)
    print(f"Target broker: {args.host}:{args.port}")
    print(f"Purpose: Test C basic_relay_client binary forwarding")
    print("=" * 60)
    print()
    
    try:
        publisher.setup_client()
        
        if not publisher.connect():
            print("❌ Failed to connect to broker")
            return 1
            
        if args.continuous:
            print(f"🔄 Continuous mode: Publishing every {args.interval} seconds")
            print("Press Ctrl+C to stop")
            print()
            
            try:
                while True:
                    publisher.publish_test_messages()
                    print(f"⏳ Waiting {args.interval} seconds before next cycle...")
                    print()
                    time.sleep(args.interval)
            except KeyboardInterrupt:
                print("\n🛑 Continuous publishing stopped by user")
        else:
            publisher.publish_test_messages()
            
        return 0
        
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        publisher.disconnect()

if __name__ == "__main__":
    sys.exit(main())