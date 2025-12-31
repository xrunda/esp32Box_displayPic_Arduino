#!/bin/bash

# 烧录固件并查找设备 IP

PORT=${1:-/dev/cu.usbmodem1101}

echo "=========================================="
echo "ESP32-S3-Box3 烧录并查找 IP"
echo "=========================================="
echo ""

# 检查并关闭占用串口的进程
echo "1. 检查串口占用..."
PIDS=$(lsof -t $PORT 2>/dev/null)
if [ ! -z "$PIDS" ]; then
    echo "   发现占用串口的进程，正在关闭..."
    kill -9 $PIDS 2>/dev/null
    sleep 2
    echo "   ✓ 已关闭"
else
    echo "   ✓ 串口未被占用"
fi
echo ""

# 烧录
echo "2. 开始烧录固件..."
source $IDF_PATH/export.sh 2>/dev/null || source ~/esp/esp-idf/export.sh 2>/dev/null
idf.py -p $PORT -b 115200 flash

if [ $? -ne 0 ]; then
    echo ""
    echo "✗ 烧录失败"
    echo ""
    echo "如果串口仍然被占用，请："
    echo "  1. 关闭所有串口监视器窗口"
    echo "  2. 运行: ./fix_port.sh"
    exit 1
fi

echo ""
echo "✓ 烧录成功"
echo ""

# 等待设备重启
echo "3. 等待设备重启并连接 WiFi..."
sleep 8

# 查找设备 IP
echo ""
echo "4. 查找设备 IP 地址..."
python3 find_device.py

echo ""
echo "=========================================="
echo "完成！"
echo "=========================================="

