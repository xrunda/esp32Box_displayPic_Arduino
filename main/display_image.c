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
static lv_img_dsc_t g_mem_img_dsc = {
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .data_size = 0,
    .header.cf = LV_IMG_CF_RAW, 
    .data = NULL,
};

// --- 函数前向声明 ---
static void display_image_from_buffer(uint8_t *buffer, size_t size);
static esp_err_t download_image_from_url(const char *url);
static httpd_handle_t start_webserver(void);
static esp_err_t upload_post_handler(httpd_req_t *req);
static esp_err_t upload_url_post_handler(httpd_req_t *req);

/**
 * @brief 核心显示逻辑 (放在 IRAM 中以防 Cache Panic)
 */
static IRAM_ATTR void display_image_from_buffer(uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) return;

    // 1. 在 PSRAM 分配空间
    uint8_t *copy_buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy_buf) {
        ESP_LOGE(TAG, "Memory allocation failed!");
        return;
    }

    // 2. 拷贝完整数据
    memcpy(copy_buf, buffer, size);

    // 3. 稳妥逻辑：增加延时，确保系统底层任务(如网络缓冲区清理)完成
    vTaskDelay(pdMS_TO_TICKS(500));

    // 4. 线程安全地更新 LVGL
    if (bsp_display_lock(pdMS_TO_TICKS(1000))) {
        // 释放旧内存
        if (g_mem_img_dsc.data) {
            void* old_data = (void*)g_mem_img_dsc.data;
            g_mem_img_dsc.data = NULL; // 先断开引用
            heap_caps_free(old_data);
        }

        // 更新描述符
        g_mem_img_dsc.data_size = size;
        g_mem_img_dsc.data = copy_buf;

        // 刷新 LVGL 内部图片缓存
        lv_img_cache_invalidate_src(NULL);
        
        // 隐藏状态标签
        if (g_status_label) {
            lv_obj_add_flag(g_status_label, LV_OBJ_FLAG_HIDDEN);
        }

        // 设置源并显示
        lv_img_set_src(g_img_obj, &g_mem_img_dsc);
        lv_obj_clear_flag(g_img_obj, LV_OBJ_FLAG_HIDDEN);
        
        // 强制重绘
        lv_obj_invalidate(lv_scr_act());
        
        bsp_display_unlock();
        ESP_LOGI(TAG, "Image displayed via PSRAM (Size: %zu)", size);
    } else {
        heap_caps_free(copy_buf);
        ESP_LOGE(TAG, "Could not get display lock!");
    }
}

// --- 网络下载处理 ---
typedef struct { uint8_t *buf; size_t len; } http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ctx->buf = heap_caps_realloc(ctx->buf, ctx->len + evt->data_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (ctx->buf) {
                memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
                ctx->len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (ctx->buf && ctx->len > 0) {
                ESP_LOGI(TAG, "Download finished. Data size: %zu", ctx->len);
                display_image_from_buffer(ctx->buf, ctx->len);
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            if (ctx->buf) { free(ctx->buf); ctx->buf = NULL; }
            ctx->len = 0;
            break;
        default: break;
    }
    return ESP_OK;
}

static esp_err_t download_image_from_url(const char *url) {
    http_ctx_t ctx = { .buf = NULL, .len = 0 };
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .timeout_ms = 20000,
        .skip_cert_common_name_check = true
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
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

static esp_err_t upload_url_post_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret > 0) {
        buf[ret] = '\0';
        cJSON *json = cJSON_Parse(buf);
        cJSON *url = cJSON_GetObjectItem(json, "url");
        if (url && cJSON_IsString(url)) {
            download_image_from_url(url->valuestring);
        }
        cJSON_Delete(json);
    }
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