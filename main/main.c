
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

// ESP-BOX-S3
#include "esp_box.h"
#include "bsp/esp-box-s3.h"

// Your modules
#include "wifi.h"
#include "mqtt.h"
#include "rmaker.h"
#include "ui.h"
// #include "sr.h"   // 🔒 Disabled for stability (enable later)

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "===============================");
    ESP_LOGI(TAG, "ESP-BOX-S3 HUB STARTING");
    ESP_LOGI(TAG, "===============================");

    // ✅ 1. NVS (REQUIRED FIRST)
    ESP_LOGI(TAG, "[1] Initializing NVS...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS issue, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %d", ret);
        return;
    }

    // ✅ 2. ESP-BOX INIT (LCD + AUDIO + BOARD)
    ESP_LOGI(TAG, "[2] Initializing ESP-BOX...");
    ret = esp_box_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-BOX init failed: %d", ret);
        return;
    }

    // ✅ 3. DISPLAY START (ESP-BOX-S3 specific)
    ESP_LOGI(TAG, "[3] Starting display...");
    bsp_display_start();
    bsp_display_backlight_on();

    // ✅ 4. UI INIT (LVGL)
    ESP_LOGI(TAG, "[4] Initializing UI...");
    ui_init();

    // Small delay so screen is visible before networking starts
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ✅ 5. WIFI
    ESP_LOGI(TAG, "[5] Connecting WiFi...");
    wifi_init_sta();

    // Give WiFi time to connect (simple approach)
    vTaskDelay(pdMS_TO_TICKS(3000));

    // ✅ 6. MQTT
    ESP_LOGI(TAG, "[6] Starting MQTT...");
    mqtt_app_start();

    // ✅ 7. RAINMAKER
    ESP_LOGI(TAG, "[7] Starting RainMaker...");
    rmaker_start();

    // ✅ 8. SPEECH RECOGNITION (DISABLED FOR NOW)
    /*
    ESP_LOGI(TAG, "[8] Starting Speech Recognition...");
    sr_init();
    sr_start();
    */

    ESP_LOGI(TAG, "===============================");
    ESP_LOGI(TAG, "SYSTEM READY");
    ESP_LOGI(TAG, "===============================");

    // Main loop (optional)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Running...");
    }
}
