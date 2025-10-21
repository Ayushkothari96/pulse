#!/usr/bin/env python3
"""
Simple test script for Pulse USB Shell Interface

Usage: python test_shell.py COM3
"""

import serial
import sys
import time

def send_command(ser, cmd, timeout=2.0):
    """Send a command and read response"""
    print(f"\n→ {cmd}")
    
    # Clear any pending input
    if ser.in_waiting:
        ser.read(ser.in_waiting)
    
    # Send command
    ser.write((cmd + '\n').encode())
    
    # Wait and read response with timeout
    response = []
    start_time = time.time()
    last_data_time = start_time
    
    while (time.time() - start_time) < timeout:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
            lines = chunk.decode('utf-8', errors='ignore').split('\n')
            response.extend(lines)
            last_data_time = time.time()
        
        # If we got data and nothing new for 200ms, we're done
        if response and (time.time() - last_data_time) > 0.2:
            break
        
        time.sleep(0.01)
    
    # Filter and print response
    filtered = []
    for line in response:
        line = line.strip()
        # Skip empty lines, echoed command, and prompts
        if line and not line.startswith('pulse:~$') and line != cmd:
            filtered.append(line)
            print(f"  {line}")
    
    return filtered

def main():
    if len(sys.argv) < 2:
        print("Usage: python test_shell.py <PORT>")
        print("Example: python test_shell.py COM3")
        sys.exit(1)
    
    port = sys.argv[1]
    
    print("="*60)
    print("Pulse USB Shell Test Script")
    print("="*60)
    print(f"Connecting to {port}...")
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)
        
        # Clear any initial data
        ser.reset_input_buffer()
        print("✓ Connected\n")
        
        # Test commands
        print("Testing Shell Commands")
        print("="*60)
        
        send_command(ser, "device")
        send_command(ser, "accel")
        send_command(ser, "ml state")
        send_command(ser, "ml info")
        
        print("\n" + "="*60)
        print("Interactive Mode")
        print("="*60)
        print("Enter commands (or 'quit' to exit):")
        print("Note: Response may include log messages from device\n")
        
        while True:
            try:
                cmd = input("pulse:~$ ").strip()
                
                if cmd.lower() in ['quit', 'exit', 'q']:
                    break
                
                if not cmd:
                    continue
                
                send_command(ser, cmd, timeout=3.0)
                
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
