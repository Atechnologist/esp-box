#include "voice.h"
#include "esp_log.h"
#include "esp_sr_iface.h"
#include "esp_sr_models.h"

extern void voice_command_handler(const char *cmd);

static const char *TAG = "VOICE";

void voice_task(void *arg)
{
    srmodel_list_t *models = esp_srmodel_init("model");

    if (!models) {
        ESP_LOGE(TAG, "No models found");
        vTaskDelete(NULL);
        return;
    }

    esp_afe_sr_iface_t *afe_handle = esp_afe_handle_from_name("AFE_SR");
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);

    ESP_LOGI(TAG, "Voice engine started");

    while (1) {

        afe_fetch_result_t *res = afe_handle->fetch(afe_data);

        if (!res || res->ret_value == ESP_FAIL) continue;

        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "Wake word detected");
        }

        if (res->state == SR_RESULT_VERIFIED) {

            const char *cmd = res->phrase_id == 0 ?
                "turn on relay" : "turn off relay";

            ESP_LOGI(TAG, "Command detected: %s", cmd);

            voice_command_handler(cmd);
        }
    }
}

void voice_init(void)
{
    xTaskCreatePinnedToCore(
        voice_task,
        "voice_task",
        8192,
        NULL,
        5,
        NULL,
        1
    );
}
