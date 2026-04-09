#include "app_sr.h"
#include "esp_log.h"

static const char *TAG = "SR";

void sr_init(void)
{
    esp_err_t ret = app_sr_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SR init failed: %d", ret);
    }
}

void sr_start(void)
{
    esp_err_t ret = app_sr_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SR start failed: %d", ret);
    }
}
