/*
 * MCP (Message Control Protocol) Client for ESP32
 * Provides WebSocket-based MCP client functionality
 */

#ifndef MCP_CLIENT_H
#define MCP_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tool callback function type
 * Called when a tool is invoked via MCP
 * 
 * @param tool_name Name of the tool being called
 * @param arguments JSON string containing tool arguments
 * @param result_out Output buffer for result JSON string (caller must free)
 * @param is_error_out Output flag indicating if execution resulted in error
 * @return ESP_OK on success
 */
typedef esp_err_t (*mcp_tool_callback_t)(const char *tool_name, const char *arguments, char **result_out, bool *is_error_out);

/**
 * @brief Tool information structure
 */
typedef struct {
    const char *name;              // Tool name
    const char *description;       // Tool description
    const char *input_schema;       // JSON schema for tool input
    mcp_tool_callback_t callback;   // Callback function when tool is called
} mcp_tool_t;

/**
 * @brief MCP client configuration
 */
typedef struct {
    const char *server_url;         // MCP server URL (e.g., "wss://api.xiaozhi.me/mcp/")
    const char *token;              // Authentication token
    const char *client_name;        // Client name for server info
    const char *client_version;     // Client version for server info
    mcp_tool_t *tools;              // Array of tools
    size_t tool_count;              // Number of tools
} mcp_client_config_t;

/**
 * @brief Initialize MCP client
 * 
 * @param config MCP client configuration
 * @return ESP_OK on success
 */
esp_err_t mcp_client_init(const mcp_client_config_t *config);

/**
 * @brief Deinitialize MCP client
 */
void mcp_client_deinit(void);

/**
 * @brief Check if MCP client is connected
 * 
 * @return true if connected, false otherwise
 */
bool mcp_client_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // MCP_CLIENT_H

