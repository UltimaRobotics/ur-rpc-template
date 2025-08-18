#!/usr/bin/env python3
"""
SSL Relay Subscriber Test Script

Subscribes to SSL destination broker (port 1886) to verify that the ssl_relay_client
C binary correctly forwards encrypted messages from SSL source broker (port 1884).

Usage: python3 ssl_relay_subscriber.py
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import ssl
import argparse
import os
from datetime import datetime

class SSLRelaySubscriber:
    def __init__(self, host="localhost", port=1886, verbose=False):
        self.host = host
        self.port = port
        self.verbose = verbose
        self.client = None
        self.connected = False
        self.messages_received = 0
        self.test_results = {}
        self.start_time = None
        
        # SSL certificate paths (same as ssl_relay_config.json)
        self.ca_cert = "../ssl_certs/ca.crt"
        self.client_cert = "../ssl_certs/client.crt"
        self.client_key = "../ssl_certs/client.key"
        
    def setup_client(self):
        """Setup SSL MQTT client for subscribing to destination broker"""
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION1,
            client_id="ssl_relay_subscriber"
        )
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
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
            print(f"🔐 SSL connection established to destination broker at {self.host}:{self.port}")
            
            # Subscribe to expected encrypted destination topics based on ssl_relay_config.json
            subscription_topics = [
                "encrypted/sensors/+",      # secure/sensors/+ → encrypted/sensors/+
                "encrypted/events/+",       # secure/events/+ → encrypted/events/+
                "encrypted/commands/+"      # secure/commands/+ → encrypted/commands/+
            ]
            
            print("🔔 Subscribing to encrypted destination topics:")
            for topic in subscription_topics:
                result = client.subscribe(topic, qos=1)
                if result[0] == mqtt.MQTT_ERR_SUCCESS:
                    print(f"   🔐 {topic}")
                else:
                    print(f"   ❌ {topic} (failed)")
                    
            print()
            self.start_time = datetime.now()
            print("🎧 Listening for encrypted relayed messages...")
            print("=" * 60)
        else:
            print(f"❌ SSL connection failed to destination broker: {rc}")
            
    def on_message(self, client, userdata, msg):
        """Handle encrypted messages received from the SSL relay"""
        self.messages_received += 1
        topic = msg.topic
        
        try:
            payload = msg.payload.decode('utf-8')
            timestamp = datetime.now().isoformat()
            
            print(f"🔐 ENCRYPTED RELAY MESSAGE RECEIVED #{self.messages_received}")
            print(f"   Topic: {topic}")
            print(f"   Time: {timestamp}")
            
            if self.verbose:
                print(f"   QoS: {msg.qos}")
                print(f"   Retain: {msg.retain}")
                
            # Try to parse JSON payload
            try:
                data = json.loads(payload)
                test_id = data.get('test_id', 'unknown')
                is_encrypted = data.get('encrypted', False)
                
                print(f"   Test ID: {test_id}")
                print(f"   Encrypted: {'✅ Yes' if is_encrypted else '❌ No'}")
                
                # Extract useful fields
                if 'sensor_id' in data:
                    print(f"   Sensor: {data['sensor_id']} = {data.get('value', 'N/A')} {data.get('unit', '')}")
                elif 'event_type' in data:
                    print(f"   Event: {data['event_type']} - {data.get('message', 'N/A')}")
                elif 'command' in data:
                    print(f"   Command: {data['command']} ({data.get('algorithm', 'N/A')})")
                    
                # Track test results
                self.test_results[test_id] = {
                    'topic': topic,
                    'timestamp': timestamp,
                    'data': data,
                    'encrypted': is_encrypted
                }
                
            except json.JSONDecodeError:
                print(f"   Encrypted Payload (raw): {payload}")
                
            # Analyze SSL relay correctness
            self.analyze_ssl_relay_correctness(topic, payload)
            
        except Exception as e:
            print(f"   ❌ Error processing encrypted message: {e}")
            
        print("   " + "-" * 56)
        print()
        
    def analyze_ssl_relay_correctness(self, topic, payload):
        """Analyze if the SSL relay forwarding is working correctly"""
        relay_status = "🔐 CORRECT"
        analysis = []
        
        # Check encrypted topic forwarding rules from ssl_relay_config.json
        if topic.startswith("encrypted/sensors/"):
            expected_source = topic.replace("encrypted/sensors/", "secure/sensors/")
            analysis.append(f"Encrypted sensor relay: {expected_source} → {topic}")
            
        elif topic.startswith("encrypted/events/"):
            expected_source = topic.replace("encrypted/events/", "secure/events/")
            analysis.append(f"Encrypted event relay: {expected_source} → {topic}")
            
        elif topic.startswith("encrypted/commands/"):
            expected_source = topic.replace("encrypted/commands/", "secure/commands/")
            analysis.append(f"Encrypted command relay: {expected_source} → {topic}")
            
        else:
            relay_status = "❓ UNEXPECTED"
            analysis.append(f"Unexpected encrypted topic pattern: {topic}")
            
        print(f"   SSL Relay Status: {relay_status}")
        for note in analysis:
            print(f"   Analysis: {note}")
            
    def on_disconnect(self, client, userdata, rc):
        self.connected = False
        print(f"🔐 SSL disconnected from destination broker")
        
    def on_log(self, client, userdata, level, buf):
        # Only log SSL-related messages
        if "SSL" in buf or "TLS" in buf or "certificate" in buf:
            print(f"🔐 SSL Log: {buf}")
        
    def connect(self):
        """Connect to the SSL destination broker"""
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
            
    def listen(self, duration=None):
        """Listen for encrypted messages for specified duration"""
        if not self.connected:
            print("❌ Not connected to SSL broker")
            return False
            
        try:
            if duration:
                print(f"⏱️  Listening for encrypted messages for {duration} seconds...")
                time.sleep(duration)
            else:
                print("🎧 Listening for encrypted messages indefinitely... Press Ctrl+C to stop")
                while True:
                    time.sleep(1)
                    
        except KeyboardInterrupt:
            print("\n🛑 Encrypted listening stopped by user")
            
        return True
        
    def print_summary(self):
        """Print SSL test summary"""
        if self.start_time:
            duration = datetime.now() - self.start_time
            print("\n" + "=" * 60)
            print("🔐 SSL RELAY TEST SUMMARY")
            print("=" * 60)
            print(f"Duration: {duration}")
            print(f"Encrypted messages received: {self.messages_received}")
            print(f"Unique test IDs: {len(self.test_results)}")
            
            if self.test_results:
                print("\n📋 Received Encrypted Test Messages:")
                for test_id, result in self.test_results.items():
                    encryption_status = "🔐" if result.get('encrypted', False) else "❌"
                    print(f"   {encryption_status} {test_id}: {result['topic']}")
                    
                # Check expected encrypted message types
                expected_patterns = [
                    "encrypted/sensors/",
                    "encrypted/events/",
                    "encrypted/commands/"
                ]
                
                print("\n🔍 SSL Relay Pattern Analysis:")
                for pattern in expected_patterns:
                    matching = [r for r in self.test_results.values() if r['topic'].startswith(pattern)]
                    status = "🔐" if matching else "❌"
                    print(f"   {status} {pattern}*: {len(matching)} messages")
                    
                # Check encryption status
                encrypted_count = sum(1 for r in self.test_results.values() if r.get('encrypted', False))
                total_count = len(self.test_results)
                
                print(f"\n🔐 Encryption Analysis:")
                print(f"   Encrypted messages: {encrypted_count}/{total_count}")
                if encrypted_count == total_count:
                    print(f"   Status: ✅ All messages properly encrypted")
                else:
                    print(f"   Status: ❌ Some messages not encrypted")
                    
            else:
                print("\n❌ No encrypted messages received - Check:")
                print("   1. Is ssl_relay_client C binary running?")
                print("   2. Is the SSL publisher sending encrypted messages?")
                print("   3. Are SSL source and destination brokers running?")
                print("   4. Check SSL relay configuration file")
                print("   5. Verify SSL certificates are valid and accessible")
                
            print("=" * 60)
            
    def disconnect(self):
        """Disconnect from the SSL broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
def main():
    parser = argparse.ArgumentParser(description='SSL Relay Subscriber Test')
    parser.add_argument('--host', default='localhost', help='MQTT broker host')
    parser.add_argument('--port', type=int, default=1886, help='MQTT broker port')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--duration', '-d', type=int, help='Listen duration in seconds')
    
    args = parser.parse_args()
    
    subscriber = SSLRelaySubscriber(args.host, args.port, args.verbose)
    
    print("🔐 SSL Relay Subscriber Test")
    print("=" * 60)
    print(f"Target SSL broker: {args.host}:{args.port}")
    print(f"Purpose: Verify C ssl_relay_client binary encrypted forwarding")
    print("=" * 60)
    print()
    
    try:
        # Check SSL certificates first
        if not subscriber.check_ssl_certificates():
            return 1
            
        if not subscriber.setup_client():
            return 1
            
        if not subscriber.connect():
            print("❌ Failed to establish SSL connection to broker")
            return 1
            
        subscriber.listen(args.duration)
        
        return 0
        
    except Exception as e:
        print(f"❌ SSL Error: {e}")
        return 1
    finally:
        subscriber.print_summary()
        subscriber.disconnect()

if __name__ == "__main__":
    sys.exit(main())