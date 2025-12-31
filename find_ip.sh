#!/bin/bash

# 快速查找设备 IP 地址
# 使用场景：设备已连接 WiFi，只需要查找 IP

echo "正在查找 ESP32-S3-Box3 设备 IP..."
echo ""

# 检查 find_device.py 是否存在
if [ ! -f "find_device.py" ]; then
    echo "错误: 找不到 find_device.py"
    exit 1
fi

# 运行查找脚本
python3 find_device.py

echo ""
echo "提示："
echo "  - 如果未找到设备，请检查设备是否已连接 WiFi"
echo "  - 可以通过串口监视器查看设备 IP: idf.py monitor"
echo "  - 确保设备和电脑在同一网络"

