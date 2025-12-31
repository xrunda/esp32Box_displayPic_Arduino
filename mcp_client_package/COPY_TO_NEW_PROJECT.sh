#!/bin/bash

# 将 MCP Client 组件复制到新项目的脚本
# 使用方法: ./COPY_TO_NEW_PROJECT.sh /path/to/new/project

if [ -z "$1" ]; then
    echo "使用方法: $0 <新项目路径>"
    echo "示例: $0 /path/to/my_new_project"
    exit 1
fi

NEW_PROJECT="$1"

if [ ! -d "$NEW_PROJECT" ]; then
    echo "错误: 目录不存在: $NEW_PROJECT"
    exit 1
fi

echo "=========================================="
echo "MCP Client 组件迁移工具"
echo "=========================================="
echo ""
echo "源目录: $(pwd)"
echo "目标项目: $NEW_PROJECT"
echo ""

# 检查目标项目是否是 ESP-IDF 项目
if [ ! -f "$NEW_PROJECT/CMakeLists.txt" ]; then
    echo "⚠️  警告: 目标目录可能不是 ESP-IDF 项目（未找到 CMakeLists.txt）"
    read -p "是否继续？(y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# 创建 components 目录
if [ ! -d "$NEW_PROJECT/components" ]; then
    echo "创建 components 目录..."
    mkdir -p "$NEW_PROJECT/components"
fi

# 复制组件
echo "复制 MCP Client 组件..."
if [ -d "$NEW_PROJECT/components/mcp_client" ]; then
    echo "⚠️  目标目录已存在 mcp_client，是否覆盖？"
    read -p "(y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$NEW_PROJECT/components/mcp_client"
        cp -r mcp_client "$NEW_PROJECT/components/"
        echo "✓ 组件已覆盖"
    else
        echo "跳过复制"
    fi
else
    cp -r mcp_client "$NEW_PROJECT/components/"
    echo "✓ 组件已复制"
fi

# 检查 sdkconfig
echo ""
echo "检查项目配置..."
if [ -f "$NEW_PROJECT/sdkconfig.defaults" ]; then
    if ! grep -q "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE" "$NEW_PROJECT/sdkconfig.defaults"; then
        echo "⚠️  建议在 sdkconfig.defaults 中添加："
        echo "   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y"
        echo "   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y"
    else
        echo "✓ SSL/TLS 配置已存在"
    fi
else
    echo "⚠️  未找到 sdkconfig.defaults，建议创建并添加 SSL 配置"
fi

# 检查 main/CMakeLists.txt
echo ""
echo "检查 main/CMakeLists.txt..."
if [ -f "$NEW_PROJECT/main/CMakeLists.txt" ]; then
    if ! grep -q "mcp_client" "$NEW_PROJECT/main/CMakeLists.txt"; then
        echo "⚠️  需要在 main/CMakeLists.txt 中添加 mcp_client 依赖"
        echo "   在 REQUIRES 或 PRIV_REQUIRES 中添加: mcp_client"
    else
        echo "✓ mcp_client 依赖已配置"
    fi
else
    echo "⚠️  未找到 main/CMakeLists.txt"
fi

echo ""
echo "=========================================="
echo "迁移完成！"
echo "=========================================="
echo ""
echo "下一步："
echo "  1. cd $NEW_PROJECT"
echo "  2. 检查并更新 sdkconfig.defaults（如果需要）"
echo "  3. 在 main/CMakeLists.txt 中添加 mcp_client 依赖"
echo "  4. 运行: idf.py build"
echo ""

