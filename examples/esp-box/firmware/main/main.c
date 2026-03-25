#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

static const char *TAG = "ESP_BOX";

/* ================= WEB SERVER ================= */

static esp_err_t root_handler(httpd_req_t *req)
{
    const char* resp = "ESP-BOX connected!";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler
        };
        httpd_register_uri_handler(server, &root);
        ESP_LOGI(TAG, "Webserver started");
    }
}

/* ================= EVENT HANDLER ================= */

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {

        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;

        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received SSID: %s", (char*)cfg->ssid);
            break;
        }

        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;

        case WIFI_PROV_CRED_FAIL:
            ESP_LOGE(TAG, "Provisioning failed");
            break;

        case WIFI_PROV_END:
            ESP_LOGI(TAG, "Provisioning ended");
            wifi_prov_mgr_deinit();
            break;
        }
    }

    if (event_base == WIFI_EVENT) {

        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA start → connecting...");
            esp_wifi_connect();
        }

        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Disconnected → retrying...");
            esp_wifi_connect();
        }
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

        ESP_LOGI(TAG, "GOT IP: " IPSTR, IP2STR(&event->ip_info.ip));

        /* 🔥 Start server ONLY when IP is ready */
        start_webserver();
    }
}

/* ================= MAIN ================= */

void app_main(void)
{
    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Network */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    /* WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {

        ESP_LOGI(TAG, "Starting provisioning...");

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1,
            "abcd1234",
            "ESP-BOX-PROV",
            "12345678"
        ));

        ESP_LOGI(TAG, "Connect to WiFi: ESP-BOX-PROV");
        ESP_LOGI(TAG, "Open: http://192.168.4.1");

    } else {

        ESP_LOGI(TAG, "Already provisioned → connecting");

        wifi_prov_mgr_deinit();

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}
