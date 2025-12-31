#!/bin/bash
# Simple script to upload image to ESP32 device
# Usage: ./upload_simple.sh [device_ip] [image_file]

DEVICE_IP=${1:-"192.168.1.100"}
IMAGE_FILE=${2:-"mengm.jpg"}

echo "Uploading $IMAGE_FILE to http://$DEVICE_IP/upload"
echo ""

# Try to upload using curl
curl -X POST \
     -F "image=@$IMAGE_FILE" \
     http://$DEVICE_IP/upload \
     --max-time 30 \
     -v

echo ""
echo "Done!"

