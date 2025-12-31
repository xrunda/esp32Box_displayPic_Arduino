/*
 * MCP (Message Control Protocol) Client for ESP32
 * WebSocket-based MCP client implementation
 */

#include "mcp_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "mcp_client";

// MCP client state
static mcp_client_config_t s_config = {0};
static esp_transport_handle_t s_ws_transport = NULL;
static bool s_mcp_connected = false;
static TaskHandle_t s_receive_task_handle = NULL;
static TaskHandle_t s_monitor_task_handle = NULL;

/**
 * @brief Handle ping request
 */
static void handle_ping(cJSON *json)
{
    cJSON *id = cJSON_GetObjectItem(json, "id");
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (id != NULL) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    }
    cJSON *result = cJSON_CreateObject();
    cJSON_AddItemToObject(response, "result", result);
    
    char *response_str = cJSON_Print(response);
    if (response_str != NULL) {
        ESP_LOGI(TAG, "Responding to ping");
        esp_transport_write(s_ws_transport, response_str, strlen(response_str), 5000);
        free(response_str);
    }
    cJSON_Delete(response);
}

/**
 * @brief Handle tools/list request
 */
static void handle_tools_list(cJSON *json)
{
    cJSON *id = cJSON_GetObjectItem(json, "id");
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (id != NULL) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateArray();
    
    // Add all registered tools
    for (size_t i = 0; i < s_config.tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_config.tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_config.tools[i].description);
        cJSON *input_schema = cJSON_Parse(s_config.tools[i].input_schema);
        if (input_schema != NULL) {
            cJSON_AddItemToObject(tool, "inputSchema", input_schema);
        }
        cJSON_AddItemToArray(tools, tool);
    }
    
    cJSON_AddItemToObject(result, "tools", tools);
    cJSON_AddItemToObject(response, "result", result);
    
    char *response_str = cJSON_Print(response);
    if (response_str != NULL) {
        ESP_LOGI(TAG, "Responding to tools/list with %zu tools", s_config.tool_count);
        esp_transport_write(s_ws_transport, response_str, strlen(response_str), 5000);
        free(response_str);
    }
    cJSON_Delete(response);
}

/**
 * @brief Handle tools/call request
 */
static void handle_tools_call(cJSON *json)
{
    cJSON *id = cJSON_GetObjectItem(json, "id");
    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (params == NULL) {
        ESP_LOGW(TAG, "tools/call missing params");
        return;
    }
    
    cJSON *name = cJSON_GetObjectItem(params, "name");
    if (name == NULL || !cJSON_IsString(name)) {
        ESP_LOGW(TAG, "tools/call missing or invalid name");
        return;
    }
    
    const char *tool_name = cJSON_GetStringValue(name);
    ESP_LOGI(TAG, "Received tool call: %s", tool_name);
    
    // Find tool
    mcp_tool_t *tool = NULL;
    for (size_t i = 0; i < s_config.tool_count; i++) {
        if (strcmp(s_config.tools[i].name, tool_name) == 0) {
            tool = &s_config.tools[i];
            break;
        }
    }
    
    if (tool == NULL) {
        ESP_LOGW(TAG, "Unknown tool: %s", tool_name);
        return;
    }
    
    // Get arguments
    cJSON *arguments = cJSON_GetObjectItem(params, "arguments");
    char *arguments_str = NULL;
    if (arguments != NULL) {
        arguments_str = cJSON_Print(arguments);
    }
    
    // Call tool callback
    char *result_str = NULL;
    bool is_error = false;
    esp_err_t ret = ESP_FAIL;
    
    if (tool->callback != NULL) {
        ret = tool->callback(tool_name, arguments_str ? arguments_str : "{}", &result_str, &is_error);
    }
    
    if (arguments_str != NULL) {
        free(arguments_str);
    }
    
    // Build response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (id != NULL) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    
    if (ret == ESP_OK && result_str != NULL) {
        cJSON_AddStringToObject(content_item, "text", result_str);
    } else {
        is_error = true;
        cJSON_AddStringToObject(content_item, "text", "Tool execution failed");
    }
    
    cJSON_AddItemToArray(content, content_item);
    cJSON_AddItemToObject(result, "content", content);
    cJSON_AddBoolToObject(result, "isError", is_error);
    cJSON_AddItemToObject(response, "result", result);
    
    char *response_str = cJSON_Print(response);
    if (response_str != NULL) {
        ESP_LOGI(TAG, "Sending tool call response");
        esp_transport_write(s_ws_transport, response_str, strlen(response_str), 5000);
        free(response_str);
    }
    cJSON_Delete(response);
    
    if (result_str != NULL) {
        free(result_str);
    }
}

/**
 * @brief Parse MCP message and handle different message types
 */
static void handle_mcp_message(const char *data, size_t len)
{
    cJSON *json = cJSON_ParseWithLength(data, len);
    if (json == NULL) {
        ESP_LOGW(TAG, "Failed to parse JSON message");
        return;
    }

    cJSON *method = cJSON_GetObjectItem(json, "method");
    
    if (method != NULL && cJSON_IsString(method)) {
        const char *method_str = cJSON_GetStringValue(method);
        ESP_LOGI(TAG, "Received MCP method: %s", method_str);
        
        if (strcmp(method_str, "ping") == 0) {
            handle_ping(json);
        } else if (strcmp(method_str, "tools/list") == 0) {
            handle_tools_list(json);
        } else if (strcmp(method_str, "tools/call") == 0) {
            handle_tools_call(json);
        } else {
            ESP_LOGW(TAG, "Unknown method: %s", method_str);
        }
    } else {
        ESP_LOGW(TAG, "Message missing method field");
    }
    
    cJSON_Delete(json);
}

/**
 * @brief WebSocket receive task
 */
static void websocket_receive_task(void *pvParameters)
{
    char *buffer = (char *)malloc(4096);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate receive buffer");
        vTaskDelete(NULL);
        return;
    }
    
    while (s_mcp_connected) {
        int len = esp_transport_read(s_ws_transport, buffer, 4095, 1000);
        if (len > 0) {
            buffer[len] = '\0';
            ESP_LOGI(TAG, "Received: %.*s", len > 200 ? 200 : len, buffer);
            
            // Check if it's an initialization message
            cJSON *json = cJSON_ParseWithLength(buffer, len);
            if (json != NULL) {
                cJSON *method = cJSON_GetObjectItem(json, "method");
                
                // Check if it's an initialize request
                if (method != NULL && cJSON_IsString(method) && 
                    strcmp(cJSON_GetStringValue(method), "initialize") == 0) {
                    ESP_LOGI(TAG, "Received initialize request, sending response");
                    
                    // Send initialize response
                    cJSON *response = cJSON_CreateObject();
                    cJSON *id = cJSON_GetObjectItem(json, "id");
                    if (id != NULL) {
                        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
                    }
                    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                    
                    cJSON *result = cJSON_CreateObject();
                    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
                    
                    cJSON *capabilities = cJSON_CreateObject();
                    cJSON *experimental = cJSON_CreateObject();
                    cJSON *prompts = cJSON_CreateObject();
                    cJSON_AddBoolToObject(prompts, "listChanged", false);
                    cJSON *resources = cJSON_CreateObject();
                    cJSON_AddBoolToObject(resources, "subscribe", false);
                    cJSON_AddBoolToObject(resources, "listChanged", false);
                    cJSON *tools = cJSON_CreateObject();
                    cJSON_AddBoolToObject(tools, "listChanged", false);
                    
                    cJSON_AddItemToObject(capabilities, "experimental", experimental);
                    cJSON_AddItemToObject(capabilities, "prompts", prompts);
                    cJSON_AddItemToObject(capabilities, "resources", resources);
                    cJSON_AddItemToObject(capabilities, "tools", tools);
                    
                    cJSON *server_info = cJSON_CreateObject();
                    cJSON_AddStringToObject(server_info, "name", s_config.client_name ? s_config.client_name : "ESP32-MCP-Client");
                    cJSON_AddStringToObject(server_info, "version", s_config.client_version ? s_config.client_version : "1.0.0");
                    
                    cJSON_AddItemToObject(result, "capabilities", capabilities);
                    cJSON_AddItemToObject(result, "serverInfo", server_info);
                    cJSON_AddItemToObject(response, "result", result);
                    
                    char *response_str = cJSON_Print(response);
                    if (response_str != NULL) {
                        esp_transport_write(s_ws_transport, response_str, strlen(response_str), 5000);
                        free(response_str);
                    }
                    cJSON_Delete(response);
                    
                    // Send initialized notification (no id, no params)
                    const char *initialized_notif = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}";
                    esp_transport_write(s_ws_transport, initialized_notif, strlen(initialized_notif), 5000);
                    ESP_LOGI(TAG, "Sent initialized notification");
                } else {
                    // Handle other messages
                    handle_mcp_message(buffer, len);
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGW(TAG, "Failed to parse JSON, treating as raw message");
                handle_mcp_message(buffer, len);
            }
        } else if (len < 0 && len != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "WebSocket read error: %d", len);
            break;
        }
    }
    
    free(buffer);
    ESP_LOGI(TAG, "WebSocket receive task ended");
    s_receive_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Connect to MCP server via WebSocket
 */
static esp_err_t connect_to_mcp_server(void)
{
    if (s_config.server_url == NULL || s_config.token == NULL) {
        ESP_LOGE(TAG, "MCP server URL or token not configured");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to MCP server: %s", s_config.server_url);
    
    // Parse URL (simple parsing for wss://host/path format)
    const char *url = s_config.server_url;
    const char *host_start = NULL;
    const char *path_start = NULL;
    int port = 443; // Default for WSS
    
    if (strncmp(url, "wss://", 6) == 0) {
        host_start = url + 6;
        port = 443;
    } else if (strncmp(url, "ws://", 5) == 0) {
        host_start = url + 5;
        port = 80;
    } else {
        ESP_LOGE(TAG, "Invalid URL format, must start with wss:// or ws://");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Find path separator
    path_start = strchr(host_start, '/');
    if (path_start == NULL) {
        path_start = "/";
    }
    
    // Extract host (before path or port)
    char host[256] = {0};
    const char *port_start = strchr(host_start, ':');
    if (port_start != NULL && (path_start == NULL || port_start < path_start)) {
        // Port specified
        size_t host_len = port_start - host_start;
        if (host_len >= sizeof(host)) {
            host_len = sizeof(host) - 1;
        }
        strncpy(host, host_start, host_len);
        host[host_len] = '\0';
        // Parse port (skip for now, use default)
    } else {
        // No port specified
        size_t host_len = path_start - host_start;
        if (host_len >= sizeof(host)) {
            host_len = sizeof(host) - 1;
        }
        strncpy(host, host_start, host_len);
        host[host_len] = '\0';
    }
    
    // Create transport list
    esp_transport_list_handle_t transport_list = esp_transport_list_init();
    
    // Add TCP transport
    esp_transport_handle_t tcp = esp_transport_tcp_init();
    esp_transport_list_add(transport_list, tcp, "tcp");
    
    // Add SSL transport (for WSS)
    esp_transport_handle_t ssl = NULL;
    if (port == 443) {
        ssl = esp_transport_ssl_init();
        // Use certificate bundle for server verification
        esp_transport_ssl_crt_bundle_attach(ssl, esp_crt_bundle_attach);
        // Skip common name check (for development)
        esp_transport_ssl_skip_common_name_check(ssl);
        esp_transport_list_add(transport_list, ssl, "ssl");
    }
    
    // Add WebSocket transport (on top of SSL or TCP)
    s_ws_transport = esp_transport_ws_init(ssl ? ssl : tcp);
    if (s_ws_transport == NULL) {
        ESP_LOGE(TAG, "Failed to create WebSocket transport");
        esp_transport_list_destroy(transport_list);
        return ESP_FAIL;
    }
    
    // Set WebSocket path with token
    char ws_path[512];
    snprintf(ws_path, sizeof(ws_path), "%s?token=%s", path_start, s_config.token);
    esp_transport_ws_set_path(s_ws_transport, ws_path);
    
    // Connect
    int ret = esp_transport_connect(s_ws_transport, host, port, 10000);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to connect to MCP server: %d", ret);
        esp_transport_close(s_ws_transport);
        esp_transport_list_destroy(transport_list);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Connected to MCP server");
    s_mcp_connected = true;
    
    // Start receive task first (it will handle initialization and tool registration)
    // Increased stack size to 8192 to prevent stack overflow
    xTaskCreate(websocket_receive_task, "mcp_ws_receive", 8192, NULL, 5, &s_receive_task_handle);
    
    // Wait a bit for connection to stabilize before receiving messages
    vTaskDelay(pdMS_TO_TICKS(500));
    
    return ESP_OK;
}

/**
 * @brief MCP connection monitoring task
 */
static void mcp_monitor_task(void *pvParameters)
{
    while (1) {
        if (!s_mcp_connected) {
            ESP_LOGI(TAG, "Attempting to connect to MCP server...");
            if (connect_to_mcp_server() == ESP_OK) {
                ESP_LOGI(TAG, "MCP connection established");
            } else {
                ESP_LOGW(TAG, "Failed to connect, retrying in 10 seconds...");
                vTaskDelay(pdMS_TO_TICKS(10000));
            }
        } else {
            // Check connection status
            int poll_result = esp_transport_poll_read(s_ws_transport, 1000);
            if (poll_result < 0 && poll_result != ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Connection lost, reconnecting...");
                s_mcp_connected = false;
                if (s_ws_transport != NULL) {
                    esp_transport_close(s_ws_transport);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t mcp_client_init(const mcp_client_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "MCP client config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->server_url == NULL || config->token == NULL) {
        ESP_LOGE(TAG, "MCP server URL or token not provided");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    memcpy(&s_config, config, sizeof(mcp_client_config_t));
    
    ESP_LOGI(TAG, "Initializing MCP client...");
    ESP_LOGI(TAG, "Server: %s", s_config.server_url);
    ESP_LOGI(TAG, "Tools: %zu", s_config.tool_count);
    
    // Start MCP connection monitor task
    xTaskCreate(mcp_monitor_task, "mcp_monitor", 4096, NULL, 5, &s_monitor_task_handle);
    
    ESP_LOGI(TAG, "MCP client initialized");
    return ESP_OK;
}

void mcp_client_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing MCP client...");
    
    s_mcp_connected = false;
    
    if (s_receive_task_handle != NULL) {
        vTaskDelete(s_receive_task_handle);
        s_receive_task_handle = NULL;
    }
    
    if (s_monitor_task_handle != NULL) {
        vTaskDelete(s_monitor_task_handle);
        s_monitor_task_handle = NULL;
    }
    
    if (s_ws_transport != NULL) {
        esp_transport_close(s_ws_transport);
        s_ws_transport = NULL;
    }
    
    memset(&s_config, 0, sizeof(s_config));
    
    ESP_LOGI(TAG, "MCP client deinitialized");
}

bool mcp_client_is_connected(void)
{
    return s_mcp_connected;
}

