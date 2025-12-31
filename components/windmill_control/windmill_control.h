/*
 * Windmill Control Component
 * Controls windmill (GPIO 21) via MCP protocol
 */

#ifndef WINDMILL_CONTROL_H
#define WINDMILL_CONTROL_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize windmill control system
 * @return ESP_OK on success
 */
esp_err_t windmill_control_init(void);

#ifdef __cplusplus
}
#endif

#endif // WINDMILL_CONTROL_H

