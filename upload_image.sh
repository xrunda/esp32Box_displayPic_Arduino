#!/bin/bash

# Script to upload image to SPIFFS partition on ESP32-S3-Box3
# Usage: ./upload_image.sh [port] [image_file]
# Example: ./upload_image.sh /dev/ttyUSB0 mengm.jpg

PORT=${1:-/dev/cu.usbmodem1101}
IMAGE_FILE=${2:-mengm.jpg}
SPIFFS_OFFSET=0x110000
SPIFFS_SIZE=0x100000

echo "ESP32-S3-Box3 SPIFFS Image Uploader"
echo "===================================="
echo "Port: $PORT"
echo "Image: $IMAGE_FILE"
echo "SPIFFS Offset: $SPIFFS_OFFSET"
echo ""

# Check if image file exists
if [ ! -f "$IMAGE_FILE" ]; then
    echo "Error: Image file '$IMAGE_FILE' not found!"
    exit 1
fi

# Check if spiffsgen.py exists
SPIFFSGEN="$IDF_PATH/components/spiffs/spiffsgen.py"
if [ ! -f "$SPIFFSGEN" ]; then
    echo "Error: spiffsgen.py not found at $SPIFFSGEN"
    echo "Please set IDF_PATH environment variable"
    exit 1
fi

# Create temporary directory for SPIFFS
TMP_DIR=$(mktemp -d)
echo "Creating SPIFFS image in temporary directory: $TMP_DIR"

# Copy image to temporary directory
cp "$IMAGE_FILE" "$TMP_DIR/"

# Generate SPIFFS image
echo "Generating SPIFFS image..."
python "$SPIFFSGEN" $SPIFFS_SIZE "$TMP_DIR" spiffs_image.bin

if [ $? -ne 0 ]; then
    echo "Error: Failed to generate SPIFFS image"
    rm -rf "$TMP_DIR"
    exit 1
fi

# Upload SPIFFS image to device
echo "Uploading SPIFFS image to device..."
esptool.py --chip esp32s3 --port "$PORT" write_flash $SPIFFS_OFFSET spiffs_image.bin

if [ $? -eq 0 ]; then
    echo ""
    echo "Success! Image uploaded to SPIFFS partition"
    echo "You can now reset the device to see the image"
else
    echo "Error: Failed to upload SPIFFS image"
    rm -rf "$TMP_DIR"
    rm -f spiffs_image.bin
    exit 1
fi

# Cleanup
rm -rf "$TMP_DIR"
rm -f spiffs_image.bin

echo "Done!"

