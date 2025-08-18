#!/usr/bin/env python3
"""
DEPRECATED: Use run_relay_tests.py instead

This file has been replaced by the new modular relay testing system:
- basic_relay_publisher.py + basic_relay_subscriber.py  
- ssl_relay_publisher.py + ssl_relay_subscriber.py
- conditional_relay_publisher.py + conditional_relay_subscriber.py
- run_relay_tests.py (comprehensive test runner)

The new system separates publisher/subscriber concerns and provides better
testing of the C relay binaries.
"""
"""
Comprehensive Relay Test Suite

Runs all relay client verification tests in sequence:
1. Basic Relay Test
2. SSL Relay Test  
3. Conditional Relay Test

Provides consolidated results and overall system verification.
"""

import subprocess
import sys
import time
import os
from datetime import datetime

class RelayTestSuite:
    def __init__(self):
        self.test_scripts = [
            {
                'name': 'Basic Relay Test',
                'script': 'test_basic_relay.py',
                'description': 'Tests basic message forwarding between different ports'
            },
            {
                'name': 'SSL Relay Test',
                'script': 'test_ssl_relay.py', 
                'description': 'Tests SSL-secured message forwarding with encryption'
            },
            {
                'name': 'Conditional Relay Test',
                'script': 'test_conditional_relay.py',
                'description': 'Tests intelligent message filtering and conditional logic'
            }
        ]
        
        self.test_results = {}
        self.start_time = datetime.now()

    def check_dependencies(self):
        """Check if all required dependencies are available"""
        print("🔍 Checking dependencies...")
        
        # Check Python dependencies
        try:
            import paho.mqtt.client as mqtt
            print("✅ paho-mqtt library available")
        except ImportError:
            print("❌ paho-mqtt library not found. Install with: pip install paho-mqtt")
            return False
        
        # Check if relay executables exist
        executables = [
            'build/basic_relay_client',
            'build/ssl_relay_client', 
            'build/conditional_relay_client'
        ]
        
        for exe in executables:
            if os.path.exists(exe):
                print(f"✅ {exe} found")
            else:
                print(f"❌ {exe} not found. Run 'make all' to build relay clients")
                return False
        
        # Check if mosquitto is available
        try:
            result = subprocess.run(['mosquitto', '-h'], capture_output=True)
            print("✅ mosquitto broker available")
        except FileNotFoundError:
            print("❌ mosquitto broker not found. Install mosquitto")
            return False
        
        # Check SSL certificates for SSL test
        ssl_files = [
            '../ssl_certs/ca.crt',
            '../ssl_certs/client.crt',
            '../ssl_certs/client.key',
            '../ssl_certs/server.crt',
            '../ssl_certs/server.key'
        ]
        
        ssl_available = True
        for ssl_file in ssl_files:
            if os.path.exists(ssl_file):
                print(f"✅ {ssl_file} found")
            else:
                print(f"⚠️  {ssl_file} not found - SSL test may fail")
                ssl_available = False
        
        if ssl_available:
            print("✅ SSL certificates available for SSL relay test")
        else:
            print("⚠️  Some SSL certificates missing - SSL test will be attempted anyway")
        
        return True

    def run_test(self, test_info):
        """Run a single test script"""
        print(f"\n{'='*60}")
        print(f"  Running: {test_info['name']}")
        print(f"  {test_info['description']}")
        print(f"{'='*60}")
        
        start_time = time.time()
        
        try:
            result = subprocess.run([
                sys.executable, test_info['script']
            ], capture_output=True, text=True, timeout=180)  # 3 minute timeout
            
            end_time = time.time()
            duration = end_time - start_time
            
            success = result.returncode == 0
            
            self.test_results[test_info['name']] = {
                'success': success,
                'duration': duration,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'return_code': result.returncode
            }
            
            if success:
                print(f"✅ {test_info['name']} PASSED (Duration: {duration:.1f}s)")
            else:
                print(f"❌ {test_info['name']} FAILED (Duration: {duration:.1f}s)")
                print(f"   Return code: {result.returncode}")
                if result.stderr:
                    print(f"   Error output: {result.stderr[:200]}...")
            
            return success
            
        except subprocess.TimeoutExpired:
            print(f"⏰ {test_info['name']} TIMEOUT (exceeded 3 minutes)")
            self.test_results[test_info['name']] = {
                'success': False,
                'duration': 180.0,
                'stdout': '',
                'stderr': 'Test timeout after 3 minutes',
                'return_code': -1
            }
            return False
            
        except Exception as e:
            print(f"💥 {test_info['name']} ERROR: {e}")
            self.test_results[test_info['name']] = {
                'success': False,
                'duration': 0.0,
                'stdout': '',
                'stderr': str(e),
                'return_code': -2
            }
            return False

    def run_all_tests(self):
        """Run all relay tests in sequence"""
        print("🚀 Starting Comprehensive Relay Test Suite")
        print(f"📅 Started at: {self.start_time.strftime('%Y-%m-%d %H:%M:%S')}")
        
        # Check dependencies first
        if not self.check_dependencies():
            print("❌ Dependency check failed. Cannot proceed with tests.")
            return False
        
        print(f"\n📋 Running {len(self.test_scripts)} relay tests...")
        
        all_passed = True
        
        for i, test_info in enumerate(self.test_scripts, 1):
            print(f"\n🔄 Test {i}/{len(self.test_scripts)}: {test_info['name']}")
            
            success = self.run_test(test_info)
            if not success:
                all_passed = False
            
            # Small delay between tests to allow cleanup
            if i < len(self.test_scripts):
                print("⏳ Waiting 5 seconds before next test...")
                time.sleep(5)
        
        return all_passed

    def generate_test_report(self):
        """Generate comprehensive test report"""
        end_time = datetime.now()
        total_duration = (end_time - self.start_time).total_seconds()
        
        print(f"\n{'='*80}")
        print("  COMPREHENSIVE RELAY TEST SUITE REPORT")
        print(f"{'='*80}")
        
        print(f"📅 Test Suite Duration: {total_duration:.1f} seconds")
        print(f"🕐 Started: {self.start_time.strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"🕐 Ended: {end_time.strftime('%Y-%m-%d %H:%M:%S')}")
        
        # Overall results
        total_tests = len(self.test_results)
        passed_tests = sum(1 for result in self.test_results.values() if result['success'])
        failed_tests = total_tests - passed_tests
        
        print(f"\n📊 Overall Results:")
        print(f"   Total Tests: {total_tests}")
        print(f"   Passed: {passed_tests}")
        print(f"   Failed: {failed_tests}")
        print(f"   Success Rate: {(passed_tests/total_tests*100):.1f}%")
        
        # Individual test results
        print(f"\n🔍 Individual Test Results:")
        for test_name, result in self.test_results.items():
            status = "✅ PASSED" if result['success'] else "❌ FAILED"
            duration = result['duration']
            print(f"   {test_name}: {status} ({duration:.1f}s)")
            
            if not result['success'] and result['stderr']:
                print(f"      Error: {result['stderr'][:100]}...")
        
        # Feature verification summary
        print(f"\n🎯 Relay Features Verified:")
        if self.test_results.get('Basic Relay Test', {}).get('success', False):
            print("   ✅ Basic message forwarding between different ports")
            print("   ✅ Topic pattern matching with wildcards")
            print("   ✅ Bidirectional relay functionality")
        
        if self.test_results.get('SSL Relay Test', {}).get('success', False):
            print("   ✅ SSL/TLS encrypted message forwarding")
            print("   ✅ Certificate validation and secure handshake")
            print("   ✅ Encrypted broker-to-broker communication")
        
        if self.test_results.get('Conditional Relay Test', {}).get('success', False):
            print("   ✅ Priority-based message filtering")
            print("   ✅ Message type filtering (debug blocking)")
            print("   ✅ Timestamp-based message validation")
            print("   ✅ Conditional forwarding logic")
        
        # Recommendations
        print(f"\n💡 Recommendations:")
        if failed_tests == 0:
            print("   🎉 All relay clients are functioning correctly!")
            print("   🚀 System is ready for production deployment")
        else:
            print("   🔧 Review failed tests and check:")
            print("      - MQTT broker connectivity")
            print("      - SSL certificate validity")
            print("      - Network port availability")
            print("      - Relay client configuration files")
        
        print(f"\n{'='*80}")
        
        return passed_tests == total_tests

    def save_detailed_logs(self):
        """Save detailed test logs to files"""
        print("💾 Saving detailed test logs...")
        
        timestamp = self.start_time.strftime('%Y%m%d_%H%M%S')
        
        for test_name, result in self.test_results.items():
            safe_name = test_name.lower().replace(' ', '_')
            
            # Save stdout
            if result['stdout']:
                with open(f"relay_test_{safe_name}_{timestamp}.log", 'w') as f:
                    f.write(f"Test: {test_name}\n")
                    f.write(f"Success: {result['success']}\n")
                    f.write(f"Duration: {result['duration']:.1f}s\n")
                    f.write(f"Return Code: {result['return_code']}\n")
                    f.write("="*50 + "\n")
                    f.write(result['stdout'])
            
            # Save stderr if exists
            if result['stderr']:
                with open(f"relay_test_{safe_name}_{timestamp}.err", 'w') as f:
                    f.write(result['stderr'])
        
        print(f"✅ Test logs saved with timestamp: {timestamp}")

def main():
    """Main test suite execution"""
    test_suite = RelayTestSuite()
    
    try:
        # Run all tests
        success = test_suite.run_all_tests()
        
        # Generate report
        overall_success = test_suite.generate_test_report()
        
        # Save detailed logs
        test_suite.save_detailed_logs()
        
        return 0 if overall_success else 1
        
    except KeyboardInterrupt:
        print("\n🛑 Test suite interrupted by user")
        return 130
    except Exception as e:
        print(f"💥 Test suite failed with exception: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())