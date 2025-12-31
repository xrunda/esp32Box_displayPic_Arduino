/*
 * Windmill Control Component
 * Controls windmill (GPIO 21) via MCP protocol
 */

#include "windmill_control.h"
#include "mcp_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "windmill_control";

// MCP Configuration
#define MCP_SERVER_URL "wss://api.xiaozhi.me/mcp/"
#define MCP_TOKEN "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOjk5MTQsImFnZW50SWQiOjEyNDk3NTUsImVuZHBvaW50SWQiOiJhZ2VudF8xMjQ5NzU1IiwicHVycG9zZSI6Im1jcC1lbmRwb2ludCIsImlhdCI6MTc2Njc0NjgwMywiZXhwIjoxNzk4MzA0NDAzfQ.AvI_Vlr2m-0qZjPo-Aymz8JYd-SyIaBYuKn_NMGF35hHEzln3oNH77H4QSDEUQp-QclkfCLyeYa5j3oM6I-QXA"

// GPIO Configuration
#define WINDMILL_GPIO 21

static char s_windmill_state[16] = "";

/**
 * @brief Windmill tool callback
 */
static esp_err_t windmill_tool_callback(const char *tool_name, const char *arguments, char **result_out, bool *is_error_out)
{
    (void)tool_name; // Unused
    
    if (result_out == NULL || is_error_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *result_out = NULL;
    *is_error_out = false;
    
    // Parse arguments
    cJSON *args_json = cJSON_Parse(arguments);
    if (args_json == NULL) {
        ESP_LOGW(TAG, "Failed to parse arguments");
        *is_error_out = true;
        return ESP_FAIL;
    }
    
    cJSON *state = cJSON_GetObjectItem(args_json, "state");
    if (state == NULL || !cJSON_IsString(state)) {
        ESP_LOGW(TAG, "Missing or invalid state argument");
        cJSON_Delete(args_json);
        *is_error_out = true;
        return ESP_FAIL;
    }
    
    const char *state_str = cJSON_GetStringValue(state);
    
    // Control windmill
    if (strcmp(state_str, "on") == 0) {
        gpio_set_level(WINDMILL_GPIO, 1);
        strcpy(s_windmill_state, "on");
        ESP_LOGI(TAG, "风车灯开始旋转");
    } else if (strcmp(state_str, "off") == 0) {
        gpio_set_level(WINDMILL_GPIO, 0);
        strcpy(s_windmill_state, "off");
        ESP_LOGI(TAG, "风车灯停止旋转");
    } else {
        ESP_LOGW(TAG, "Invalid state: %s", state_str);
        cJSON_Delete(args_json);
        *is_error_out = true;
        return ESP_FAIL;
    }
    
    cJSON_Delete(args_json);
    
    // Build result JSON
    cJSON *result_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(result_json, "success", true);
    cJSON_AddStringToObject(result_json, "state", s_windmill_state);
    
    *result_out = cJSON_Print(result_json);
    cJSON_Delete(result_json);
    
    if (*result_out == NULL) {
        *is_error_out = true;
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

/**
 * @brief Initialize windmill GPIO
 */
static void init_windmill_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WINDMILL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(WINDMILL_GPIO, 0);
    strcpy(s_windmill_state, "off");
    ESP_LOGI(TAG, "Windmill GPIO %d initialized", WINDMILL_GPIO);
}

/**
 * @brief Initialize windmill control system
 */
esp_err_t windmill_control_init(void)
{
    ESP_LOGI(TAG, "Initializing windmill control...");
    
    // Initialize GPIO
    init_windmill_gpio();
    
    // Configure MCP client
    static mcp_tool_t windmill_tool = {
        .name = "windmill",
        .description = "风车",
        .input_schema = "{\"type\":\"object\",\"properties\":{\"state\":{\"type\":\"string\",\"enum\":[\"on\",\"off\"]}},\"required\":[\"state\"]}",
        .callback = windmill_tool_callback
    };
    
    mcp_client_config_t mcp_config = {
        .server_url = MCP_SERVER_URL,
        .token = MCP_TOKEN,
        .client_name = "ESP32-S3-Box3",
        .client_version = "1.0.0",
        .tools = &windmill_tool,
        .tool_count = 1
    };
    
    // Initialize MCP client
    esp_err_t ret = mcp_client_init(&mcp_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MCP client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Windmill control initialized");
    return ESP_OK;
}

