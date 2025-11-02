#!/usr/bin/env python3
"""
Simple test script for Pulse USB Console Interface

Usage: python test_shell.py COM3
"""

import serial
import sys
import time

def send_command(ser, cmd, timeout=3.0):
    """Send a command and read response"""
    print(f"\n→ {cmd}")
    
    # Clear any pending input
    if ser.in_waiting:
        ser.read(ser.in_waiting)
    
    # Send command with newline
    ser.write((cmd + '\r\n').encode())
    
    # Wait and read response with timeout
    response = []
    start_time = time.time()
    last_data_time = start_time
    
    while (time.time() - start_time) < timeout:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
            text = chunk.decode('utf-8', errors='ignore')
            response.append(text)
            last_data_time = time.time()
        
        # If we got data and nothing new for 500ms, we're done
        if response and (time.time() - last_data_time) > 0.5:
            break
        
        time.sleep(0.05)
    
    # Print response
    full_response = ''.join(response)
    if full_response:
        print(full_response)
    
    return full_response

def main():
    if len(sys.argv) < 2:
        print("Usage: python test_shell.py <PORT>")
        print("Example: python test_shell.py COM3")
        sys.exit(1)
    
    port = sys.argv[1]
    
    print("="*60)
    print("Pulse USB Console Test Script")
    print("="*60)
    print(f"Connecting to {port}...")
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)
        
        # Clear any initial data
        ser.reset_input_buffer()
        print("✓ Connected\n")
        
        # Test commands
        print("Testing Console Commands")
        print("="*60)
        
        send_command(ser, "STATUS")
        time.sleep(0.5)
        # send_command(ser, "RESET")
        
        print("\n" + "="*60)
        print("Interactive Mode")
        print("="*60)
        print("Available commands:")
        print("  STATUS - Show device status and ML state")
        print("  RESET  - Reset ML model for retraining")
        print("  quit   - Exit this script")
        print("="*60 + "\n")
        
        while True:
            try:
                cmd = input("> ").strip()
                
                if cmd.lower() in ['quit', 'exit', 'q']:
                    break
                
                if not cmd:
                    continue
                
                # Convert to uppercase for device
                send_command(ser, cmd.upper(), timeout=5.0)
                
            except KeyboardInterrupt:
                print("\n\nExiting...")
                break
            except Exception as e:
                print(f"Error: {e}")
        
        ser.close()
        print("\n✓ Disconnected")
        
    except serial.SerialException as e:
        print(f"✗ Error: {e}")
        print("\nTroubleshooting:")
        print("1. Check that device is connected")
        print("2. Verify correct COM port")
        print("3. Close other programs using the port")
        sys.exit(1)

if __name__ == '__main__':
    main()
