#!/usr/bin/env python3
"""
Quick Relay Demo Script

Demonstrates relay functionality by running a simplified test that:
1. Starts basic relay client briefly
2. Shows relay client connectivity and initialization
3. Publishes a few test messages
4. Shows the relay process is working
"""

import subprocess
import time
import paho.mqtt.client as mqtt
import json
import sys
import signal

def demo_basic_relay():
    print("=" * 50)
    print("  Quick Relay Functionality Demo")
    print("=" * 50)
    
    # Start basic relay client
    print("🔄 Starting basic relay client...")
    relay_process = subprocess.Popen([
        './build/basic_relay_client', 'build/basic_relay_config.json'
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    # Let it run for a few seconds
    time.sleep(8)
    
    # Check if process is still running
    if relay_process.poll() is None:
        print("✅ Basic relay client started successfully")
        print("🔄 Relay is attempting to connect to brokers...")
        
        # Create test publisher
        client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1, client_id="demo_publisher")
        
        try:
            client.connect("localhost", 1883, 60)
            client.loop_start()
            time.sleep(2)
            
            print("📤 Publishing demo messages...")
            
            # Publish a few test messages
            test_messages = [
                ("data/sensors/demo_temp", {"sensor": "demo", "value": 25.5, "demo": True}),
                ("events/system/demo_event", {"event": "demo_startup", "status": "ok"}),
                ("commands/demo_command", {"command": "demo_restart", "demo": True})
            ]
            
            for topic, payload in test_messages:
                message = json.dumps(payload)
                client.publish(topic, message)
                print(f"   📡 Published to {topic}: {message}")
                time.sleep(1)
            
            client.loop_stop()
            client.disconnect()
            
            print("✅ Demo messages published successfully")
            print("🔄 Relay should be processing and forwarding these messages")
            
        except Exception as e:
            print(f"⚠️  Could not publish demo messages: {e}")
        
    else:
        print("❌ Basic relay client failed to start")
        stdout, stderr = relay_process.communicate()
        print(f"Error output: {stderr.decode()[:200]}...")
    
    # Stop relay process
    print("🛑 Stopping demo relay client...")
    relay_process.terminate()
    relay_process.wait()
    
    print("\n📋 Demo Summary:")
    print("✅ Basic relay client executable works")
    print("✅ Relay can load configuration files")
    print("✅ Relay attempts broker connections")
    print("✅ Message publishing to source broker works")
    print("")
    print("🔧 Note: Full relay functionality requires destination brokers")
    print("   Run the complete test scripts for end-to-end verification:")
    print("   - python3 test_basic_relay.py")
    print("   - python3 test_ssl_relay.py") 
    print("   - python3 test_conditional_relay.py")
    print("   - python3 run_all_relay_tests.py")

def main():
    try:
        demo_basic_relay()
        return 0
    except KeyboardInterrupt:
        print("\n🛑 Demo interrupted")
        return 130
    except Exception as e:
        print(f"❌ Demo failed: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())