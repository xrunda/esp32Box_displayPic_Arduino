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
