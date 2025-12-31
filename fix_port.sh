#!/bin/bash

# 修复串口占用问题

PORT=${1:-/dev/cu.usbmodem1101}

echo "检查串口占用情况: $PORT"
echo ""

# 查找占用串口的进程
PIDS=$(lsof -t $PORT 2>/dev/null)

if [ -z "$PIDS" ]; then
    echo "✓ 串口未被占用"
    echo ""
    echo "如果仍然无法烧录，请尝试："
    echo "  1. 拔掉 USB 线，等待 2 秒，重新插入"
    echo "  2. 手动进入下载模式："
    echo "     - 按住 BOOT 按钮"
    echo "     - 按一下 RESET 按钮"
    echo "     - 释放 BOOT 按钮"
    echo "  3. 然后运行: idf.py -p $PORT flash"
else
    echo "发现占用串口的进程:"
    for PID in $PIDS; do
        ps -p $PID -o pid,comm,args
    done
    echo ""
    read -p "是否关闭这些进程？(y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        for PID in $PIDS; do
            echo "关闭进程 $PID..."
            kill -9 $PID 2>/dev/null
        done
        sleep 1
        echo "✓ 进程已关闭"
        echo ""
        echo "现在可以尝试烧录:"
        echo "  idf.py -p $PORT flash"
    else
        echo "请手动关闭占用串口的程序（如串口监视器）"
    fi
fi

