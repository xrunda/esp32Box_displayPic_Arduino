/*
 * ESP32-S3-Box3 Image Display Example
 * Display JPEG image from SPIFFS file system
 */

#include "bsp/esp-box-3.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <string.h>
#include <stdbool.h>
#include "esp_spiffs.h"
#include "esp_vfs.h"

static const char *TAG = "display_image";

// LVGL file system interface
static void *fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    // LVGL passes path without drive letter, e.g., "/mengm.jpg"
    // We need to prepend "/spiffs" to match our SPIFFS mount point
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/spiffs%s", path);
    
    ESP_LOGI(TAG, "Opening file: %s (full path: %s)", path, full_path);
    
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
 * @brief Create UI with image display
 */
static void create_ui(void)
{
    lv_obj_t *screen = lv_scr_act();
    
    // Set screen background color
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Create image object
    lv_obj_t *img = lv_img_create(screen);
    
    // Try to load image from SPIFFS
    // The image should be stored at /spiffs/mengm.jpg
    // LVGL file system path format: S:/path (where S: is the registered drive letter)
    const char *img_path = "S:/mengm.jpg";
    lv_img_set_src(img, img_path);
    
    // Center the image
    lv_obj_center(img);
    
    // Set image zoom if needed (optional)
    // lv_img_set_zoom(img, 128); // 128 = 100% (no zoom)
    
    ESP_LOGI(TAG, "Image object created, trying to load: %s", img_path);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Image Display Example Started");

    /* Initialize I2C (for touch and audio) */
    ESP_LOGI(TAG, "Initializing I2C...");
    bsp_i2c_init();
    ESP_LOGI(TAG, "I2C initialized");

    /* Initialize SPIFFS file system */
    esp_err_t ret = init_spiffs();
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

    vTaskDelay(pdMS_TO_TICKS(1000));

    bsp_display_lock(0);
    lv_refr_now(default_disp);
    ESP_LOGI(TAG, "Forced LVGL display refresh");
    bsp_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Image Display Example Initialized");
    ESP_LOGI(TAG, "Make sure mengm.jpg is uploaded to SPIFFS partition");
    ESP_LOGI(TAG, "The image will be loaded from: S:/mengm.jpg");
}

