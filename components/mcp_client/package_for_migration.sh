#!/bin/bash

# MCP Client 组件打包脚本
# 用于将组件打包，方便迁移到新项目

PACKAGE_DIR="mcp_client_package"
SOURCE_DIR="components/mcp_client"

echo "=========================================="
echo "MCP Client 组件打包工具"
echo "=========================================="
echo ""

# 创建打包目录
if [ -d "$PACKAGE_DIR" ]; then
    echo "清理旧的打包目录..."
    rm -rf "$PACKAGE_DIR"
fi

mkdir -p "$PACKAGE_DIR/mcp_client"

echo "1. 复制组件文件..."
# 复制核心文件
cp "$SOURCE_DIR/mcp_client.h" "$PACKAGE_DIR/mcp_client/"
cp "$SOURCE_DIR/mcp_client.c" "$PACKAGE_DIR/mcp_client/"
cp "$SOURCE_DIR/CMakeLists.txt" "$PACKAGE_DIR/mcp_client/"
cp "$SOURCE_DIR/idf_component.yml" "$PACKAGE_DIR/mcp_client/"
cp "$SOURCE_DIR/README.md" "$PACKAGE_DIR/mcp_client/" 2>/dev/null || true

echo "   ✓ 核心文件已复制"

# 复制文档
echo "2. 复制文档文件..."
cp "$SOURCE_DIR/AI_MIGRATION_PROMPT.md" "$PACKAGE_DIR/" 2>/dev/null || true
cp "$SOURCE_DIR/MIGRATION_GUIDE.md" "$PACKAGE_DIR/" 2>/dev/null || true
cp "$SOURCE_DIR/QUICK_START.md" "$PACKAGE_DIR/" 2>/dev/null || true

echo "   ✓ 文档已复制"

# 创建迁移说明
echo "3. 创建迁移说明..."
cat > "$PACKAGE_DIR/MIGRATION_INSTRUCTIONS.md" << 'EOF'
# MCP Client 组件迁移说明

## 快速迁移步骤

1. **复制组件目录**
   ```bash
   cp -r mcp_client /path/to/new/project/components/
   ```

2. **检查项目配置**
   确保新项目的 `sdkconfig` 或 `sdkconfig.defaults` 中包含：
   ```
   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
   ```

3. **在 main/CMakeLists.txt 中添加依赖**
   ```cmake
   REQUIRES mcp_client
   ```

4. **编译验证**
   ```bash
   idf.py build
   ```

## 详细说明

请参考 `AI_MIGRATION_PROMPT.md` 获取完整的 AI 提示词和详细迁移步骤。

## 组件依赖

- tcp_transport
- esp-tls
- json (cJSON)
- esp_crt_bundle
- freertos

这些依赖会在编译时自动解析，无需手动安装。
EOF

echo "   ✓ 迁移说明已创建"

# 创建验证脚本
echo "4. 创建验证脚本..."
cat > "$PACKAGE_DIR/verify_component.sh" << 'EOF'
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
EOF

chmod +x "$PACKAGE_DIR/verify_component.sh"

echo "   ✓ 验证脚本已创建"

# 创建 README
cat > "$PACKAGE_DIR/README.md" << 'EOF'
# MCP Client 组件迁移包

这是一个完整的 MCP Client 组件包，可以直接复制到新的 ESP-IDF 项目中使用。

## 目录结构

```
mcp_client_package/
├── mcp_client/              # 组件目录（复制到新项目的 components/ 下）
│   ├── mcp_client.h
│   ├── mcp_client.c
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── README.md
├── AI_MIGRATION_PROMPT.md   # AI 迁移提示词
├── MIGRATION_GUIDE.md       # 迁移指南
├── QUICK_START.md           # 快速开始
├── MIGRATION_INSTRUCTIONS.md # 迁移说明
└── verify_component.sh      # 验证脚本
```

## 使用方法

1. 将 `mcp_client/` 目录复制到新项目的 `components/` 目录下
2. 按照 `MIGRATION_INSTRUCTIONS.md` 中的步骤完成迁移
3. 运行 `verify_component.sh` 验证组件完整性

## 更多信息

- 详细迁移步骤：`MIGRATION_INSTRUCTIONS.md`
- AI 提示词：`AI_MIGRATION_PROMPT.md`
- 快速开始：`QUICK_START.md`
EOF

echo ""
echo "=========================================="
echo "打包完成！"
echo "=========================================="
echo ""
echo "打包目录: $PACKAGE_DIR"
echo ""
echo "包含文件："
ls -lh "$PACKAGE_DIR" | tail -n +2
echo ""
echo "下一步："
echo "  1. 将 $PACKAGE_DIR/mcp_client/ 复制到新项目的 components/ 目录"
echo "  2. 参考 MIGRATION_INSTRUCTIONS.md 完成迁移"
echo ""

