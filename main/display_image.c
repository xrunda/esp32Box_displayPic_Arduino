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
#include <dirent.h>
#include <sys/stat.h>
#include <inttypes.h>

static const char *TAG = "display_image";

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
 * @brief Create UI with image display
 * Since the image is now 320x240 (same as screen), we can use LVGL's built-in decoder directly
 */
static void create_ui(void)
{
    lv_obj_t *screen = lv_scr_act();
    
    // Set screen background color to black (will be covered by image)
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    ESP_LOGI(TAG, "Screen resolution: %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    
    // Check if image file exists
    const char *spiffs_path = "/spiffs/mengm.jpg";
    if (!check_file_exists(spiffs_path)) {
        ESP_LOGE(TAG, "Image file not found: %s", spiffs_path);
        // Create a label to show error
        lv_obj_t *label = lv_label_create(screen);
        lv_label_set_text(label, "Image not found!\nPlease upload mengm.jpg");
        lv_obj_center(label);
        return;
    }
    
    ESP_LOGI(TAG, "Image file found: %s", spiffs_path);
    
    // Get file size for logging
    struct stat st;
    if (stat(spiffs_path, &st) == 0) {
        ESP_LOGI(TAG, "Image file size: %" PRIu32 " bytes", (uint32_t)st.st_size);
    }
    
    // Use LVGL's built-in JPEG decoder via file system
    // LVGL will handle the decoding automatically when we set the image source
    const char *lvgl_path = "S:/mengm.jpg";  // Use LVGL file system path
    
    // Get image info to print dimensions
    lv_img_header_t header;
    lv_res_t res = lv_img_decoder_get_info(lvgl_path, &header);
    if (res == LV_RES_OK) {
        ESP_LOGI(TAG, "Image dimensions from decoder: %dx%d", header.w, header.h);
        ESP_LOGI(TAG, "Screen resolution: %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
        if (header.w == BSP_LCD_H_RES && header.h == BSP_LCD_V_RES) {
            ESP_LOGI(TAG, "Image size matches screen resolution - perfect fit!");
        } else {
            ESP_LOGW(TAG, "Image size (%dx%d) does NOT match screen (%dx%d) - may need to resize image or re-upload", 
                     header.w, header.h, BSP_LCD_H_RES, BSP_LCD_V_RES);
        }
    } else {
        ESP_LOGW(TAG, "Failed to get image info, will try to display anyway");
    }
    
    ESP_LOGI(TAG, "Creating LVGL image object and setting source to: %s", lvgl_path);
    
    // Create an image object
    lv_obj_t *img = lv_img_create(screen);
    
    // Set the image source - LVGL will decode the JPEG automatically
    lv_img_set_src(img, lvgl_path);
    
    // Center the image
    lv_obj_center(img);
    
    // Invalidate to trigger refresh
    lv_obj_invalidate(img);
    
    ESP_LOGI(TAG, "Image source set, LVGL will decode and display asynchronously");
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
    } else {
        // List files in SPIFFS to verify image is there
        DIR *dir = opendir("/spiffs");
        if (dir != NULL) {
            ESP_LOGI(TAG, "SPIFFS files:");
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                ESP_LOGI(TAG, "  - %s", entry->d_name);
            }
            closedir(dir);
        }
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
    // Just invalidate to trigger refresh - LVGL will handle it asynchronously
    lv_obj_invalidate(lv_scr_act());
    bsp_display_unlock();

    // Don't call lv_refr_now() here - it blocks and causes watchdog timeout
    // LVGL will refresh the display in its background task
    ESP_LOGI(TAG, "UI created, LVGL will refresh asynchronously");

    ESP_LOGI(TAG, "Image Display Example Initialized");
    ESP_LOGI(TAG, "Make sure mengm.jpg is uploaded to SPIFFS partition");
    ESP_LOGI(TAG, "The image will be loaded from: S:/mengm.jpg");
}

