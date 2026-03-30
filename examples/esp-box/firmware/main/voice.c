#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_sr_iface.h"
#include "esp_sr_models.h"
#include "esp_afe_sr_iface.h"

/* External handler from main.c */
extern void voice_command_handler(const char *cmd);

/* Embedded model symbols (from CMake EMBED_FILES) */
extern const uint8_t model_model_bin_start[] asm("_binary_model_model_bin_start");
extern const uint8_t model_model_bin_end[]   asm("_binary_model_model_bin_end");

static const char *TAG = "VOICE";

/* ================= VOICE TASK ================= */

static void voice_task(void *arg)
{
    ESP_LOGI(TAG, "Starting voice engine...");

    int model_size = model_model_bin_end - model_model_bin_start;
    ESP_LOGI(TAG, "Model size: %d bytes", model_size);

    if (model_size <= 0) {
        ESP_LOGE(TAG, "Model not embedded!");
        vTaskDelete(NULL);
        return;
    }

    /* Load model from embedded memory */
    srmodel_list_t *models = esp_srmodel_init_from_mem(
        model_model_bin_start,
        model_size
    );

    if (!models) {
        ESP_LOGE(TAG, "Failed to load model");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Model loaded successfully");

    /* ================= AFE CONFIG ================= */

    esp_afe_sr_iface_t *afe_handle = esp_afe_handle_from_name("AFE_SR");
    if (!afe_handle) {
        ESP_LOGE(TAG, "AFE handle not found");
        vTaskDelete(NULL);
        return;
    }

    afe_config_t afe_config = AFE_CONFIG_DEFAULT();

    /* ESP-BOX specific tuning */
    afe_config.wakenet_init = true;
    afe_config.aec_init = false;
    afe_config.se_init = true;
    afe_config.vad_init = true;

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);

    if (!afe_data) {
        ESP_LOGE(TAG, "AFE create failed");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "AFE initialized");

    /* ================= MAIN LOOP ================= */

    while (1) {

        afe_fetch_result_t *res = afe_handle->fetch(afe_data);

        if (!res || res->ret_value == ESP_FAIL) {
            continue;
        }

        /* Wake word detected */
        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "Wake word detected");
        }

        /* Command recognized */
        if (res->state == SR_RESULT_VERIFIED) {

            int id = res->phrase_id;
            ESP_LOGI(TAG, "Command ID: %d", id);

            /* Map command IDs */
            switch (id) {

                case 0:
                    ESP_LOGI(TAG, "Command: TURN ON RELAY");
                    voice_command_handler("turn on relay");
                    break;

                case 1:
                    ESP_LOGI(TAG, "Command: TURN OFF RELAY");
                    voice_command_handler("turn off relay");
                    break;

                default:
                    ESP_LOGW(TAG, "Unknown command ID: %d", id);
                    break;
            }
        }
    }
}

/* ================= INIT ================= */

void voice_init(void)
{
    ESP_LOGI(TAG, "Initializing voice task");

    xTaskCreatePinnedToCore(
        voice_task,
        "voice_task",
        10000,     // stack size (important!)
        NULL,
        5,
        NULL,
        1          // core 1 (better for audio)
    );
}
