#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"

static const char *TAG = "ESP_BOX";

// === Optional defaults, can be overridden via provisioning ===
#define WIFI_SSID_DEFAULT "YOUR_WIFI"
#define WIFI_PASS_DEFAULT "YOUR_PASSWORD"

// ================= Wi-Fi STA Init =================
static void wifi_init_sta(const char *ssid, const char *pass)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi STA started, connecting to SSID: %s", ssid);
}

// ================= Webserver =================
const char *html_ap = "<html><body><h1>ESP-BOX Provisioning</h1></body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_ap, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_uri_t uri_index = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = index_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_index);
    }
    ESP_LOGI(TAG, "Webserver started");
    return server;
}

// ================= App Main =================
void app_main(void)
{
    // 1. Initialize Wi-Fi with default credentials (replace with provisioning later)
    wifi_init_sta(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);

    // 2. Start Webserver for configuration or UI
    start_webserver();

    // 3. Here you can add Matter/RainMaker provisioning code
    // Example placeholder:
    ESP_LOGI(TAG, "ESP-BOX ready. Connect to Wi-Fi and open web portal.");
}
