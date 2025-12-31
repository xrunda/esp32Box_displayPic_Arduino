#!/usr/bin/env python3
"""
Upload image URL to ESP32-S3-Box3 via HTTP
Usage: python upload_image_url.py <device_ip> <image_url>
Example: python upload_image_url.py 192.168.1.100 "https://example.com/image.jpg"
"""

import sys
import requests
import argparse
import json

def upload_image_url(device_ip, image_url):
    """Upload image URL to device via HTTP POST"""
    url = f"http://{device_ip}/upload_url"
    
    print(f"Sending image URL to {url}...")
    print(f"Image URL: {image_url}")
    
    try:
        # Send as JSON
        data = {"url": image_url}
        response = requests.post(url, json=data, timeout=60)
        
        if response.status_code == 200:
            print("✓ Image URL sent successfully!")
            print(f"Response: {response.text}")
            return True
        else:
            print(f"✗ Upload failed with status code: {response.status_code}")
            print(f"Response: {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Error sending URL: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Upload image URL to ESP32-S3-Box3 via HTTP')
    parser.add_argument('device_ip', help='Device IP address')
    parser.add_argument('image_url', help='Image URL to download and display')
    
    args = parser.parse_args()
    
    upload_image_url(args.device_ip, args.image_url)

if __name__ == '__main__':
    main()

