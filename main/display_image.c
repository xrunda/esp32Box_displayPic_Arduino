#include "bsp/esp-box-3.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "windmill_control.h"

static const char *TAG = "display_image";

// WiFi 配置
#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASSWORD  CONFIG_WIFI_PASSWORD

// 全局 UI 变量
static lv_obj_t *g_img_obj = NULL;
static lv_obj_t *g_status_label = NULL;

// LVGL 内存图片描述符
// 注意：对于JPEG图片，不设置cf，让LVGL自动检测格式
static lv_img_dsc_t g_mem_img_dsc = {
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .data_size = 0,
    .header.cf = LV_IMG_CF_UNKNOWN,  // 让LVGL自动检测格式（JPEG/SJPG等）
    .data = NULL,
};

// --- 函数前向声明 ---
static void display_image_from_buffer(uint8_t *buffer, size_t size);
static esp_err_t download_image_from_url(const char *url);
static httpd_handle_t start_webserver(void);
static esp_err_t upload_post_handler(httpd_req_t *req);
static esp_err_t upload_url_post_handler(httpd_req_t *req);
static void download_image_task(void *pvParameters);
static void free_old_image_task(void *pvParameters);

/**
 * @brief 核心显示逻辑
 * 注意：不能使用 IRAM_ATTR，因为 LVGL 函数在 Flash 中
 */
static void display_image_from_buffer(uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) return;

    // 1. 分配内存 - 使用DMA兼容内存（可缓存），LVGL的JPEG解码器需要可缓存内存
    // 如果图片太大，尝试使用PSRAM，但需要确保数据可访问
    uint8_t *copy_buf = NULL;
    if (size > 50000) {
        // 大图片使用PSRAM
        copy_buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
        // 小图片使用DMA内存（可缓存）
        copy_buf = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    }
    
    if (!copy_buf) {
        // 如果DMA内存不足，尝试使用默认内存
        copy_buf = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    }
    
    if (!copy_buf) {
        ESP_LOGE(TAG, "Memory allocation failed for size: %zu!", size);
        return;
    }
    
    ESP_LOGI(TAG, "Allocated %zu bytes for image data", size);

    // 2. 拷贝完整数据
    memcpy(copy_buf, buffer, size);

    // 3. 稳妥逻辑：增加延时，确保系统底层任务(如网络缓冲区清理)完成
    vTaskDelay(pdMS_TO_TICKS(500));

    // 4. 线程安全地更新 LVGL
    // 使用 bsp_display_lock 确保线程安全
    if (bsp_display_lock(pdMS_TO_TICKS(2000))) {
        // 检查对象是否有效
        if (g_img_obj == NULL) {
            ESP_LOGE(TAG, "Image object is NULL!");
            heap_caps_free(copy_buf);
            bsp_display_unlock();
            return;
        }
        
        // 先刷新缓存，确保旧图片数据被清理
        // 注意：必须在解锁前完成，确保LVGL不再使用旧数据
        lv_img_cache_invalidate_src(&g_mem_img_dsc);
        
        // 先设置新数据，再释放旧数据（确保数据在解码期间有效）
        void* old_data = NULL;
        if (g_mem_img_dsc.data) {
            old_data = (void*)g_mem_img_dsc.data;
        }

        // 更新描述符为新数据
        g_mem_img_dsc.data_size = size;
        g_mem_img_dsc.data = copy_buf;
        g_mem_img_dsc.header.cf = LV_IMG_CF_UNKNOWN;  // 让LVGL自动检测格式
        g_mem_img_dsc.header.w = 0;  // 宽度由解码器自动检测
        g_mem_img_dsc.header.h = 0;  // 高度由解码器自动检测
        
        // 隐藏状态标签
        if (g_status_label) {
            lv_obj_add_flag(g_status_label, LV_OBJ_FLAG_HIDDEN);
        }

        // 设置源并显示（LVGL会自动检测JPEG格式并解码）
        lv_img_set_src(g_img_obj, &g_mem_img_dsc);
        lv_obj_clear_flag(g_img_obj, LV_OBJ_FLAG_HIDDEN);
        
        // 强制重绘
        lv_obj_invalidate(g_img_obj);
        
        // 解锁后再释放旧数据，给LVGL时间完成解码
        bsp_display_unlock();
        
        ESP_LOGI(TAG, "Image displayed (Size: %zu bytes)", size);
        
        // 延迟释放旧数据，确保LVGL解码器不再使用
        // JPEG解码可能需要一些时间，在解锁后异步释放
        if (old_data) {
            // 创建一个任务来延迟释放旧数据
            xTaskCreate(free_old_image_task, "free_old_img", 2048, old_data, 1, NULL);
        }
        
        ESP_LOGI(TAG, "Image displayed (Size: %zu bytes)", size);
    } else {
        heap_caps_free(copy_buf);
        ESP_LOGE(TAG, "Could not get display lock within timeout!");
    }
}

// --- 网络下载处理 ---
typedef struct { uint8_t *buf; size_t len; } http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // 使用DMA内存（可缓存），LVGL解码器需要可缓存内存
            ctx->buf = heap_caps_realloc(ctx->buf, ctx->len + evt->data_len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
            if (!ctx->buf && (ctx->len + evt->data_len) > 50000) {
                // 如果数据太大，尝试使用PSRAM
                ctx->buf = heap_caps_realloc(ctx->buf, ctx->len + evt->data_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            }
            if (!ctx->buf) {
                // 最后尝试默认内存
                ctx->buf = heap_caps_realloc(ctx->buf, ctx->len + evt->data_len, MALLOC_CAP_DEFAULT);
            }
            if (ctx->buf) {
                memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
                ctx->len += evt->data_len;
            } else {
                ESP_LOGE(TAG, "Failed to realloc buffer for HTTP data (size: %zu)", ctx->len + evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (ctx->buf && ctx->len > 0) {
                ESP_LOGI(TAG, "Download finished. Data size: %zu", ctx->len);
                uint8_t *buf_to_display = ctx->buf;
                size_t len_to_display = ctx->len;
                // 先清空 ctx，防止在 display_image_from_buffer 执行期间被其他地方释放
                ctx->buf = NULL;
                ctx->len = 0;
                // 显示图片（会复制数据到新缓冲区）
                display_image_from_buffer(buf_to_display, len_to_display);
                // 释放原始缓冲区（display_image_from_buffer 已经复制了数据）
                free(buf_to_display);
            } else {
                ESP_LOGE(TAG, "Download finished but no data received (size: %zu)", ctx->len);
                if (ctx->buf) {
                    free(ctx->buf);
                    ctx->buf = NULL;
                }
                ctx->len = 0;
            }
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR occurred");
            if (ctx->buf) {
                free(ctx->buf);
                ctx->buf = NULL;
            }
            ctx->len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            if (ctx->buf) {
                free(ctx->buf);
                ctx->buf = NULL;
            }
            ctx->len = 0;
            break;
        default: break;
    }
    return ESP_OK;
}

static esp_err_t download_image_from_url(const char *url) {
    ESP_LOGI(TAG, "Starting download from URL: %s", url);
    http_ctx_t ctx = { .buf = NULL, .len = 0 };
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .timeout_ms = 30000,  // 增加到30秒
        .skip_cert_common_name_check = true
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        // 如果请求失败，确保清理内存
        if (ctx.buf) {
            free(ctx.buf);
            ctx.buf = NULL;
        }
    } else if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP request returned status code: %d", status_code);
        err = ESP_FAIL;
        // 如果状态码不是200，确保清理内存
        if (ctx.buf) {
            free(ctx.buf);
            ctx.buf = NULL;
        }
    } else {
        ESP_LOGI(TAG, "HTTP request successful, status: %d", status_code);
        // 成功时，内存应该在 HTTP_EVENT_ON_FINISH 中已释放
        // 但为了安全，再次检查
        if (ctx.buf) {
            ESP_LOGW(TAG, "Buffer not freed in HTTP_EVENT_ON_FINISH, freeing now");
            free(ctx.buf);
            ctx.buf = NULL;
        }
    }
    
    esp_http_client_cleanup(client);
    return err;
}

// --- HTTP 接口 ---
static esp_err_t upload_post_handler(httpd_req_t *req) {
    size_t len = req->content_len;
    uint8_t *buf = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return ESP_FAIL;
    if (httpd_req_recv(req, (char *)buf, len) > 0) {
        display_image_from_buffer(buf, len);
    }
    free(buf);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static void free_old_image_task(void *pvParameters) {
    void* old_data = pvParameters;
    if (old_data) {
        vTaskDelay(pdMS_TO_TICKS(2000));  // 等待2秒确保解码完成
        heap_caps_free(old_data);
        ESP_LOGI(TAG, "Old image data freed");
    }
    vTaskDelete(NULL);
}

static void download_image_task(void *pvParameters) {
    char *url = (char *)pvParameters;
    if (url == NULL) {
        ESP_LOGE(TAG, "Invalid URL parameter in download task");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Download task started for URL: %s", url);
    
    // 给系统一点时间稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    
    esp_err_t err = download_image_from_url(url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to download image from URL: %s (error: %s)", 
                 url, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Successfully downloaded and displayed image from URL: %s", url);
    }
    
    // 释放URL字符串内存
    free(url);
    
    // 删除任务
    vTaskDelete(NULL);
}

static esp_err_t upload_url_post_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret > 0) {
        buf[ret] = '\0';
        ESP_LOGI(TAG, "Received URL request: %s", buf);
        cJSON *json = cJSON_Parse(buf);
        if (json == NULL) {
            ESP_LOGE(TAG, "Failed to parse JSON");
            httpd_resp_sendstr(req, "Error: Invalid JSON");
            return ESP_FAIL;
        }
        cJSON *url_item = cJSON_GetObjectItem(json, "url");
        if (url_item && cJSON_IsString(url_item)) {
            const char *url_str = url_item->valuestring;
            ESP_LOGI(TAG, "Received image URL: %s", url_str);
            
            // 复制URL字符串到堆内存（任务会释放）
            char *url_copy = malloc(strlen(url_str) + 1);
            if (url_copy == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for URL");
                httpd_resp_sendstr(req, "Error: Memory allocation failed");
                cJSON_Delete(json);
                return ESP_FAIL;
            }
            strcpy(url_copy, url_str);
            
            // 创建任务异步下载图片（不阻塞HTTP响应）
            // 增加栈大小以处理大图片下载
            BaseType_t task_result = xTaskCreate(
                download_image_task,
                "download_img",
                16384,  // 16KB stack (增加以处理大图片)
                url_copy,
                5,      // 优先级
                NULL
            );
            
            if (task_result != pdPASS) {
                ESP_LOGE(TAG, "Failed to create download task");
                free(url_copy);
                httpd_resp_sendstr(req, "Error: Failed to create download task");
                cJSON_Delete(json);
                return ESP_FAIL;
            }
            
            ESP_LOGI(TAG, "Download task created, returning HTTP response");
        } else {
            ESP_LOGE(TAG, "URL not found in JSON or not a string");
            httpd_resp_sendstr(req, "Error: URL not found");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        cJSON_Delete(json);
    } else {
        ESP_LOGE(TAG, "Failed to receive request data");
        httpd_resp_sendstr(req, "Error: No data received");
        return ESP_FAIL;
    }
    
    // 立即返回响应，不等待下载完成
    httpd_resp_sendstr(req, "Accepted");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t u1 = { "/upload", HTTP_POST, upload_post_handler, NULL };
        httpd_uri_t u2 = { "/upload_url", HTTP_POST, upload_url_post_handler, NULL };
        httpd_register_uri_handler(server, &u1);
        httpd_register_uri_handler(server, &u2);
    }
    return server;
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
}

// --- 主函数 ---
void app_main(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wcfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD } };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    bsp_i2c_init();
    
    // 稳妥配置：增加缓冲区至 320*60
    bsp_display_cfg_t dcfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = 320 * 60,
        .double_buffer = 0,
        .flags = { .buff_dma = true }
    };
    bsp_display_start_with_config(&dcfg);
    
    bsp_display_lock(0);
    g_status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(g_status_label, "System Ready...");
    lv_obj_center(g_status_label);
    
    g_img_obj = lv_img_create(lv_scr_act());
    lv_obj_set_size(g_img_obj, 320, 240);
    lv_obj_add_flag(g_img_obj, LV_OBJ_FLAG_HIDDEN);
    bsp_display_unlock();
    
    bsp_display_backlight_on();

    start_webserver();
    
    // 增加网络就绪延时，防止启动时 MCP 客户端 DNS 冲突
    vTaskDelay(pdMS_TO_TICKS(8000));
    windmill_control_init();
}