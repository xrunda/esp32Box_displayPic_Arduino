#!/bin/bash

# 更新 WiFi 配置并重新编译烧录

echo "=========================================="
echo "更新 WiFi 配置"
echo "=========================================="
echo ""

# 检查 sdkconfig.defaults
echo "1. 检查 sdkconfig.defaults 中的 WiFi 配置..."
SSID=$(grep "CONFIG_WIFI_SSID=" sdkconfig.defaults | grep -v "^#" | cut -d'"' -f2)
if [ -z "$SSID" ]; then
    echo "   ⚠️  未找到 WiFi SSID 配置"
    exit 1
fi
echo "   当前配置: SSID=$SSID"
echo ""

# 删除旧的 sdkconfig 以强制重新生成
echo "2. 删除旧的 sdkconfig（强制重新读取 sdkconfig.defaults）..."
if [ -f "sdkconfig" ]; then
    mv sdkconfig sdkconfig.old.backup
    echo "   ✓ 已备份旧配置为 sdkconfig.old.backup"
fi
echo ""

# 重新配置
echo "3. 重新配置项目（应用 sdkconfig.defaults）..."
source $IDF_PATH/export.sh 2>/dev/null || source ~/esp/esp-idf/export.sh 2>/dev/null
idf.py reconfigure
if [ $? -ne 0 ]; then
    echo "   ✗ 配置失败"
    exit 1
fi
echo "   ✓ 配置成功"
echo ""

# 验证配置
echo "4. 验证 WiFi 配置..."
NEW_SSID=$(grep "CONFIG_WIFI_SSID=" sdkconfig | cut -d'"' -f2)
echo "   编译配置中的 SSID: $NEW_SSID"
if [ "$NEW_SSID" != "$SSID" ]; then
    echo "   ⚠️  警告：配置可能不一致"
fi
echo ""

# 重新编译
echo "5. 重新编译项目..."
idf.py build
if [ $? -ne 0 ]; then
    echo "   ✗ 编译失败"
    exit 1
fi
echo "   ✓ 编译成功"
echo ""

# 询问是否烧录
read -p "6. 是否立即烧录到设备？(y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    PORT=${1:-/dev/cu.usbmodem1101}
    
    # 检查串口占用
    PIDS=$(lsof -t $PORT 2>/dev/null)
    if [ ! -z "$PIDS" ]; then
        echo "   关闭占用串口的进程..."
        kill -9 $PIDS 2>/dev/null
        sleep 2
    fi
    
    echo "   开始烧录..."
    idf.py -p $PORT -b 115200 flash
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "   ✓ 烧录成功！"
        echo ""
        echo "   设备将重启并连接新的 WiFi: $SSID"
        echo "   等待 10 秒后查找设备 IP..."
        sleep 10
        python3 find_device.py
    else
        echo "   ✗ 烧录失败"
    fi
else
    echo ""
    echo "可以稍后使用以下命令烧录："
    echo "  idf.py -p /dev/cu.usbmodem1101 flash"
fi

echo ""
echo "=========================================="
echo "完成！"
echo "=========================================="

