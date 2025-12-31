# MCP Client 组件迁移 - AI 提示词

## 完整提示词（可直接复制使用）

```
我需要将一个 ESP-IDF 的 MCP (Message Control Protocol) 客户端组件从现有项目迁移到新项目中。

## 任务目标
将 `components/mcp_client/` 组件完整迁移到新项目，确保所有依赖正确配置，代码可以正常编译和运行。

## 源项目组件信息
- **组件路径**：`components/mcp_client/`
- **组件类型**：ESP-IDF 自定义组件
- **ESP-IDF 版本**：5.4
- **目标芯片**：ESP32-S3

## 组件文件结构
```
components/mcp_client/
├── mcp_client.h          # 头文件，定义 API 接口
├── mcp_client.c          # 实现文件，包含完整功能
├── CMakeLists.txt        # CMake 构建配置
├── idf_component.yml     # 组件依赖声明
└── README.md            # 使用文档
```

## 组件功能
这是一个通用的 MCP 客户端组件，提供：
- WebSocket 连接（支持 WSS/WS）
- MCP 协议实现（initialize, tools/list, tools/call, ping）
- 工具注册和回调机制
- 自动重连功能
- SSL/TLS 证书验证

## 组件依赖（必须在 CMakeLists.txt 中声明）
- `tcp_transport` (PRIV_REQUIRES) - TCP 传输层
- `esp-tls` (PRIV_REQUIRES) - TLS/SSL 支持
- `json` (PRIV_REQUIRES) - cJSON 库
- `esp_crt_bundle` (PRIV_REQUIRES) - SSL 证书包
- `freertos` (REQUIRES) - FreeRTOS 任务支持

## 迁移步骤
请按以下步骤完成迁移：

1. **创建组件目录**
   - 在新项目中创建 `components/mcp_client/` 目录

2. **复制组件文件**
   - 复制所有源文件到新目录
   - 保持文件结构不变

3. **检查项目配置**
   - 确保新项目的 `CMakeLists.txt` 正确配置（ESP-IDF 会自动发现 components/ 目录）
   - 检查 `sdkconfig` 或 `sdkconfig.defaults` 中是否启用：
     - `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`
     - `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y`

4. **配置组件依赖**
   - 如果新项目的 `main/CMakeLists.txt` 需要使用此组件，添加：
     ```cmake
     REQUIRES mcp_client
     ```
     或
     ```cmake
     PRIV_REQUIRES mcp_client
     ```

5. **验证编译**
   - 运行 `idf.py build` 确保编译成功
   - 检查是否有未解析的依赖或编译错误

## 使用示例代码
迁移完成后，在新项目中可以这样使用：

```c
#include "mcp_client.h"
#include "esp_log.h"

static const char *TAG = "my_app";

// 工具回调函数
esp_err_t my_tool_callback(const char *tool_name, const char *arguments, 
                           char **result_out, bool *is_error_out) {
    // 解析 arguments（JSON 字符串）
    cJSON *args = cJSON_Parse(arguments);
    if (args == NULL) {
        *is_error_out = true;
        return ESP_FAIL;
    }
    
    // 处理工具逻辑
    // ...
    
    // 构建结果（必须分配内存）
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "success");
    *result_out = cJSON_Print(result);
    cJSON_Delete(result);
    cJSON_Delete(args);
    
    *is_error_out = false;
    return ESP_OK;
}

// 在 app_main() 中初始化
void app_main(void) {
    // 配置工具
    static mcp_tool_t my_tool = {
        .name = "my_tool",
        .description = "我的工具",
        .input_schema = "{\"type\":\"object\",\"properties\":{\"param\":{\"type\":\"string\"}},\"required\":[\"param\"]}",
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
    esp_err_t ret = mcp_client_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MCP client: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "MCP client initialized successfully");
}
```

## 重要注意事项
1. **内存管理**：工具回调函数中的 `result_out` 必须使用 `malloc` 或 `cJSON_Print` 分配内存，调用者会负责释放
2. **SSL 配置**：确保新项目启用了 SSL/TLS 证书包支持
3. **堆内存**：MCP 客户端会创建任务和 WebSocket 连接，确保有足够的堆内存
4. **版本兼容性**：如果新项目使用不同版本的 ESP-IDF，可能需要调整代码

## 验证清单
迁移完成后，请验证：
- [ ] 组件文件已正确复制到新项目
- [ ] `idf.py build` 编译成功，无错误
- [ ] 所有依赖都已正确解析
- [ ] SSL/TLS 配置已启用
- [ ] 代码可以正常初始化 MCP 客户端
- [ ] WebSocket 连接可以正常建立
- [ ] 工具调用功能正常工作

## 如果遇到问题
- 检查 `build/log/` 目录下的编译日志
- 确认所有依赖组件都已正确安装
- 检查 `sdkconfig` 中的相关配置
- 验证网络连接和服务器 URL 是否正确

请帮我完成这个迁移工作，确保组件在新项目中可以正常编译和运行。
```

## 简化版提示词（快速迁移）

```
请帮我把 ESP-IDF 项目中的 `components/mcp_client/` 组件迁移到新项目中。

组件信息：
- 路径：components/mcp_client/
- 文件：mcp_client.h, mcp_client.c, CMakeLists.txt, idf_component.yml
- 依赖：tcp_transport, esp-tls, json, esp_crt_bundle, freertos
- ESP-IDF 5.4, ESP32-S3

请：
1. 在新项目创建 components/mcp_client/ 目录
2. 复制所有组件文件
3. 检查并配置依赖
4. 确保编译成功

完成后提供使用示例代码。
```

