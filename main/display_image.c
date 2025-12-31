/*
 * ESP32-S3-Box3 Image Display Example
 * Display JPEG image from network upload
 */

#include "bsp/esp-box-3.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <string.h>
#include <stdbool.h>
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include <dirent.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG = "display_image";

// WiFi credentials - can be configured via menuconfig or set here
#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASSWORD  CONFIG_WIFI_PASSWORD

// Global variables for image display
static lv_obj_t *g_img_obj = NULL;
static SemaphoreHandle_t g_display_mutex = NULL;

// LVGL file system interface
static void *fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    // LVGL passes path with drive letter removed, e.g., "/mengm.jpg" for "S:/mengm.jpg"
    // We need to prepend "/spiffs" to match our SPIFFS mount point
    char full_path[256];
    
    // Remove leading slash if present (LVGL may pass "/mengm.jpg" or "mengm.jpg")
    const char *path_ptr = path;
    if (path[0] == '/') {
        path_ptr = path + 1;
    }
    
    snprintf(full_path, sizeof(full_path), "/spiffs/%s", path_ptr);
    
    ESP_LOGI(TAG, "LVGL requested path: %s, full path: %s", path, full_path);
    
    const char *flags = "";
    if (mode == LV_FS_MODE_WR) {
        flags = "wb";
    } else if (mode == LV_FS_MODE_RD) {
        flags = "rb";
    } else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        flags = "rb+";
    }

    FILE *f = fopen(full_path, flags);
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        return NULL;
    }

    return f;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    fclose((FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    *br = fread(buf, 1, btr, (FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    int w = SEEK_SET;
    if (whence == LV_FS_SEEK_CUR) {
        w = SEEK_CUR;
    } else if (whence == LV_FS_SEEK_END) {
        w = SEEK_END;
    }
    
    fseek((FILE *)file_p, pos, w);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    *pos_p = ftell((FILE *)file_p);
    return LV_FS_RES_OK;
}

/**
 * @brief Initialize SPIFFS file system
 */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}

/**
 * @brief Register LVGL file system driver
 */
static void register_lvgl_fs(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = 'S';
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;

    lv_fs_drv_register(&fs_drv);
    ESP_LOGI(TAG, "LVGL file system driver registered with letter 'S'");
}

/**
 * @brief Check if file exists in SPIFFS
 */
static bool check_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f != NULL) {
        fclose(f);
        return true;
    }
    return false;
}

/**
 * @brief Create initial UI (waiting for image)
 */
static void create_ui(void)
{
    lv_obj_t *screen = lv_scr_act();
    
    // Set screen background color to black
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    ESP_LOGI(TAG, "Screen resolution: %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    
    // Create a label to show waiting message
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Waiting for image upload...\n\nConnect to WiFi and\nupload image via HTTP");
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    // Create image object (will be used when image is uploaded)
    g_img_obj = lv_img_create(screen);
    lv_obj_set_size(g_img_obj, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_center(g_img_obj);
    lv_obj_add_flag(g_img_obj, LV_OBJ_FLAG_HIDDEN); // Hide initially
    
    ESP_LOGI(TAG, "UI created, waiting for image upload");
}

/**
 * @brief Display image from memory buffer
 * Saves image to SPIFFS temporary file and displays it
 */
static void display_image_from_buffer(uint8_t *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid image buffer");
        return;
    }
    
    if (g_display_mutex == NULL) {
        ESP_LOGE(TAG, "Display mutex not initialized");
        return;
    }
    
    if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take display mutex");
        return;
    }
    
    bsp_display_lock(0);
    
    // Save image to SPIFFS temporary file
    const char *temp_path = "/spiffs/temp_image.jpg";
    FILE *f = fopen(temp_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open temp file for writing");
        bsp_display_unlock();
        xSemaphoreGive(g_display_mutex);
        return;
    }
    
    size_t written = fwrite(buffer, 1, size, f);
    fclose(f);
    
    if (written != size) {
        ESP_LOGE(TAG, "Failed to write complete image data. Written %zu of %zu bytes", written, size);
        bsp_display_unlock();
        xSemaphoreGive(g_display_mutex);
        return;
    }
    
    ESP_LOGI(TAG, "Image saved to %s, size: %zu bytes", temp_path, size);
    
    // Hide label and show image
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *label = lv_obj_get_child(screen, 0);
    if (label != NULL) {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Use LVGL file system path to load image
    const char *lvgl_path = "S:/temp_image.jpg";
    lv_img_set_src(g_img_obj, lvgl_path);
    lv_obj_clear_flag(g_img_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(g_img_obj);
    
    ESP_LOGI(TAG, "Image displayed from buffer");
    
    bsp_display_unlock();
    xSemaphoreGive(g_display_mutex);
}

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/**
 * @brief Initialize WiFi
 */
static esp_err_t wifi_init_sta(void)
{
    ESP_LOGI(TAG, "Initializing WiFi...");
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization finished. Connecting to %s...", WIFI_SSID);
    return ESP_OK;
}

/**
 * @brief HTTP POST handler for image upload
 * Supports both raw binary upload and multipart/form-data
 */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received image upload request");
    
    // Get content length
    size_t content_len = req->content_len;
    if (content_len == 0) {
        ESP_LOGE(TAG, "Content length is 0");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content length is 0");
        return ESP_FAIL;
    }
    
    // Limit to 500KB
    if (content_len > 500 * 1024) {
        ESP_LOGE(TAG, "Image too large: %zu bytes", content_len);
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Image too large (max 500KB)");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Receiving image, size: %zu bytes", content_len);
    
    // Allocate buffer for image data (add some extra for multipart headers)
    size_t buffer_size = content_len + 1024;  // Extra space for multipart boundaries
    uint8_t *image_buffer = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (image_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for image");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    
    // Receive all data
    int remaining = content_len;
    int received = 0;
    while (remaining > 0) {
        int ret = httpd_req_recv(req, (char *)(image_buffer + received), remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Failed to receive image data");
            free(image_buffer);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    
    ESP_LOGI(TAG, "Data received successfully, %d bytes", received);
    
    // Check if it's multipart/form-data
    char content_type[64] = {0};
    size_t image_size = received;
    uint8_t *image_data = image_buffer;
    
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) == ESP_OK) {
        if (strstr(content_type, "multipart/form-data")) {
        // Find JPEG start marker (0xFF 0xD8)
        bool found_jpeg = false;
        for (int i = 0; i < received - 1; i++) {
            if (image_buffer[i] == 0xFF && image_buffer[i + 1] == 0xD8) {
                // Found JPEG start, find JPEG end (0xFF 0xD9)
                for (int j = i + 2; j < received - 1; j++) {
                    if (image_buffer[j] == 0xFF && image_buffer[j + 1] == 0xD9) {
                        image_data = image_buffer + i;
                        image_size = j - i + 2;
                        found_jpeg = true;
                        ESP_LOGI(TAG, "Found JPEG in multipart data, size: %zu bytes", image_size);
                        break;
                    }
                }
                break;
            }
        }
        
        if (!found_jpeg) {
            ESP_LOGW(TAG, "Could not find JPEG in multipart data, using raw data");
        }
        }
    }
    
    // Display image
    display_image_from_buffer(image_data, image_size);
    
    // Free buffer (image is now saved to SPIFFS)
    free(image_buffer);
    
    // Send success response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Image uploaded and displayed\"}");
    
    return ESP_OK;
}

// Structure to hold download context
typedef struct {
    uint8_t *buffer;
    size_t buffer_size;
    size_t received_size;
    bool buffer_allocated;
} download_context_t;

/**
 * @brief HTTP event handler for downloading image from URL
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    download_context_t *ctx = (download_context_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            if (ctx != NULL && ctx->buffer != NULL) {
                free(ctx->buffer);
                ctx->buffer = NULL;
                ctx->buffer_allocated = false;
            }
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            // Check content length
            if (ctx != NULL && strcasecmp(evt->header_key, "Content-Length") == 0) {
                ctx->buffer_size = atoi(evt->header_value);
                if (ctx->buffer_size > 500 * 1024) {
                    ESP_LOGE(TAG, "Image too large: %zu bytes", ctx->buffer_size);
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "Image size from header: %zu bytes", ctx->buffer_size);
                ctx->buffer = (uint8_t *)heap_caps_malloc(ctx->buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (ctx->buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for image");
                    return ESP_FAIL;
                }
                ctx->buffer_allocated = true;
                ctx->received_size = 0;
            }
            break;
        case HTTP_EVENT_ON_DATA:
            if (ctx != NULL) {
                // If buffer not allocated yet (no Content-Length header), allocate dynamically
                if (!ctx->buffer_allocated) {
                    // Start with 64KB, will realloc if needed
                    if (ctx->buffer == NULL) {
                        ctx->buffer_size = 64 * 1024;
                        ctx->buffer = (uint8_t *)heap_caps_malloc(ctx->buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                        if (ctx->buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate initial buffer");
                            return ESP_FAIL;
                        }
                        ctx->received_size = 0;
                    }
                }
                
                // Check if we need to expand buffer
                if (ctx->received_size + evt->data_len > ctx->buffer_size) {
                    if (ctx->buffer_size >= 500 * 1024) {
                        ESP_LOGE(TAG, "Image too large (max 500KB)");
                        return ESP_FAIL;
                    }
                    // Expand buffer
                    size_t new_size = ctx->buffer_size * 2;
                    if (new_size > 500 * 1024) {
                        new_size = 500 * 1024;
                    }
                    uint8_t *new_buffer = (uint8_t *)heap_caps_realloc(ctx->buffer, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    if (new_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to reallocate buffer");
                        return ESP_FAIL;
                    }
                    ctx->buffer = new_buffer;
                    ctx->buffer_size = new_size;
                }
                
                // Copy data
                memcpy(ctx->buffer + ctx->received_size, evt->data, evt->data_len);
                ctx->received_size += evt->data_len;
                ESP_LOGD(TAG, "Received %d bytes, total: %zu", evt->data_len, ctx->received_size);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH, received %zu bytes", ctx != NULL ? ctx->received_size : 0);
            if (ctx != NULL && ctx->buffer != NULL && ctx->received_size > 0) {
                ESP_LOGI(TAG, "Downloaded %zu bytes, displaying image...", ctx->received_size);
                // Display the downloaded image (this will copy to SPIFFS)
                display_image_from_buffer(ctx->buffer, ctx->received_size);
                // Free buffer after display (image is now in SPIFFS)
                free(ctx->buffer);
                ctx->buffer = NULL;
                ctx->buffer_allocated = false;
            } else {
                ESP_LOGW(TAG, "No data received or buffer is NULL");
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            // Cleanup if buffer still allocated (error case)
            if (ctx != NULL && ctx->buffer != NULL) {
                free(ctx->buffer);
                ctx->buffer = NULL;
                ctx->buffer_allocated = false;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Download image from URL and display it
 */
static esp_err_t download_image_from_url(const char *url)
{
    ESP_LOGI(TAG, "Downloading image from URL: %s", url);
    
    // Initialize download context
    download_context_t ctx = {
        .buffer = NULL,
        .buffer_size = 0,
        .received_size = 0,
        .buffer_allocated = false
    };
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .timeout_ms = 30000,
        .skip_cert_common_name_check = true,  // Skip CN check for HTTPS
        .keep_alive_enable = true,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    // Set method to GET
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    int64_t content_length = esp_http_client_get_content_length(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %" PRId64, status_code, content_length);
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "Download successful, received %zu bytes", ctx.received_size);
            if (ctx.received_size == 0) {
                ESP_LOGE(TAG, "No data received");
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "HTTP error status: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s (status: %d)", esp_err_to_name(err), status_code);
    }
    
    // Cleanup buffer if still allocated (should be freed in event handler, but just in case)
    if (ctx.buffer != NULL) {
        free(ctx.buffer);
        ctx.buffer = NULL;
    }
    
    esp_http_client_cleanup(client);
    return err;
}

/**
 * @brief HTTP POST handler for uploading image URL
 */
static esp_err_t upload_url_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received image URL upload request");
    
    // Get content length
    size_t content_len = req->content_len;
    if (content_len == 0 || content_len > 512) {  // URL should be short
        ESP_LOGE(TAG, "Invalid content length: %zu", content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    
    // Allocate buffer for JSON/URL data
    char *buffer = (char *)malloc(content_len + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    
    // Receive data
    int remaining = content_len;
    int received = 0;
    while (remaining > 0) {
        int ret = httpd_req_recv(req, buffer + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Failed to receive data");
            free(buffer);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buffer[received] = '\0';
    
    ESP_LOGI(TAG, "Received data: %s", buffer);
    
    // Parse JSON or plain URL
    char *image_url = NULL;
    
    // Try to parse as JSON first
    cJSON *json = cJSON_Parse(buffer);
    if (json != NULL) {
        cJSON *url_item = cJSON_GetObjectItem(json, "url");
        if (url_item != NULL && cJSON_IsString(url_item)) {
            image_url = strdup(cJSON_GetStringValue(url_item));
        }
        cJSON_Delete(json);
    }
    
    // If not JSON or no url field, treat as plain URL
    if (image_url == NULL) {
        // Check if it looks like a URL
        if (strstr(buffer, "http://") != NULL || strstr(buffer, "https://") != NULL) {
            image_url = strdup(buffer);
        }
    }
    
    free(buffer);
    
    if (image_url == NULL) {
        ESP_LOGE(TAG, "No valid URL found in request");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No valid URL provided");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Image URL: %s", image_url);
    
    // Download and display image in a separate task to avoid blocking
    // For now, we'll do it synchronously (could be improved)
    esp_err_t ret = download_image_from_url(image_url);
    free(image_url);
    
    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Image downloaded and displayed\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to download image");
    }
    
    return ret == ESP_OK ? ESP_OK : ESP_FAIL;
}

/**
 * @brief HTTP GET handler for status
 */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    char response[256];
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    
    if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(response, sizeof(response),
                 "{\"status\":\"ok\",\"ip\":\"" IPSTR "\",\"screen\":\"%dx%d\"}",
                 IP2STR(&ip_info.ip), BSP_LCD_H_RES, BSP_LCD_V_RES);
    } else {
        snprintf(response, sizeof(response),
                 "{\"status\":\"ok\",\"ip\":\"not connected\",\"screen\":\"%dx%d\"}",
                 BSP_LCD_H_RES, BSP_LCD_V_RES);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

/**
 * @brief Start HTTP server
 */
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.max_resp_headers = 8;
    config.stack_size = 8192;
    
    httpd_handle_t server = NULL;
    
    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register upload handler
        httpd_uri_t upload_uri = {
            .uri       = "/upload",
            .method    = HTTP_POST,
            .handler   = upload_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &upload_uri);
        
        // Register status handler
        httpd_uri_t status_uri = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);
        
        // Register URL upload handler
        httpd_uri_t upload_url_uri = {
            .uri       = "/upload_url",
            .method    = HTTP_POST,
            .handler   = upload_url_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &upload_url_uri);
        
        ESP_LOGI(TAG, "HTTP server started");
        ESP_LOGI(TAG, "Endpoints: POST /upload (file), POST /upload_url (URL), GET /status");
        return server;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Network Image Display Example Started");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Create display mutex */
    g_display_mutex = xSemaphoreCreateMutex();
    if (g_display_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create display mutex");
        return;
    }

    /* Initialize I2C (for touch and audio) */
    ESP_LOGI(TAG, "Initializing I2C...");
    bsp_i2c_init();
    ESP_LOGI(TAG, "I2C initialized");

    /* Initialize SPIFFS file system */
    ret = init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS initialization failed, continuing anyway...");
    }

    /* Initialize display and LVGL */
    ESP_LOGI(TAG, "Initializing display...");
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = 0,
        .flags = {
            .buff_dma = true,
        }
    };
    lv_disp_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display initialization failed!");
        return;
    }
    ESP_LOGI(TAG, "Display initialized successfully");

    /* Register LVGL file system driver */
    register_lvgl_fs();

    /* Wait a bit for display to stabilize before turning on backlight */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Set display brightness to 100% */
    ESP_LOGI(TAG, "Setting display brightness to 100%%...");
    ret = bsp_display_backlight_on();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on backlight: %s", esp_err_to_name(ret));
        ret = bsp_display_brightness_set(100);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set brightness: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Brightness set successfully");
        }
    } else {
        ESP_LOGI(TAG, "Backlight turned on successfully");
    }

    /* Wait for LVGL task to fully start and backlight to stabilize */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Get display handle */
    lv_disp_t *default_disp = lv_disp_get_default();
    if (default_disp == NULL) {
        ESP_LOGE(TAG, "Failed to get default display!");
        return;
    }

    /* Create UI with thread safety */
    ESP_LOGI(TAG, "Creating UI...");
    bsp_display_lock(0);
    create_ui();
    lv_obj_invalidate(lv_scr_act());
    bsp_display_unlock();

    ESP_LOGI(TAG, "UI created, LVGL will refresh asynchronously");

    /* Initialize WiFi */
    ESP_ERROR_CHECK(wifi_init_sta());

    /* Wait for WiFi connection */
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* Start HTTP server */
    start_webserver();

    ESP_LOGI(TAG, "Network Image Display Example Initialized");
    ESP_LOGI(TAG, "Upload images via HTTP POST to /upload");
    ESP_LOGI(TAG, "Check status via HTTP GET /status");
}

