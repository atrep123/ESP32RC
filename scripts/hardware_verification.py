#!/usr/bin/env python3
"""
ESP32RC Hardware Verification Script

Interactive script to help verify hardware functionality via serial connection.
Provides automated and manual verification tests for bring-up.

Usage:
    python scripts/hardware_verification.py <COMPORT> [baudrate]
    
Example:
    python scripts/hardware_verification.py COM3 115200
"""

import serial
import time
import sys
from pathlib import Path


class HardwareVerification:
    """Hardware verification helper for ESP32RC"""
    
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.connected = False
        
    def connect(self):
        """Connect to the serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=2)
            time.sleep(1)  # Wait for ESP32 to stabilize
            self.connected = True
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from serial port"""
        if self.ser:
            self.ser.close()
            self.connected = False
    
    def send_command(self, cmd):
        """Send a command and read response"""
        if not self.connected:
            print("Not connected")
            return None
        
        self.ser.write((cmd + "\n").encode())
        time.sleep(0.1)
        
        response = ""
        while self.ser.in_waiting:
            response += self.ser.read(1).decode(errors='ignore')
            time.sleep(0.01)
        
        return response
    
    def read_until_timeout(self, timeout=2):
        """Read all available data until timeout"""
        response = ""
        start = time.time()
        
        while time.time() - start < timeout:
            if self.ser.in_waiting:
                response += self.ser.read(1).decode(errors='ignore')
                start = time.time()  # Reset timeout on new data
            else:
                time.sleep(0.01)
        
        return response
    
    def verify_firmware_running(self):
        """Verify that firmware boots correctly"""
        print("\n--- Firmware Boot Test ---")
        self.ser.flush()
        
        # Restart and capture boot output
        output = self.read_until_timeout(3)
        
        if "ESP32RC" in output:
            print("✓ Firmware booted successfully")
            if "Initialization complete" in output:
                print("✓ Initialization completed")
                return True
        else:
            print("✗ No ESP32RC boot message detected")
            print(f"  Received: {output[:100]}")
            return False
        
        return True
    
    def check_system_status(self):
        """Check current system status"""
        print("\n--- System Status ---")
        response = self.send_command("status")
        
        if response:
            print(response)
            
            checks = [
                ("System: ENABLED" if "System: ENABLED" in response else "System: DISABLED", "System status"),
                ("RC Link:" in response, "RC Link detection"),
            ]
            
            for check, desc in checks:
                if check:
                    print(f"✓ {desc}")
                else:
                    print(f"✗ {desc}")
            
            return True
        else:
            print("✗ No response to status command")
            return False
    
    def check_sensor_availability(self):
        """Check if INA219 sensor is available"""
        print("\n--- Sensor Availability ---")
        response = self.send_command("sensor")
        
        if "INA219 not available" in response:
            print("⚠ INA219 sensor not detected")
            return False
        elif "Bus voltage:" in response:
            print("✓ INA219 sensor detected and responding")
            
            # Extract some values
            lines = response.split("\n")
            for line in lines:
                if "Bus voltage:" in line or "Current:" in line or "Power:" in line:
                    print(f"  {line.strip()}")
            
            return True
        else:
            print("⚠ Unknown sensor response")
            print(f"  Response: {response[:200]}")
            return False
    
    def test_self_test_mode(self):
        """Test self-test mode if available"""
        print("\n--- Self-Test Mode ---")
        
        # Check if self-test is available
        response = self.send_command("help")
        
        if "selftest" not in response:
            print("⚠ Self-test not available (not compiled with ENABLE_SERIAL_SELF_TEST)")
            return True  # Not a failure, just not available
        
        # Try to enable self-test
        print("Enabling self-test mode...")
        response = self.send_command("selftest on")
        
        if "Self-test enabled" in response:
            print("✓ Self-test mode enabled")
            
            # Try some self-test commands
            print("\nTesting power output...")
            self.send_command("pwm 10")
            time.sleep(0.2)
            
            response = self.send_command("status")
            if "Self-test: ACTIVE" in response:
                print("✓ PWM command accepted")
            
            # Disable self-test
            self.send_command("selftest off")
            print("✓ Self-test disabled")
            
            return True
        else:
            print("✗ Failed to enable self-test")
            return False
    
    def run_verification_suite(self):
        """Run complete verification suite"""
        print("=" * 60)
        print("ESP32RC Hardware Verification")
        print("=" * 60)
        
        if not self.connect():
            return False
        
        results = []
        
        try:
            results.append(("Firmware running", self.verify_firmware_running()))
            results.append(("System status", self.check_system_status()))
            results.append(("Sensor check", self.check_sensor_availability()))
            results.append(("Self-test mode", self.test_self_test_mode()))
            
        except Exception as e:
            print(f"\n✗ Error during verification: {e}")
            return False
        
        finally:
            self.disconnect()
        
        # Summary
        print("\n" + "=" * 60)
        print("Verification Summary")
        print("=" * 60)
        
        passed = sum(1 for _, result in results if result)
        total = len(results)
        
        for name, result in results:
            status = "✓ PASS" if result else "✗ FAIL"
            print(f"{status}: {name}")
        
        print(f"\nTotal: {passed}/{total} tests passed")
        
        return passed == total


def main():
    if len(sys.argv) < 2:
        print("Usage: python hardware_verification.py <COMPORT> [baudrate]")
        print("Example: python hardware_verification.py COM3 115200")
        sys.exit(1)
    
    port = sys.argv[1]
    baudrate = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    
    verifier = HardwareVerification(port, baudrate)
    success = verifier.run_verification_suite()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
