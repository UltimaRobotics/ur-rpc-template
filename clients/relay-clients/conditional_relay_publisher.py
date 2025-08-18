#!/usr/bin/env python3
"""
Conditional Relay Publisher Test Script

Publishes test messages to source broker (port 1883) to verify that the conditional_relay_client
C binary correctly applies filtering logic and forwards only qualifying messages to destination broker (port 1887).

Usage: python3 conditional_relay_publisher.py
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import argparse
from datetime import datetime

class ConditionalRelayPublisher:
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
            client_id="conditional_relay_publisher"
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
            
    def publish_conditional_test_messages(self):
        """Publish test messages with various conditions to test filtering logic"""
        if not self.connected:
            print("❌ Not connected to broker")
            return False
            
        print("🧠 Publishing conditional test messages for filtering verification...")
        print("=" * 70)
        
        current_time = int(time.time())
        old_time = current_time - 400  # 6+ minutes old (should be filtered)
        
        # Test messages based on conditional_relay_config.json rules:
        # 1. smart/sensors/+ → filtered/sensors/+
        # 2. smart/events/+ → filtered/events/+
        # 3. smart/high_priority/+ → filtered/priority/+
        # 4. smart/commands/+ → filtered/commands/+
        # Plus conditional logic (priority, type, timestamp filtering)
        
        test_messages = [
            # HIGH PRIORITY - Should pass through filtering
            {
                'topic': 'smart/high_priority/critical_alert',
                'payload': json.dumps({
                    'alert_id': 'critical_001',
                    'priority': 'high',
                    'type': 'critical',
                    'message': 'Critical system failure detected',
                    'timestamp': current_time,
                    'test_id': 'conditional_high_priority'
                }),
                'expected_destination': 'filtered/priority/critical_alert',
                'should_pass': True,
                'filter_reason': 'High priority should pass'
            },
            
            # LOW PRIORITY - Should be filtered out
            {
                'topic': 'smart/sensors/low_priority_temp',
                'payload': json.dumps({
                    'sensor_id': 'temp_low_001',
                    'priority': 'low',
                    'type': 'sensor_reading',
                    'value': 22.0,
                    'timestamp': current_time,
                    'test_id': 'conditional_low_priority'
                }),
                'expected_destination': 'filtered/sensors/low_priority_temp',
                'should_pass': False,
                'filter_reason': 'Low priority should be filtered'
            },
            
            # DEBUG TYPE - Should be filtered out
            {
                'topic': 'smart/events/debug_event',
                'payload': json.dumps({
                    'event_id': 'debug_001',
                    'priority': 'normal',
                    'type': 'debug',
                    'message': 'Debug information for developers',
                    'timestamp': current_time,
                    'test_id': 'conditional_debug_type'
                }),
                'expected_destination': 'filtered/events/debug_event',
                'should_pass': False,
                'filter_reason': 'Debug messages should be filtered'
            },
            
            # OLD MESSAGE - Should be filtered out
            {
                'topic': 'smart/sensors/old_reading',
                'payload': json.dumps({
                    'sensor_id': 'temp_old_001',
                    'priority': 'normal',
                    'type': 'sensor_reading',
                    'value': 25.5,
                    'timestamp': old_time,
                    'test_id': 'conditional_old_message'
                }),
                'expected_destination': 'filtered/sensors/old_reading',
                'should_pass': False,
                'filter_reason': 'Old messages should be filtered'
            },
            
            # NORMAL PRIORITY, CURRENT TIME - Should pass
            {
                'topic': 'smart/sensors/normal_temp',
                'payload': json.dumps({
                    'sensor_id': 'temp_normal_001',
                    'priority': 'normal',
                    'type': 'sensor_reading',
                    'value': 24.5,
                    'timestamp': current_time,
                    'test_id': 'conditional_normal_temp'
                }),
                'expected_destination': 'filtered/sensors/normal_temp',
                'should_pass': True,
                'filter_reason': 'Normal priority current message should pass'
            },
            
            # INFO EVENT - Should pass
            {
                'topic': 'smart/events/info_event',
                'payload': json.dumps({
                    'event_id': 'info_001',
                    'priority': 'normal',
                    'type': 'info',
                    'message': 'System backup completed successfully',
                    'timestamp': current_time,
                    'test_id': 'conditional_info_event'
                }),
                'expected_destination': 'filtered/events/info_event',
                'should_pass': True,
                'filter_reason': 'Info events should pass'
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
                    'timestamp': current_time,
                    'test_id': 'conditional_command'
                }),
                'expected_destination': 'filtered/commands/restart_service',
                'should_pass': True,
                'filter_reason': 'Commands should pass'
            },
            
            # MEDIUM PRIORITY - Should pass (not explicitly filtered)
            {
                'topic': 'smart/sensors/medium_priority',
                'payload': json.dumps({
                    'sensor_id': 'pressure_001',
                    'priority': 'medium',
                    'type': 'sensor_reading',
                    'value': 1013.25,
                    'timestamp': current_time,
                    'test_id': 'conditional_medium_priority'
                }),
                'expected_destination': 'filtered/sensors/medium_priority',
                'should_pass': True,
                'filter_reason': 'Medium priority should pass'
            },
            
            # WARNING EVENT - Should pass
            {
                'topic': 'smart/events/warning_event',
                'payload': json.dumps({
                    'event_id': 'warn_001',
                    'priority': 'medium',
                    'type': 'warning',
                    'message': 'Disk space running low',
                    'timestamp': current_time,
                    'test_id': 'conditional_warning_event'
                }),
                'expected_destination': 'filtered/events/warning_event',
                'should_pass': True,
                'filter_reason': 'Warning events should pass'
            },
            
            # ERROR TYPE - Should pass (higher importance than debug)
            {
                'topic': 'smart/events/error_event',
                'payload': json.dumps({
                    'event_id': 'error_001',
                    'priority': 'high',
                    'type': 'error',
                    'message': 'Database connection failed',
                    'timestamp': current_time,
                    'test_id': 'conditional_error_event'
                }),
                'expected_destination': 'filtered/events/error_event',
                'should_pass': True,
                'filter_reason': 'Error events should pass'
            },
            
            # SENSOR WITH NO PRIORITY - Should be filtered (missing priority)
            {
                'topic': 'smart/sensors/no_priority',
                'payload': json.dumps({
                    'sensor_id': 'temp_no_priority',
                    'type': 'sensor_reading',
                    'value': 23.0,
                    'timestamp': current_time,
                    'test_id': 'conditional_no_priority'
                }),
                'expected_destination': 'filtered/sensors/no_priority',
                'should_pass': False,
                'filter_reason': 'Messages without priority should be filtered'
            }
        ]
        
        expected_to_pass = len([m for m in test_messages if m['should_pass']])
        expected_to_filter = len([m for m in test_messages if not m['should_pass']])
        
        print(f"Publishing {len(test_messages)} conditional test messages...")
        print(f"Expected to pass filtering: {expected_to_pass}")
        print(f"Expected to be filtered: {expected_to_filter}")
        print()
        
        for i, msg in enumerate(test_messages, 1):
            pass_status = "✅ SHOULD PASS" if msg['should_pass'] else "🚫 SHOULD FILTER"
            
            print(f"[{i}/{len(test_messages)}] Publishing to: {msg['topic']}")
            print(f"                Expected at: {msg['expected_destination']}")
            print(f"                Filter test: {pass_status}")
            print(f"                Reason: {msg['filter_reason']}")
            
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
            
        print("=" * 70)
        print(f"🧠 Completed publishing {len(test_messages)} conditional test messages")
        print(f"📊 Total messages published: {self.messages_published}")
        print(f"📊 Expected to pass: {expected_to_pass}")
        print(f"📊 Expected to filter: {expected_to_filter}")
        print()
        print("📋 Next steps:")
        print("   1. Ensure the conditional_relay_client C binary is running")
        print("   2. Run conditional_relay_subscriber.py to verify filtering logic")
        print("   3. Check that only qualifying messages appear on destination broker (port 1887)")
        print("   4. Verify filtering rules: priority, type, timestamp, etc.")
        
        return True
        
    def disconnect(self):
        """Disconnect from the broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
def main():
    parser = argparse.ArgumentParser(description='Conditional Relay Publisher Test')
    parser.add_argument('--host', default='localhost', help='MQTT broker host')
    parser.add_argument('--port', type=int, default=1883, help='MQTT broker port')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--continuous', '-c', action='store_true', help='Publish messages continuously')
    parser.add_argument('--interval', type=int, default=20, help='Interval between message sets (seconds)')
    
    args = parser.parse_args()
    
    publisher = ConditionalRelayPublisher(args.host, args.port, args.verbose)
    
    print("🧠 Conditional Relay Publisher Test")
    print("=" * 70)
    print(f"Target broker: {args.host}:{args.port}")
    print(f"Purpose: Test C conditional_relay_client binary filtering logic")
    print("=" * 70)
    print()
    
    try:
        publisher.setup_client()
        
        if not publisher.connect():
            print("❌ Failed to connect to broker")
            return 1
            
        if args.continuous:
            print(f"🔄 Continuous conditional mode: Publishing every {args.interval} seconds")
            print("Press Ctrl+C to stop")
            print()
            
            try:
                while True:
                    publisher.publish_conditional_test_messages()
                    print(f"⏳ Waiting {args.interval} seconds before next conditional cycle...")
                    print()
                    time.sleep(args.interval)
            except KeyboardInterrupt:
                print("\n🛑 Continuous conditional publishing stopped by user")
        else:
            publisher.publish_conditional_test_messages()
            
        return 0
        
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        publisher.disconnect()

if __name__ == "__main__":
    sys.exit(main())