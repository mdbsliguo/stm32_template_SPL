#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Chinese Font Upload Tool
# Upload Chinese font file to STM32 via UART
#
# Usage:
#     python send_chinese_font.py [COM port] [font file] [baudrate]
#
# Example:
#     python send_chinese_font.py COM4 chinese16x16.bin 115200
#
# Dependencies:
#     pip install pyserial

import serial
import time
import sys
import os

# Protocol definitions
CMD_START = 0xAA
CMD_DATA = 0xBB
CMD_END = 0xCC
CMD_ACK = 0xDD
CMD_CHINESE = ord('C')  # ÖÐÎÄ×Ö¿âÃüÁî
CHUNK_SIZE = 256

def send_font_file(port, baudrate, font_file):
    """Send font file to STM32"""
    
    if not os.path.exists(font_file):
        print(f"Error: File not found: {font_file}")
        return False
    
    file_size = os.path.getsize(font_file)
    print(f"Font file: {font_file}")
    print(f"File size: {file_size} bytes ({file_size/1024:.2f} KB)")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=3)
        print(f"Connected to {port}, baudrate {baudrate}")
        
        # Clear any pending data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Send 'C' command for Chinese font
        print("Sending 'C' command (Chinese font)...")
        ser.write(bytes([CMD_CHINESE]))
        ser.flush()
        
        # Wait for "OK" response
        print("Waiting for 'OK' response...")
        response = ser.read(4)  # Read "OK\r\n"
        if len(response) < 4:
            print("Warning: Did not receive 'OK' response, but continuing...")
        else:
            response_str = response.decode('ascii', errors='ignore')
            if "OK" in response_str:
                print("Received 'OK', starting upload...")
            else:
                print(f"Warning: Unexpected response: {response_str}")
        
        # Clear buffer before sending commands
        time.sleep(0.5)
        ser.reset_input_buffer()
        
        # Step 1: Send start command
        max_retries = 50
        retry_count = 0
        ack_received = False
        
        print("Sending start command (CMD_START=0xAA)...")
        while retry_count < max_retries and not ack_received:
            ser.reset_input_buffer()
            time.sleep(0.1)
            
            ser.write(bytes([CMD_START]))
            ser.flush()
            time.sleep(0.05)
            
            ser.write(file_size.to_bytes(4, 'little'))
            ser.flush()
            time.sleep(0.2)
            
            ack = ser.read(10)
            if len(ack) > 0:
                for byte in ack:
                    if byte == CMD_ACK:
                        ack_received = True
                        print("ACK received! Starting data transfer...")
                        break
                
                if not ack_received:
                    if retry_count % 5 == 0:
                        hex_bytes = ' '.join([f'0x{b:02X}' for b in ack[:5]])
                        print(f"Received: {hex_bytes} (waiting for 0x{CMD_ACK:02X})...")
            else:
                retry_count += 1
                if retry_count % 5 == 0:
                    print(f"Waiting for STM32 ACK... (attempt {retry_count}/{max_retries})")
                time.sleep(0.2)
        
        if not ack_received:
            print(f"Error: No ACK received after {max_retries} attempts")
            ser.close()
            return False
        
        # Step 2: Send data in chunks
        with open(font_file, 'rb') as f:
            total_sent = 0
            while total_sent < file_size:
                chunk = f.read(CHUNK_SIZE)
                if len(chunk) == 0:
                    break
                
                ser.write(bytes([CMD_DATA]))
                time.sleep(0.01)
                
                packet_size = len(chunk)
                ser.write(packet_size.to_bytes(2, 'little'))
                time.sleep(0.01)
                
                ser.write(chunk)
                time.sleep(0.05)
                
                ack = ser.read(1)
                if len(ack) == 0 or ack[0] != CMD_ACK:
                    print(f"\nError: No ACK at {total_sent} bytes")
                    ser.close()
                    return False
                
                total_sent += len(chunk)
                progress = (total_sent / file_size) * 100
                print(f"\rProgress: {total_sent}/{file_size} bytes ({progress:.1f}%)", end='', flush=True)
        
        print()
        
        # Step 3: Send end command
        ser.write(bytes([CMD_END]))
        time.sleep(0.1)
        
        ser.close()
        print("Transfer complete!")
        return True
        
    except serial.SerialException as e:
        print(f"Serial port error: {e}")
        return False
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    default_port = "COM4"
    default_font_file = "chinese16x16.bin"
    default_baudrate = 115200
    
    if len(sys.argv) >= 3:
        port = sys.argv[1]
        font_file = sys.argv[2]
        baudrate = int(sys.argv[3]) if len(sys.argv) > 3 else default_baudrate
    elif len(sys.argv) == 2:
        port = sys.argv[1]
        font_file = default_font_file
        baudrate = default_baudrate
    else:
        port = default_port
        font_file = default_font_file
        baudrate = default_baudrate
        print(f"Using default parameters:")
        print(f"  Port: {port}")
        print(f"  Font file: {font_file}")
        print(f"  Baudrate: {baudrate}")
        print(f"\nTo customize, use: python send_chinese_font.py <COM port> <font file> [baudrate]")
        print()
    
    if not os.path.isabs(font_file) and not os.path.exists(font_file):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        font_file_path = os.path.join(script_dir, font_file)
        if os.path.exists(font_file_path):
            font_file = font_file_path
    
    success = send_font_file(port, baudrate, font_file)
    if success:
        print("\nUpload completed successfully!")
    else:
        print("\nUpload failed!")
    
    print("\nPress Enter to exit...")
    input()
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()

