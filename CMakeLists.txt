cmake_minimum_required(VERSION 3.5)

# Set target to esp32s3 for ESP32-S3-Box3
set(IDF_TARGET esp32s3)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-fdiagnostics-color=always)

project(displayPic)

