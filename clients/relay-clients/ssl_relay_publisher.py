#!/usr/bin/env python3
"""
SSL Relay Publisher Test Script

Publishes test messages to SSL source broker (port 1884) to verify that the ssl_relay_client
C binary correctly forwards encrypted messages to SSL destination broker (port 1886).

Usage: python3 ssl_relay_publisher.py
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import ssl
import argparse
import os
from datetime import datetime

class SSLRelayPublisher:
    def __init__(self, host="localhost", port=1884, verbose=False):
        self.host = host
        self.port = port
        self.verbose = verbose
        self.client = None
        self.connected = False
        self.messages_published = 0
        
        # SSL certificate paths (same as ssl_relay_config.json)
        self.ca_cert = "../ssl_certs/ca.crt"
        self.client_cert = "../ssl_certs/client.crt" 
        self.client_key = "../ssl_certs/client.key"
        
    def setup_client(self):
        """Setup SSL MQTT client for publishing to source broker"""
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION1,
            client_id="ssl_relay_publisher"
        )
        self.client.on_connect = self.on_connect
        self.client.on_publish = self.on_publish
        self.client.on_disconnect = self.on_disconnect
        
        if self.verbose:
            self.client.on_log = self.on_log
            
        # Configure SSL/TLS
        try:
            self.client.tls_set(
                ca_certs=self.ca_cert,
                certfile=self.client_cert,
                keyfile=self.client_key,
                cert_reqs=ssl.CERT_REQUIRED,
                tls_version=ssl.PROTOCOL_TLS,
                ciphers=None
            )
            print(f"🔐 SSL configuration applied with certificates:")
            print(f"   CA: {self.ca_cert}")
            print(f"   Cert: {self.client_cert}")
            print(f"   Key: {self.client_key}")
        except Exception as e:
            print(f"❌ SSL configuration failed: {e}")
            return False
            
        return True
        
    def check_ssl_certificates(self):
        """Check if SSL certificates exist"""
        ssl_files = [self.ca_cert, self.client_cert, self.client_key]
        missing_files = []
        
        for ssl_file in ssl_files:
            if not os.path.exists(ssl_file):
                missing_files.append(ssl_file)
                
        if missing_files:
            print("❌ Missing SSL certificate files:")
            for file in missing_files:
                print(f"   {file}")
            print("\nGenerate SSL certificates first or check paths.")
            return False
            
        print("✅ All SSL certificate files found")
        return True
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"🔐 SSL connection established to source broker at {self.host}:{self.port}")
        else:
            print(f"❌ SSL connection failed to source broker: {rc}")
            
    def on_publish(self, client, userdata, mid):
        self.messages_published += 1
        if self.verbose:
            print(f"📤 Encrypted message published (mid: {mid})")
            
    def on_disconnect(self, client, userdata, rc):
        self.connected = False
        print(f"🔐 SSL disconnected from source broker")
        
    def on_log(self, client, userdata, level, buf):
        # Only log SSL-related messages
        if "SSL" in buf or "TLS" in buf or "certificate" in buf:
            print(f"🔐 SSL Log: {buf}")
        
    def connect(self):
        """Connect to the SSL source broker"""
        try:
            self.client.connect(self.host, self.port, 60)
            self.client.loop_start()
            
            # Wait for SSL connection
            timeout = 15  # SSL connections may take longer
            while not self.connected and timeout > 0:
                time.sleep(0.5)
                timeout -= 0.5
                
            return self.connected
        except Exception as e:
            print(f"❌ SSL connection failed: {e}")
            return False
            
    def publish_ssl_test_messages(self):
        """Publish encrypted test messages according to ssl_relay_config.json rules"""
        if not self.connected:
            print("❌ Not connected to SSL broker")
            return False
            
        print("🔐 Publishing encrypted test messages for SSL Relay verification...")
        print("=" * 60)
        
        # Test messages based on ssl_relay_config.json rules:
        # 1. secure/sensors/+ → encrypted/sensors/+
        # 2. secure/events/+ → encrypted/events/+
        # 3. secure/commands/+ → encrypted/commands/+ (bidirectional)
        
        test_messages = [
            # Secure sensor data messages (should be forwarded to encrypted/sensors/+)
            {
                'topic': 'secure/sensors/temperature',
                'payload': json.dumps({
                    'sensor_id': 'ssl_temp_001',
                    'value': 28.7,
                    'unit': 'celsius',
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_sensor_temp'
                }),
                'expected_destination': 'encrypted/sensors/temperature'
            },
            {
                'topic': 'secure/sensors/pressure',
                'payload': json.dumps({
                    'sensor_id': 'ssl_press_001',
                    'value': 1013.25,
                    'unit': 'hPa', 
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_sensor_press'
                }),
                'expected_destination': 'encrypted/sensors/pressure'
            },
            {
                'topic': 'secure/sensors/humidity',
                'payload': json.dumps({
                    'sensor_id': 'ssl_hum_001',
                    'value': 67.3,
                    'unit': 'percent',
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_sensor_hum'
                }),
                'expected_destination': 'encrypted/sensors/humidity'
            },
            
            # Secure events (should be forwarded to encrypted/events/+)
            {
                'topic': 'secure/events/security',
                'payload': json.dumps({
                    'event_type': 'security_alert',
                    'level': 'high',
                    'message': 'Unauthorized access attempt detected',
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_event_security'
                }),
                'expected_destination': 'encrypted/events/security'
            },
            {
                'topic': 'secure/events/backup',
                'payload': json.dumps({
                    'event_type': 'backup_complete',
                    'status': 'success',
                    'size': '1.2GB',
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_event_backup'
                }),
                'expected_destination': 'encrypted/events/backup'
            },
            {
                'topic': 'secure/events/audit',
                'payload': json.dumps({
                    'event_type': 'audit_log',
                    'user': 'admin',
                    'action': 'config_change',
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_event_audit'
                }),
                'expected_destination': 'encrypted/events/audit'
            },
            
            # Secure commands (should be relayed to encrypted/commands/+ - bidirectional)
            {
                'topic': 'secure/commands/encrypt',
                'payload': json.dumps({
                    'command': 'encrypt_data',
                    'algorithm': 'AES-256',
                    'key_rotation': True,
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_cmd_encrypt'
                }),
                'expected_destination': 'encrypted/commands/encrypt'
            },
            {
                'topic': 'secure/commands/backup',
                'payload': json.dumps({
                    'command': 'secure_backup',
                    'target': 'user_data',
                    'encryption': 'enabled',
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_cmd_backup'
                }),
                'expected_destination': 'encrypted/commands/backup'
            },
            {
                'topic': 'secure/commands/rotate_keys',
                'payload': json.dumps({
                    'command': 'rotate_encryption_keys',
                    'key_type': 'ssl_certificates',
                    'validity': '365_days',
                    'encrypted': True,
                    'timestamp': time.time(),
                    'test_id': 'ssl_cmd_rotate'
                }),
                'expected_destination': 'encrypted/commands/rotate_keys'
            }
        ]
        
        print(f"Publishing {len(test_messages)} encrypted test messages...")
        print()
        
        for i, msg in enumerate(test_messages, 1):
            print(f"[{i}/{len(test_messages)}] Publishing to: {msg['topic']}")
            print(f"                Expected at: {msg['expected_destination']}")
            
            if self.verbose:
                payload_preview = msg['payload'][:100] + "..." if len(msg['payload']) > 100 else msg['payload']
                print(f"                Payload: {payload_preview}")
            
            result = self.client.publish(msg['topic'], msg['payload'], qos=1)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"                Status: 🔐 Encrypted & Published successfully")
            else:
                print(f"                Status: ❌ Publish failed (rc: {result.rc})")
                
            print()
            time.sleep(2)  # Slower for SSL processing
            
        print("=" * 60)
        print(f"🔐 Completed publishing {len(test_messages)} encrypted test messages")
        print(f"📊 Total encrypted messages published: {self.messages_published}")
        print()
        print("📋 Next steps:")
        print("   1. Ensure the ssl_relay_client C binary is running")
        print("   2. Run ssl_relay_subscriber.py to verify encrypted message forwarding")
        print("   3. Check that encrypted messages appear on SSL destination broker (port 1886)")
        
        return True
        
    def disconnect(self):
        """Disconnect from the SSL broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
def main():
    parser = argparse.ArgumentParser(description='SSL Relay Publisher Test')
    parser.add_argument('--host', default='localhost', help='MQTT broker host')
    parser.add_argument('--port', type=int, default=1884, help='MQTT broker port')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--continuous', '-c', action='store_true', help='Publish messages continuously')
    parser.add_argument('--interval', type=int, default=15, help='Interval between message sets (seconds)')
    
    args = parser.parse_args()
    
    publisher = SSLRelayPublisher(args.host, args.port, args.verbose)
    
    print("🔐 SSL Relay Publisher Test")
    print("=" * 60)
    print(f"Target SSL broker: {args.host}:{args.port}")
    print(f"Purpose: Test C ssl_relay_client binary encrypted forwarding")
    print("=" * 60)
    print()
    
    try:
        # Check SSL certificates first
        if not publisher.check_ssl_certificates():
            return 1
            
        if not publisher.setup_client():
            return 1
            
        if not publisher.connect():
            print("❌ Failed to establish SSL connection to broker")
            return 1
            
        if args.continuous:
            print(f"🔄 Continuous encrypted mode: Publishing every {args.interval} seconds")
            print("Press Ctrl+C to stop")
            print()
            
            try:
                while True:
                    publisher.publish_ssl_test_messages()
                    print(f"⏳ Waiting {args.interval} seconds before next encrypted cycle...")
                    print()
                    time.sleep(args.interval)
            except KeyboardInterrupt:
                print("\n🛑 Continuous encrypted publishing stopped by user")
        else:
            publisher.publish_ssl_test_messages()
            
        return 0
        
    except Exception as e:
        print(f"❌ SSL Error: {e}")
        return 1
    finally:
        publisher.disconnect()

if __name__ == "__main__":
    sys.exit(main())