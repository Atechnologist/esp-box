#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_board.h"
#include "app_rmaker.h"

// ✅ FORCE include FIRST
#include "web_server.h"

static const char *TAG = "main";

void start_web_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(8000));
    ESP_LOGI(TAG, "Starting Web Server...");
    web_server_start();   // now properly declared
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Factory + Web Start");

    ESP_ERROR_CHECK(nvs_flash_init());

    bsp_board_init();

    app_rmaker_start();

    xTaskCreate(start_web_task, "web_task", 4096, NULL, 5, NULL);
}
