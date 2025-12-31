#!/usr/bin/env python3
"""
Upload image to ESP32-S3-Box3 via HTTP
Usage: python upload_image_http.py <device_ip> <image_file>
Example: python upload_image_http.py 192.168.1.100 mengm.jpg
"""

import sys
import requests
import argparse

def upload_image(device_ip, image_file):
    """Upload image to device via HTTP POST"""
    url = f"http://{device_ip}/upload"
    
    print(f"Uploading {image_file} to {url}...")
    
    try:
        with open(image_file, 'rb') as f:
            files = {'image': (image_file, f, 'image/jpeg')}
            response = requests.post(url, files=files, timeout=30)
            
        if response.status_code == 200:
            print("✓ Image uploaded successfully!")
            print(f"Response: {response.text}")
            return True
        else:
            print(f"✗ Upload failed with status code: {response.status_code}")
            print(f"Response: {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Error uploading image: {e}")
        return False

def get_status(device_ip):
    """Get device status"""
    url = f"http://{device_ip}/status"
    
    try:
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            print(f"Device status: {response.text}")
            return True
        else:
            print(f"Failed to get status: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Error getting status: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Upload image to ESP32-S3-Box3 via HTTP')
    parser.add_argument('device_ip', help='Device IP address')
    parser.add_argument('image_file', nargs='?', help='Image file to upload')
    parser.add_argument('--status', action='store_true', help='Get device status only')
    
    args = parser.parse_args()
    
    if args.status:
        get_status(args.device_ip)
    elif args.image_file:
        upload_image(args.device_ip, args.image_file)
    else:
        parser.print_help()

if __name__ == '__main__':
    main()

