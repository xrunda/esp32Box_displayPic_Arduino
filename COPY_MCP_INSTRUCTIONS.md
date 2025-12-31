# 将 MCP Client 组件复制到新项目

## 快速复制方法

### 方法 1：使用自动脚本（推荐）

```bash
# 在当前项目目录下运行
./copy_mcp_to_new_project.sh /path/to/new/project
```

### 方法 2：手动复制

```bash
# 1. 在新项目中创建 components 目录（如果不存在）
mkdir -p /path/to/new/project/components

# 2. 复制组件目录
cp -r components/mcp_client /path/to/new/project/components/

# 3. 只保留核心文件（可选，删除文档文件）
cd /path/to/new/project/components/mcp_client
rm -f AI_MIGRATION_PROMPT.md MIGRATION_GUIDE.md QUICK_START.md README.md package_for_migration.sh
```

## 需要复制的核心文件

以下文件是必需的：

```
components/mcp_client/
├── mcp_client.h          # API 头文件（必需）
├── mcp_client.c          # 实现文件（必需）
├── CMakeLists.txt         # 构建配置（必需）
└── idf_component.yml      # 组件依赖（必需）
```

## 新项目配置步骤

### 1. 检查 SSL/TLS 配置

在 `sdkconfig.defaults` 中添加（如果还没有）：

```
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
```

### 2. 在 main/CMakeLists.txt 中添加依赖

在 `idf_component_register()` 中添加：

```cmake
idf_component_register(
    SRCS
        "your_source.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        mcp_client    # 添加这一行
    # ... 其他配置
)
```

### 3. 编译验证

```bash
cd /path/to/new/project
idf.py build
```

## 使用示例

在新项目的代码中使用：

```c
#include "mcp_client.h"

void app_main(void) {
    // 定义工具
    static mcp_tool_t my_tool = {
        .name = "my_tool",
        .description = "我的工具",
        .input_schema = "{\"type\":\"object\"}",
        .callback = my_tool_callback
    };
    
    // 配置 MCP 客户端
    mcp_client_config_t config = {
        .server_url = "wss://api.xiaozhi.me/mcp/",
        .token = "your_token",
        .client_name = "MyDevice",
        .client_version = "1.0.0",
        .tools = &my_tool,
        .tool_count = 1
    };
    
    // 初始化
    mcp_client_init(&config);
}
```

## 组件依赖

MCP Client 组件依赖以下 ESP-IDF 组件（会自动解析）：
- `tcp_transport` - TCP 传输层
- `esp-tls` - TLS/SSL 支持
- `json` - cJSON 库
- `esp_crt_bundle` - SSL 证书包
- `freertos` - FreeRTOS 任务支持

这些依赖已在 `CMakeLists.txt` 中声明，无需手动安装。

