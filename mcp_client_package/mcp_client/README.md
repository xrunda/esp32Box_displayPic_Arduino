# MCP Client Component

通用的 MCP (Message Control Protocol) 客户端组件，用于通过 WebSocket 连接到 MCP 服务器。

## 功能特性

- WebSocket 连接（支持 WSS/WS）
- MCP 协议实现（initialize, tools/list, tools/call, ping）
- 工具注册和回调机制
- 自动重连
- SSL/TLS 证书验证

## 使用方法

```c
#include "mcp_client.h"

// 定义工具回调函数
esp_err_t my_tool_callback(const char *tool_name, const char *arguments, 
                           char **result_out, bool *is_error_out) {
    // 处理工具调用
    // ...
    return ESP_OK;
}

// 配置工具
mcp_tool_t my_tool = {
    .name = "my_tool",
    .description = "工具描述",
    .input_schema = "{\"type\":\"object\",\"properties\":{...}}",
    .callback = my_tool_callback
};

// 配置 MCP 客户端
mcp_client_config_t config = {
    .server_url = "wss://api.example.com/mcp/",
    .token = "your_token_here",
    .client_name = "MyDevice",
    .client_version = "1.0.0",
    .tools = &my_tool,
    .tool_count = 1
};

// 初始化
mcp_client_init(&config);
```

## API 文档

### `mcp_client_init()`

初始化 MCP 客户端并连接到服务器。

### `mcp_client_deinit()`

断开连接并清理资源。

### `mcp_client_is_connected()`

检查连接状态。

## 依赖

- `tcp_transport`
- `esp-tls`
- `json` (cJSON)
- `esp_crt_bundle`

