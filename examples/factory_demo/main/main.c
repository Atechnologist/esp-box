/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "bsp_storage.h"
#include "settings.h"
#include "app_led.h"
#include "app_rmaker.h"
#include "app_sr.h"
#include "audio_player.h"
#include "file_iterator.h"
#include "gui/ui_main.h"
#include "ui_sensor_monitor.h"

#include "bsp_board.h"
#include "bsp/esp-bsp.h"

/* ✅ ADD THIS */
#include "web_server.h"

static const char *TAG = "main";

file_iterator_instance_t *file_iterator;

/* ================= WEB SERVER TASK ================= */
void start_web_task(void *arg)
{
    ESP_LOGI("WEB", "Waiting before starting web server...");
    vTaskDelay(pdMS_TO_TICKS(5000));  // wait for WiFi / RainMaker

    ESP_LOGI("WEB", "Starting web server...");
    start_webserver();

    vTaskDelete(NULL);
}
/* ================================================== */

static esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    static int last_volume;

    sys_param_t *param = settings_get_parameter();
    if (param->volume != 0) {
        last_volume = param->volume;
    }

    bsp_codec_mute_set(setting == AUDIO_PLAYER_MUTE ? true : false);

    if (setting == AUDIO_PLAYER_UNMUTE) {
        bsp_codec_volume_set(param->volume, NULL);
    }

    ESP_LOGI(TAG, "mute setting %d, volume:%d", setting, last_volume);

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "🚀 CUSTOM MAIN IS RUNNING 🚀");
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    /* Initialize NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(settings_read_parameter_from_nvs());

    bsp_spiffs_mount();
    bsp_i2c_init();

    /* Display setup */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = 0,
        .flags = {
            .buff_dma = true,
        }
    };
    cfg.lvgl_port_cfg.task_affinity = 1;

    bsp_display_start_with_config(&cfg);
    bsp_board_init();

    ESP_LOGI(TAG, "Display LVGL demo");
    sensor_task_state_event_init();
    ESP_ERROR_CHECK(ui_main_start());

    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_display_backlight_on();

    /* Audio setup */
    file_iterator = file_iterator_new("/spiffs/mp3");
    assert(file_iterator != NULL);

    audio_player_config_t config = {
        .mute_fn = audio_mute_function,
        .write_fn = bsp_i2s_write,
        .clk_set_fn = bsp_codec_set_fs,
        .priority = 5
    };
    ESP_ERROR_CHECK(audio_player_new(config));

    /* LED setup */
    const board_res_desc_t *brd = bsp_board_get_description();
#ifdef CONFIG_BSP_BOARD_ESP32_S3_BOX_3
    app_pwm_led_init(brd->PMOD2->row2[2], brd->PMOD2->row2[3], brd->PMOD2->row1[3]);
#else
    app_pwm_led_init(brd->PMOD2->row1[1], brd->PMOD2->row1[2], brd->PMOD2->row1[3]);
#endif

    /* Speech recognition */
    ESP_LOGI(TAG, "speech recognition start");
    vTaskDelay(pdMS_TO_TICKS(4000));
    app_sr_start(false);

    /* RainMaker */
    app_rmaker_start();

    /* ✅ START WEB SERVER (NON-BLOCKING) */
    xTaskCreate(start_web_task, "web_task", 4096, NULL, 5, NULL);
}
