# MCP Client 组件迁移指南

## AI 提示词模板

```
我需要将一个 ESP-IDF 的 MCP (Message Control Protocol) 客户端组件从现有项目迁移到新项目中。

## 源项目信息
- 组件路径：`components/mcp_client/`
- ESP-IDF 版本：5.4
- 目标芯片：ESP32-S3

## 组件文件列表
请将以下文件从源项目复制到新项目的 `components/mcp_client/` 目录：
1. `mcp_client.h` - 头文件，定义 MCP 客户端 API
2. `mcp_client.c` - 实现文件，包含 WebSocket 连接和 MCP 协议处理
3. `CMakeLists.txt` - 构建配置文件
4. `idf_component.yml` - 组件依赖声明
5. `README.md` - 使用文档（可选）

## 组件依赖
该组件需要以下 ESP-IDF 组件：
- `tcp_transport` (PRIV_REQUIRES)
- `esp-tls` (PRIV_REQUIRES)
- `json` (PRIV_REQUIRES) - cJSON 库
- `esp_crt_bundle` (PRIV_REQUIRES) - SSL 证书包
- `freertos` (REQUIRES)

## 迁移步骤
1. 在新项目中创建 `components/mcp_client/` 目录
2. 复制所有组件文件到新目录
3. 检查新项目的 `CMakeLists.txt` 是否包含组件目录（ESP-IDF 会自动发现 components/ 目录下的组件）
4. 在新项目的 `main/CMakeLists.txt` 中添加对 `mcp_client` 的依赖（如果需要）
5. 验证编译是否成功

## 使用示例
迁移完成后，在新项目中可以这样使用：

```c
#include "mcp_client.h"

// 定义工具回调
esp_err_t my_tool_callback(const char *tool_name, const char *arguments, 
                           char **result_out, bool *is_error_out) {
    // 处理工具调用逻辑
    // result_out 需要分配内存（使用 malloc/cJSON_Print）
    // is_error_out 设置错误标志
    return ESP_OK;
}

// 配置工具
mcp_tool_t my_tool = {
    .name = "my_tool",
    .description = "工具描述",
    .input_schema = "{\"type\":\"object\",\"properties\":{...}}",
    .callback = my_tool_callback
};

// 配置并初始化 MCP 客户端
mcp_client_config_t config = {
    .server_url = "wss://api.example.com/mcp/",
    .token = "your_token_here",
    .client_name = "MyDevice",
    .client_version = "1.0.0",
    .tools = &my_tool,
    .tool_count = 1
};

esp_err_t ret = mcp_client_init(&config);
if (ret != ESP_OK) {
    ESP_LOGE("main", "Failed to initialize MCP client");
}
```

## 注意事项
1. 确保新项目已配置 SSL/TLS 支持（在 sdkconfig 中启用 `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE`）
2. 确保新项目有足够的堆内存（MCP 客户端会创建任务和 WebSocket 连接）
3. 如果新项目使用不同的 ESP-IDF 版本，可能需要调整代码兼容性
4. 工具回调函数中的 `result_out` 必须使用 `malloc` 或 `cJSON_Print` 分配内存，调用者会负责释放

## 验证迁移
迁移完成后，请：
1. 运行 `idf.py build` 确保编译成功
2. 检查是否有未解析的依赖
3. 测试 MCP 连接是否正常建立
4. 测试工具调用是否正常工作

请帮我完成这个迁移工作，并确保所有依赖都正确配置。
```

## 快速迁移命令

如果你有访问源项目的权限，可以使用以下命令快速复制：

```bash
# 从源项目复制组件
cp -r /path/to/source/project/components/mcp_client /path/to/new/project/components/

# 或者使用 git（如果两个项目都在 git 仓库中）
cd /path/to/new/project
git checkout /path/to/source/project -- components/mcp_client
```

## 手动迁移检查清单

- [ ] 创建 `components/mcp_client/` 目录
- [ ] 复制 `mcp_client.h`
- [ ] 复制 `mcp_client.c`
- [ ] 复制 `CMakeLists.txt`
- [ ] 复制 `idf_component.yml`
- [ ] 检查新项目的 `sdkconfig` 中是否启用 SSL/TLS 支持
- [ ] 在 `main/CMakeLists.txt` 中添加 `mcp_client` 到 `REQUIRES` 或 `PRIV_REQUIRES`（如果需要）
- [ ] 运行 `idf.py build` 验证编译
- [ ] 测试 MCP 连接功能

