#!/bin/bash
# 上传 mengm2.jpg 图片URL到ESP32设备

DEVICE_IP="192.168.1.89"
# DEVICE_IP="192.168.1.31"
IMAGE_URL="http://res.xrunda.com/ai-test/rs2026.jpg"

echo "上传图片URL到设备: $DEVICE_IP"
echo "图片URL: $IMAGE_URL"
echo ""

python3 upload_image_url.py "$DEVICE_IP" "$IMAGE_URL"

