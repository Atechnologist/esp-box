#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void app_main(void)
{
    ESP_LOGI("MINIMAL", "ESP32-S3 minimal example started!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI("MINIMAL", "Tick");
    }
}
