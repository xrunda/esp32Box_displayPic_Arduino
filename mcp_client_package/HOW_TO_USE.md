# MCP Client 组件迁移包使用说明

## 📦 迁移包内容

```
mcp_client_package/
├── mcp_client/                    # 组件目录（核心文件）
│   ├── mcp_client.h              # API 头文件
│   ├── mcp_client.c              # 实现文件
│   ├── CMakeLists.txt            # 构建配置
│   ├── idf_component.yml         # 组件依赖
│   └── README.md                 # 组件文档
├── AI_MIGRATION_PROMPT.md        # AI 提示词（完整版）
├── MIGRATION_GUIDE.md            # 详细迁移指南
├── MIGRATION_INSTRUCTIONS.md      # 快速迁移说明
├── QUICK_START.md                # 快速开始
├── COPY_TO_NEW_PROJECT.sh        # 自动迁移脚本
├── verify_component.sh           # 组件验证脚本
└── README.md                     # 本文件
```

## 🚀 使用方法

### 方法 1：使用 AI 提示词（推荐）

1. **打开 AI_MIGRATION_PROMPT.md**
2. **复制完整版提示词**（从第 5 行开始到第 159 行）
3. **粘贴到 AI 对话中**（如 ChatGPT、Claude 等）
4. **AI 会自动完成迁移**

### 方法 2：使用自动迁移脚本

```bash
# 进入迁移包目录
cd mcp_client_package

# 运行自动迁移脚本
./COPY_TO_NEW_PROJECT.sh /path/to/new/project
```

### 方法 3：手动迁移

```bash
# 1. 复制组件目录
cp -r mcp_client /path/to/new/project/components/

# 2. 在新项目中配置依赖
# 编辑 main/CMakeLists.txt，添加：
#   REQUIRES mcp_client

# 3. 检查 SSL 配置
# 确保 sdkconfig.defaults 中包含：
#   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
#   CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y

# 4. 编译验证
cd /path/to/new/project
idf.py build
```

## 📋 AI 提示词使用步骤

### 步骤 1：准备提示词

打开 `AI_MIGRATION_PROMPT.md`，复制完整版提示词（第 5-159 行）

### 步骤 2：提交给 AI

将提示词粘贴到 AI 对话中，AI 会：
- 创建组件目录
- 复制所有文件
- 配置依赖
- 验证编译

### 步骤 3：验证结果

AI 完成后，运行验证：
```bash
cd /path/to/new/project
idf.py build
```

## ✅ 验证清单

迁移完成后，检查：

- [ ] `components/mcp_client/` 目录存在
- [ ] 所有必需文件都已复制
- [ ] `main/CMakeLists.txt` 包含 `mcp_client` 依赖
- [ ] `sdkconfig` 中启用了 SSL/TLS
- [ ] `idf.py build` 编译成功
- [ ] 可以正常初始化 MCP 客户端

## 🔧 故障排除

### 编译错误：找不到 mcp_client

**解决**：在 `main/CMakeLists.txt` 中添加：
```cmake
REQUIRES mcp_client
```

### SSL/TLS 错误

**解决**：在 `sdkconfig.defaults` 中添加：
```
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
```

### 依赖未找到

**解决**：运行 `idf.py reconfigure` 重新配置项目

## 📚 更多资源

- **详细迁移指南**：`MIGRATION_GUIDE.md`
- **快速开始**：`QUICK_START.md`
- **组件文档**：`mcp_client/README.md`

## 💡 提示

- **推荐使用 AI 提示词**：最省时省力，AI 会自动处理所有细节
- **完整版提示词**：包含所有信息，一次性完成迁移
- **简化版提示词**：适合有经验的开发者快速迁移

