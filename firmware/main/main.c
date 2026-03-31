#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ESP_BOX";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP Box running");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
