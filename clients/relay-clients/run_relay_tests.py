#!/usr/bin/env python3
"""
Comprehensive Relay Test Runner

Coordinates testing of all C relay binary types (basic, SSL, conditional) by running
publisher and subscriber scripts in proper sequence to verify relay functionality.

Usage: python3 run_relay_tests.py [--test-type TYPE] [--duration SECONDS]
"""

import subprocess
import time
import sys
import argparse
import os
import signal
import threading
from datetime import datetime

class RelayTestRunner:
    def __init__(self):
        self.test_processes = {}
        self.test_results = {}
        self.start_time = None
        
    def check_relay_binaries(self):
        """Check if C relay binaries exist"""
        relay_binaries = [
            'build/basic_relay_client',
            'build/ssl_relay_client', 
            'build/conditional_relay_client'
        ]
        
        missing_binaries = []
        for binary in relay_binaries:
            if not os.path.exists(binary):
                missing_binaries.append(binary)
                
        if missing_binaries:
            print("❌ Missing relay binaries:")
            for binary in missing_binaries:
                print(f"   {binary}")
            print("\nBuild the relay clients first: make all")
            return False
            
        print("✅ All relay binaries found")
        return True
        
    def check_python_scripts(self):
        """Check if Python test scripts exist"""
        required_scripts = [
            'basic_relay_publisher.py',
            'basic_relay_subscriber.py',
            'ssl_relay_publisher.py',
            'ssl_relay_subscriber.py',
            'conditional_relay_publisher.py',
            'conditional_relay_subscriber.py'
        ]
        
        missing_scripts = []
        for script in required_scripts:
            if not os.path.exists(script):
                missing_scripts.append(script)
                
        if missing_scripts:
            print("❌ Missing Python test scripts:")
            for script in missing_scripts:
                print(f"   {script}")
            return False
            
        print("✅ All Python test scripts found")
        return True
        
    def run_basic_relay_test(self, duration=30):
        """Run basic relay test with publisher and subscriber"""
        print("\n" + "=" * 60)
        print("🔄 BASIC RELAY TEST")
        print("=" * 60)
        
        # Start basic relay binary
        print("Starting basic_relay_client...")
        relay_process = subprocess.Popen([
            './build/basic_relay_client', 'build/basic_relay_config.json'
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        time.sleep(3)  # Let relay initialize
        
        try:
            # Start subscriber in background
            print("Starting basic relay subscriber...")
            subscriber_process = subprocess.Popen([
                'python3', 'basic_relay_subscriber.py', '--duration', str(duration)
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            
            time.sleep(2)  # Let subscriber connect
            
            # Run publisher
            print("Starting basic relay publisher...")
            publisher_process = subprocess.run([
                'python3', 'basic_relay_publisher.py'
            ], capture_output=True, text=True, timeout=60)
            
            # Wait for subscriber to finish
            subscriber_output, subscriber_error = subscriber_process.communicate(timeout=duration + 10)
            
            # Collect results
            self.test_results['basic'] = {
                'publisher_success': publisher_process.returncode == 0,
                'subscriber_success': subscriber_process.returncode == 0,
                'publisher_output': publisher_process.stdout,
                'subscriber_output': subscriber_output,
                'errors': publisher_process.stderr + subscriber_error
            }
            
            print("✅ Basic relay test completed")
            
        except Exception as e:
            print(f"❌ Basic relay test failed: {e}")
            self.test_results['basic'] = {'error': str(e)}
        finally:
            # Cleanup
            relay_process.terminate()
            try:
                subscriber_process.terminate()
            except:
                pass
                
    def run_ssl_relay_test(self, duration=30):
        """Run SSL relay test with publisher and subscriber"""
        print("\n" + "=" * 60)
        print("🔐 SSL RELAY TEST")
        print("=" * 60)
        
        # Check SSL certificates
        ssl_files = [
            "ssl_certs/ca.crt",
            "ssl_certs/client.crt", 
            "ssl_certs/client.key"
        ]
        
        for ssl_file in ssl_files:
            if not os.path.exists(ssl_file):
                print(f"❌ Missing SSL certificate: {ssl_file}")
                self.test_results['ssl'] = {'error': f'Missing SSL certificate: {ssl_file}'}
                return
                
        # Start SSL relay binary
        print("Starting ssl_relay_client...")
        relay_process = subprocess.Popen([
            './build/ssl_relay_client', 'build/ssl_relay_config.json'
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        time.sleep(5)  # SSL needs more time to initialize
        
        try:
            # Start subscriber in background
            print("Starting SSL relay subscriber...")
            subscriber_process = subprocess.Popen([
                'python3', 'ssl_relay_subscriber.py', '--duration', str(duration)
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            
            time.sleep(3)  # Let SSL subscriber connect
            
            # Run publisher
            print("Starting SSL relay publisher...")
            publisher_process = subprocess.run([
                'python3', 'ssl_relay_publisher.py'
            ], capture_output=True, text=True, timeout=90)
            
            # Wait for subscriber to finish
            subscriber_output, subscriber_error = subscriber_process.communicate(timeout=duration + 15)
            
            # Collect results
            self.test_results['ssl'] = {
                'publisher_success': publisher_process.returncode == 0,
                'subscriber_success': subscriber_process.returncode == 0,
                'publisher_output': publisher_process.stdout,
                'subscriber_output': subscriber_output,
                'errors': publisher_process.stderr + subscriber_error
            }
            
            print("✅ SSL relay test completed")
            
        except Exception as e:
            print(f"❌ SSL relay test failed: {e}")
            self.test_results['ssl'] = {'error': str(e)}
        finally:
            # Cleanup
            relay_process.terminate()
            try:
                subscriber_process.terminate()
            except:
                pass
                
    def run_conditional_relay_test(self, duration=40):
        """Run conditional relay test with publisher and subscriber"""
        print("\n" + "=" * 60)
        print("🧠 CONDITIONAL RELAY TEST")
        print("=" * 60)
        
        # Start conditional relay binary
        print("Starting conditional_relay_client...")
        relay_process = subprocess.Popen([
            './build/conditional_relay_client', 'build/conditional_relay_config.json'
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        time.sleep(3)  # Let relay initialize
        
        try:
            # Start subscriber in background
            print("Starting conditional relay subscriber...")
            subscriber_process = subprocess.Popen([
                'python3', 'conditional_relay_subscriber.py', '--duration', str(duration)
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            
            time.sleep(2)  # Let subscriber connect
            
            # Run publisher
            print("Starting conditional relay publisher...")
            publisher_process = subprocess.run([
                'python3', 'conditional_relay_publisher.py'
            ], capture_output=True, text=True, timeout=60)
            
            # Wait for subscriber to finish
            subscriber_output, subscriber_error = subscriber_process.communicate(timeout=duration + 10)
            
            # Collect results
            self.test_results['conditional'] = {
                'publisher_success': publisher_process.returncode == 0,
                'subscriber_success': subscriber_process.returncode == 0,
                'publisher_output': publisher_process.stdout,
                'subscriber_output': subscriber_output,
                'errors': publisher_process.stderr + subscriber_error
            }
            
            print("✅ Conditional relay test completed")
            
        except Exception as e:
            print(f"❌ Conditional relay test failed: {e}")
            self.test_results['conditional'] = {'error': str(e)}
        finally:
            # Cleanup
            relay_process.terminate()
            try:
                subscriber_process.terminate()
            except:
                pass
                
    def print_comprehensive_summary(self):
        """Print comprehensive test summary"""
        if not self.start_time:
            return
            
        duration = datetime.now() - self.start_time
        
        print("\n" + "=" * 80)
        print("📊 COMPREHENSIVE RELAY TEST SUMMARY")
        print("=" * 80)
        print(f"Total test duration: {duration}")
        print(f"Tests executed: {len(self.test_results)}")
        
        overall_success = True
        
        for test_type, results in self.test_results.items():
            print(f"\n🔍 {test_type.upper()} RELAY RESULTS:")
            
            if 'error' in results:
                print(f"   ❌ Test failed: {results['error']}")
                overall_success = False
            else:
                pub_status = "✅" if results.get('publisher_success', False) else "❌"
                sub_status = "✅" if results.get('subscriber_success', False) else "❌"
                
                print(f"   Publisher: {pub_status}")
                print(f"   Subscriber: {sub_status}")
                
                if not (results.get('publisher_success', False) and results.get('subscriber_success', False)):
                    overall_success = False
                    
                # Show key output snippets
                if results.get('subscriber_output'):
                    lines = results['subscriber_output'].split('\n')
                    message_lines = [l for l in lines if 'MESSAGE RECEIVED' in l or 'SUMMARY' in l]
                    if message_lines:
                        print(f"   Key results: {len(message_lines)} message events")
                        
                if results.get('errors'):
                    error_lines = [l for l in results['errors'].split('\n') if l.strip() and '❌' in l]
                    if error_lines:
                        print(f"   Errors found: {len(error_lines)}")
                        
        print(f"\n🎯 OVERALL RESULT: {'✅ ALL TESTS PASSED' if overall_success else '❌ SOME TESTS FAILED'}")
        
        if overall_success:
            print("\n🎉 All C relay binaries are functioning correctly!")
            print("   - Basic relay: Message forwarding verified")
            print("   - SSL relay: Encrypted message forwarding verified") 
            print("   - Conditional relay: Filtering logic verified")
        else:
            print("\n🔧 Some relay binaries need investigation:")
            for test_type, results in self.test_results.items():
                if 'error' in results or not (results.get('publisher_success', False) and results.get('subscriber_success', False)):
                    print(f"   - {test_type.upper()} relay: Check binary and configuration")
                    
        print("\n📁 Detailed logs available in test outputs above")
        print("=" * 80)
        
    def run_all_tests(self, duration=30):
        """Run all relay tests in sequence"""
        self.start_time = datetime.now()
        
        print("🚀 COMPREHENSIVE RELAY BINARY TESTING")
        print("=" * 80)
        print("Testing C relay binaries with Python publisher/subscriber verification")
        print(f"Test duration per relay type: {duration} seconds")
        print("=" * 80)
        
        # Run tests in sequence
        test_methods = [
            ('basic', self.run_basic_relay_test),
            ('ssl', self.run_ssl_relay_test), 
            ('conditional', self.run_conditional_relay_test)
        ]
        
        for test_name, test_method in test_methods:
            try:
                test_method(duration)
                time.sleep(2)  # Brief pause between tests
            except KeyboardInterrupt:
                print(f"\n🛑 Test interrupted during {test_name} relay test")
                break
            except Exception as e:
                print(f"❌ Unexpected error in {test_name} test: {e}")
                self.test_results[test_name] = {'error': str(e)}
                
        self.print_comprehensive_summary()
        
def signal_handler(sig, frame):
    print("\n🛑 Test suite interrupted by user")
    sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description='Comprehensive Relay Test Runner')
    parser.add_argument('--test-type', choices=['basic', 'ssl', 'conditional', 'all'], 
                        default='all', help='Type of relay test to run')
    parser.add_argument('--duration', type=int, default=30, 
                        help='Duration for each subscriber test (seconds)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    signal.signal(signal.SIGINT, signal_handler)
    
    runner = RelayTestRunner()
    
    # Pre-flight checks
    if not runner.check_relay_binaries():
        return 1
        
    if not runner.check_python_scripts():
        return 1
        
    try:
        if args.test_type == 'all':
            runner.run_all_tests(args.duration)
        elif args.test_type == 'basic':
            runner.start_time = datetime.now()
            runner.run_basic_relay_test(args.duration)
            runner.print_comprehensive_summary()
        elif args.test_type == 'ssl':
            runner.start_time = datetime.now()
            runner.run_ssl_relay_test(args.duration)
            runner.print_comprehensive_summary()
        elif args.test_type == 'conditional':
            runner.start_time = datetime.now()
            runner.run_conditional_relay_test(args.duration)
            runner.print_comprehensive_summary()
            
        # Determine exit code based on results
        all_passed = all(
            not ('error' in results or not (results.get('publisher_success', False) and results.get('subscriber_success', False)))
            for results in runner.test_results.values()
        )
        
        return 0 if all_passed else 1
        
    except Exception as e:
        print(f"❌ Test runner failed: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())