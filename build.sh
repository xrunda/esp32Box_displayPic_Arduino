#!/bin/bash

# Build script for ESP32-S3-Box3 Image Display Project

# Set ESP-IDF environment
cd /Users/liguang/Documents/xRunda/project/AI/github/esp-idf
. ./export.sh

# Go to project directory
cd /Users/liguang/Documents/xRunda/project/AI/github/esp32Box_displayPic_Arduino

# Remove old build directory if it exists
if [ -d "build" ]; then
    echo "Removing old build directory..."
    rm -rf build
fi

# Set target (this will create sdkconfig)
echo "Setting target to esp32s3..."
idf.py set-target esp32s3

# Build the project
echo "Building project..."
idf.py build

echo "Build complete!"

