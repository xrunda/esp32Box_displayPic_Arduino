#!/usr/bin/env python3
"""
Find ESP32-S3-Box3 device on local network by scanning for HTTP server
Usage: python find_device.py [image_file]
"""

import socket
import requests
import sys
import ipaddress
import subprocess

def get_local_network():
    """Get local network IP range"""
    try:
        # Get default gateway interface
        result = subprocess.run(['route', '-n', 'get', 'default'], 
                              capture_output=True, text=True, timeout=2)
        if result.returncode == 0:
            for line in result.stdout.split('\n'):
                if 'interface:' in line:
                    interface = line.split()[-1]
                    # Get IP address of this interface
                    result2 = subprocess.run(['ifconfig', interface], 
                                           capture_output=True, text=True, timeout=2)
                    if result2.returncode == 0:
                        for line2 in result2.stdout.split('\n'):
                            if 'inet ' in line2:
                                ip_str = line2.split()[1]
                                ip = ipaddress.IPv4Address(ip_str)
                                # Assume /24 subnet
                                network = ipaddress.IPv4Network(f"{ip_str}/24", strict=False)
                                return network
    except:
        pass
    
    # Fallback: try common local network ranges
    return ipaddress.IPv4Network('192.168.1.0/24', strict=False)

def scan_for_device(network, timeout=1):
    """Scan network for ESP32 device"""
    print(f"Scanning network {network} for ESP32 device...")
    print("This may take a minute...")
    
    found_devices = []
    
    for ip in network.hosts():
        ip_str = str(ip)
        try:
            # Try to connect to HTTP server
            url = f"http://{ip_str}/status"
            response = requests.get(url, timeout=timeout)
            if response.status_code == 200:
                print(f"\n✓ Found device at {ip_str}!")
                print(f"  Status: {response.text}")
                found_devices.append(ip_str)
        except:
            pass
        
        # Print progress
        if int(ip) % 50 == 0:
            print(f"  Scanning... {ip_str}", end='\r')
    
    return found_devices

def main():
    image_file = sys.argv[1] if len(sys.argv) > 1 else None
    
    # Get local network
    network = get_local_network()
    print(f"Local network: {network}")
    
    # Scan for device
    devices = scan_for_device(network)
    
    if not devices:
        print("\n✗ No device found on network")
        print("\nPlease check:")
        print("  1. Device is connected to WiFi")
        print("  2. Device and computer are on the same network")
        print("  3. Check serial monitor for device IP address")
        return
    
    device_ip = devices[0]
    print(f"\nUsing device IP: {device_ip}")
    
    if image_file:
        # Upload image
        print(f"\nUploading {image_file}...")
        url = f"http://{device_ip}/upload"
        try:
            with open(image_file, 'rb') as f:
                files = {'image': (image_file, f, 'image/jpeg')}
                response = requests.post(url, files=files, timeout=30)
                
            if response.status_code == 200:
                print("✓ Image uploaded successfully!")
                print(f"Response: {response.text}")
            else:
                print(f"✗ Upload failed: {response.status_code}")
                print(f"Response: {response.text}")
        except Exception as e:
            print(f"✗ Error: {e}")
    else:
        print(f"\nDevice found! Use this IP to upload images:")
        print(f"  python upload_image_http.py {device_ip} <image_file>")

if __name__ == '__main__':
    main()

