#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

static const char *TAG = "ESPBOX";

static const char *html_page = "<!DOCTYPE html><html><body><h1>ESP-BOX Web Server</h1></body></html>";

/* ------------------- HTTP server ------------------- */
static esp_err_t index_handler(httpd_req_t *req)
{
    return httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
    }
    return server;
}

/* ------------------- WiFi Provisioning ------------------- */
static void start_provisioning(void)
{
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting SoftAP provisioning...");
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1,
            NULL,
            "ESP-BOX-PROV",
            NULL
        ));
    } else {
        ESP_LOGI(TAG, "Already provisioned. Deinitializing manager.");
        wifi_prov_mgr_deinit(); // ✅ corrected
    }
}

/* ------------------- App Main ------------------- */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Default WiFi interfaces */
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_ap());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Start WiFi provisioning */
    start_provisioning();

    /* Start web server */
    start_webserver();

    ESP_LOGI(TAG, "ESP-BOX ready!");
}
