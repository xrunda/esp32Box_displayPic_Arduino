# Windmill Control Component

风车控制组件，通过 MCP 协议控制 GPIO 21 上的风车硬件。

## 功能特性

- 通过 MCP 协议远程控制风车
- GPIO 21 控制（开/关）
- 自动连接到 MCP 服务器

## 使用方法

```c
#include "windmill_control.h"

// 在 app_main() 中初始化
windmill_control_init();
```

## MCP 工具

注册的工具名称：`windmill`

参数：
- `state`: "on" 或 "off"

示例 MCP 调用：
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "windmill",
    "arguments": {
      "state": "on"
    }
  }
}
```

## 依赖

- `mcp_client` 组件
- `esp_driver_gpio`
- `json` (cJSON)

