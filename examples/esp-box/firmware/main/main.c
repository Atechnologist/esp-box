#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "esp_http_server.h"

static const char *TAG = "espbox_demo";

static const char *html_index = "<!DOCTYPE html><html><head><title>ESP-BOX</title></head>"
                                "<body><h1>ESP-BOX Connected</h1></body></html>";

/* ------------------- Web Server ------------------- */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_index, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_uri_t uri_index = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_index);
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
        ESP_ERROR_CHECK(wifi_prov_mgr_deinit());
    }
}

/* ------------------- App Main ------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP-BOX Firmware Starting...");

    // 1. NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. TCP/IP stack & event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. WiFi Init
    esp_netif_create_default_wifi_sta(); // pointer, do not wrap in ESP_ERROR_CHECK
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4. Start provisioning
    start_provisioning();

    // 5. Start web server
    start_webserver();

    ESP_LOGI(TAG, "ESP-BOX Ready. Connect to ESP-BOX-PROV to set WiFi.");
}
