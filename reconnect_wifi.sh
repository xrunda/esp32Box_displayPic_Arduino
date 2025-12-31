#!/bin/bash

# 重新连接 WiFi 并查找设备 IP
# 使用场景：更换网络后重新配置和查找设备

echo "=========================================="
echo "ESP32-S3-Box3 WiFi 重新连接脚本"
echo "=========================================="
echo ""

# 检查 WiFi 配置
echo "1. 检查 WiFi 配置..."
if grep -q "CONFIG_WIFI_SSID" sdkconfig.defaults; then
    SSID=$(grep "CONFIG_WIFI_SSID=" sdkconfig.defaults | cut -d'"' -f2)
    echo "   当前 WiFi SSID: $SSID"
else
    echo "   ⚠️  未找到 WiFi 配置"
fi
echo ""

# 提示用户确认
read -p "2. 是否已更新 sdkconfig.defaults 中的 WiFi 配置？(y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "   请先更新 sdkconfig.defaults 中的 WiFi SSID 和密码"
    echo "   然后运行: idf.py menuconfig"
    exit 1
fi

# 查找串口
echo ""
echo "3. 查找 ESP32 串口..."
ESP_PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
if [ -z "$ESP_PORT" ]; then
    echo "   ⚠️  未找到 ESP32 串口"
    echo "   请检查设备是否已连接"
    exit 1
else
    echo "   找到串口: $ESP_PORT"
fi

# 重新编译
echo ""
echo "4. 重新编译项目（WiFi 配置已更改）..."
read -p "   是否重新编译？(y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    source $IDF_PATH/export.sh 2>/dev/null || source ~/esp/esp-idf/export.sh 2>/dev/null
    idf.py build
    if [ $? -ne 0 ]; then
        echo "   ✗ 编译失败"
        exit 1
    fi
    echo "   ✓ 编译成功"
fi

# 烧录
echo ""
echo "5. 烧录固件..."
read -p "   是否烧录到设备？(y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    source $IDF_PATH/export.sh 2>/dev/null || source ~/esp/esp-idf/export.sh 2>/dev/null
    idf.py -p $ESP_PORT flash
    if [ $? -ne 0 ]; then
        echo "   ✗ 烧录失败"
        exit 1
    fi
    echo "   ✓ 烧录成功"
    echo ""
    echo "   等待设备重启并连接 WiFi..."
    sleep 5
fi

# 查找设备 IP
echo ""
echo "6. 查找设备 IP 地址..."
echo "   正在扫描网络，请稍候..."
python3 find_device.py

echo ""
echo "=========================================="
echo "完成！"
echo "=========================================="
echo ""
echo "如果找到了设备 IP，可以使用以下命令上传图片："
echo "  python3 upload_image_url.py <device_ip> <image_url>"
echo ""

