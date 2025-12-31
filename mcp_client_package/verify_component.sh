#!/bin/bash

# 验证 MCP Client 组件是否完整

echo "验证 MCP Client 组件..."

ERRORS=0

# 检查必需文件
REQUIRED_FILES=(
    "mcp_client/mcp_client.h"
    "mcp_client/mcp_client.c"
    "mcp_client/CMakeLists.txt"
    "mcp_client/idf_component.yml"
)

for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo "✗ 缺少文件: $file"
        ERRORS=$((ERRORS + 1))
    else
        echo "✓ $file"
    fi
done

if [ $ERRORS -eq 0 ]; then
    echo ""
    echo "✓ 组件文件完整！"
    exit 0
else
    echo ""
    echo "✗ 发现 $ERRORS 个错误"
    exit 1
fi
