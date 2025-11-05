#!/usr/bin/env python3
"""
UDP Client-Server Performance Test Runner
Runs multiple iterations of UDP file transfer tests and calculates statistics
"""

import subprocess
import time
import re
import signal
import os
import sys
from statistics import mean, stdev

class UDPTestRunner:
    def __init__(self, server_path="./udp_ser4", client_path="./udp_client4", server_host="localhost"):
        self.server_path = server_path
        self.client_path = client_path
        self.server_host = server_host
        self.results = []
        
    def cleanup_processes(self):
        """Kill any existing server processes"""
        try:
            subprocess.run(["pkill", "-f", "udp_ser4"], stderr=subprocess.DEVNULL)
            time.sleep(0.5)
        except:
            pass
    
    def parse_client_output(self, output):
        """Extract timing and throughput data from client output"""
        try:
            # Look for time and data rate in client output
            time_match = re.search(r'Time\(ms\)\s*:\s*([0-9.]+)', output)
            rate_match = re.search(r'Data rate:\s*([0-9.]+)', output)
            data_match = re.search(r'Data sent\(byte\):\s*([0-9]+)', output)
            
            if time_match and rate_match and data_match:
                time_ms = float(time_match.group(1))
                data_rate = float(rate_match.group(1))  # KB/s
                data_sent = int(data_match.group(1))
                
                return {
                    'time_ms': time_ms,
                    'data_rate_kbps': data_rate,
                    'data_sent_bytes': data_sent,
                    'data_rate_mbps': data_rate / 1024.0  # Convert to MB/s
                }
        except Exception as e:
            print(f"Error parsing output: {e}")
            
        return None
    
    def run_single_test(self, iteration):
        """Run a single client-server test"""
        print(f"\n--- Test {iteration + 1}/3 ---")
        
        # Cleanup any existing processes
        self.cleanup_processes()
        time.sleep(1)
        
        # Start server
        print("Starting server...")
        server_process = subprocess.Popen(
            [self.server_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Give server time to start
        time.sleep(2)
        
        try:
            # Run client
            print("Running client...")
            client_result = subprocess.run(
                [self.client_path, self.server_host],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=300  # 30 second timeout
            )
            
            # Wait a moment for server to fi    nish
            time.sleep(1)
            
            # Get server output
            server_output = ""
            if server_process.poll() is None:
                try:
                    server_output, _ = server_process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    server_process.kill()
                    server_output, _ = server_process.communicate()
            
            # Parse results
            result = self.parse_client_output(client_result.stdout)
            
            if result:
                print(f"✓ Time: {result['time_ms']:.3f} ms")
                print(f"✓ Throughput: {result['data_rate_mbps']:.2f} MB/s ({result['data_rate_kbps']:.1f} KB/s)")
                print(f"✓ Data sent: {result['data_sent_bytes']} bytes")
                
                return result
            else:
                print("✗ Failed to parse client output")
                print(f"Client stdout: {client_result.stdout}")
                print(f"Client stderr: {client_result.stderr}")
            
        except subprocess.TimeoutExpired:
            print("✗ Test timed out")
            server_process.kill()
        except Exception as e:
            print(f"✗ Test failed: {e}")
            
        finally:
            # Ensure server is terminated
            if server_process.poll() is None:
                server_process.terminate()
                try:
                    server_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    server_process.kill()
        
        return None
    
    def run_tests(self, num_iterations=10, wait_between_tests=5):
        """Run multiple test iterations"""
        print(f"Starting UDP Performance Tests ({num_iterations} iterations)")
        print(f"Server: {self.server_path}")
        print(f"Client: {self.client_path} {self.server_host}")
        print(f"Wait between tests: {wait_between_tests} seconds")
        print("=" * 60)
        
        successful_tests = 0
        
        for i in range(num_iterations):
            result = self.run_single_test(i)
            
            if result:
                self.results.append(result)
                successful_tests += 1
            else:
                print("✗ Test failed - skipping this iteration")
            
            # Wait between tests (except after last test)
            if i < num_iterations - 1:
                print(f"\nWaiting {wait_between_tests} seconds before next test...")
                time.sleep(wait_between_tests)
        
        print(f"\n{'=' * 60}")
        print(f"Completed {successful_tests}/{num_iterations} successful tests")
        
        return successful_tests > 0
    
    def calculate_statistics(self):
        """Calculate and display statistics from all test results"""
        if not self.results:
            print("No successful test results to analyze")
            return
        
        # Extract data
        times = [r['time_ms'] for r in self.results]
        throughput_mbps = [r['data_rate_mbps'] for r in self.results]
        throughput_kbps = [r['data_rate_kbps'] for r in self.results]
        data_sizes = [r['data_sent_bytes'] for r in self.results]
        
        print(f"\n{'=' * 60}")
        print("PERFORMANCE STATISTICS")
        print(f"{'=' * 60}")
        print(f"Number of successful tests: {len(self.results)}")
        print(f"File size: {data_sizes[0]} bytes ({data_sizes[0]/1024:.1f} KB)")
        
        print(f"\nTRANSMISSION TIME:")
        print(f"  Average: {mean(times):.3f} ms")
        print(f"  Min:     {min(times):.3f} ms")
        print(f"  Max:     {max(times):.3f} ms")
        if len(times) > 1:
            print(f"  Std Dev: {stdev(times):.3f} ms")
            print(f"  Variance: {(stdev(times)/mean(times)*100):.1f}%")
        
        print(f"\nTHROUGHPUT (MB/s):")
        print(f"  Average: {mean(throughput_mbps):.2f} MB/s")
        print(f"  Min:     {min(throughput_mbps):.2f} MB/s")
        print(f"  Max:     {max(throughput_mbps):.2f} MB/s")
        if len(throughput_mbps) > 1:
            print(f"  Std Dev: {stdev(throughput_mbps):.2f} MB/s")
            print(f"  Variance: {(stdev(throughput_mbps)/mean(throughput_mbps)*100):.1f}%")
        
        print(f"\nTHROUGHPUT (KB/s):")
        print(f"  Average: {mean(throughput_kbps):.1f} KB/s")
        print(f"  Min:     {min(throughput_kbps):.1f} KB/s")
        print(f"  Max:     {max(throughput_kbps):.1f} KB/s")
        
        # Performance consistency analysis
        time_variance = stdev(times)/mean(times)*100 if len(times) > 1 else 0
        throughput_variance = stdev(throughput_mbps)/mean(throughput_mbps)*100 if len(throughput_mbps) > 1 else 0
        
        print(f"\nPERFORMANCE CONSISTENCY:")
        if time_variance < 10:
            print(f"  ✓ Excellent timing consistency ({time_variance:.1f}% variance)")
        elif time_variance < 25:
            print(f"  ⚠ Good timing consistency ({time_variance:.1f}% variance)")
        else:
            print(f"  ✗ Poor timing consistency ({time_variance:.1f}% variance)")
        
        if throughput_variance < 10:
            print(f"  ✓ Excellent throughput consistency ({throughput_variance:.1f}% variance)")
        elif throughput_variance < 25:
            print(f"  ⚠ Good throughput consistency ({throughput_variance:.1f}% variance)")
        else:
            print(f"  ✗ Poor throughput consistency ({throughput_variance:.1f}% variance)")

def main():
    """Main function"""
    # Change to Ex4 directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    ex4_dir = script_dir if script_dir.endswith('Ex4') else os.path.join(script_dir, 'Ex4')
    
    print(f"Changing working directory to: {ex4_dir}")
    
    try:
        os.chdir(ex4_dir)
    except FileNotFoundError:
        print(f"Error: Directory {ex4_dir} not found")
        sys.exit(1)
    
    print(f"Current working directory: {os.getcwd()}")
    
    # Check if source files exist
    if not os.path.exists("udp_ser4.c"):
        print("Error: udp_ser4.c source file not found")
        sys.exit(1)
    
    if not os.path.exists("udp_client4.c"):
        print("Error: udp_client4.c source file not found")
        sys.exit(1)
    
    # Compile both programs before running tests
    print("Compiling UDP programs...")
    
    # Compile server
    print("  Compiling server (udp_ser4)...")
    server_compile = subprocess.run(
        ["gcc", "udp_ser4single.c", "-o", "udp_ser4"],
        capture_output=True,
        text=True
    )
    
    if server_compile.returncode != 0:
        print("✗ Server compilation failed:")
        print(server_compile.stderr)
        sys.exit(1)
    else:
        print("  ✓ Server compiled successfully")
    
    # Compile client
    print("  Compiling client (udp_client4)...")
    client_compile = subprocess.run(
        ["gcc", "udp_client4single.c", "-o", "udp_client4"],
        capture_output=True,
        text=True
    )
    
    if client_compile.returncode != 0:
        print("✗ Client compilation failed:")
        print(client_compile.stderr)
        sys.exit(1)
    else:
        print("  ✓ Client compiled successfully")
    
    print("✓ All programs compiled successfully\n")
    
    # Check if test file exists
    if not os.path.exists("myfile.txt"):
        print("Error: myfile.txt not found. Please ensure the test file exists.")
        sys.exit(1)
    
    # Create test runner
    runner = UDPTestRunner()
    
    try:
        # Run tests
        success = runner.run_tests(num_iterations=3, wait_between_tests=5)
        
        if success:
            # Calculate and display statistics
            runner.calculate_statistics()
        else:
            print("All tests failed. Please check your UDP implementation.")
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        runner.cleanup_processes()
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        runner.cleanup_processes()
    finally:
        # Final cleanup
        runner.cleanup_processes()

if __name__ == "__main__":
    main()