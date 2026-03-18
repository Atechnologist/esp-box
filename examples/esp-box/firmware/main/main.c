#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "nvs.h"

static const char *TAG = "ESP_BOX";

/* Storage */
#define STORAGE_NAMESPACE "wifi"

/* Web server */
httpd_handle_t server = NULL;

/* ================= NVS ================= */

void save_wifi_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", pass);
    nvs_commit(nvs);
    nvs_close(nvs);
}

bool load_wifi_credentials(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 32;
    size_t pass_len = 64;

    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;

    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK ||
        nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK)
    {
        nvs_close(nvs);
        return false;
    }

    nvs_close(nvs);
    return true;
}

/* ================= WEB ================= */

esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h2>ESP-BOX Control</h2>"
        "<p><a href=\"/on\"><button>LED ON</button></a></p>"
        "<p><a href=\"/off\"><button>LED OFF</button></a></p>"
        "</body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_on_handler(httpd_req_t *req)
{
    gpio_set_level(2, 1);
    httpd_resp_sendstr(req, "LED ON");
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req)
{
    gpio_set_level(2, 0);
    httpd_resp_sendstr(req, "LED OFF");
    return ESP_OK;
}

esp_err_t save_handler(httpd_req_t *req)
{
    char buf[128];
    httpd_req_get_url_query_str(req, buf, sizeof(buf));

    char ssid[32], pass[64];
    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    ESP_LOGI(TAG, "Saving SSID: %s", ssid);

    save_wifi_credentials(ssid, pass);

    httpd_resp_send(req, "Saved! Rebooting...", HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

void start_webserver()
{
    if (server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };

    httpd_uri_t on = {
        .uri = "/on",
        .method = HTTP_GET,
        .handler = led_on_handler
    };

    httpd_uri_t off = {
        .uri = "/off",
        .method = HTTP_GET,
        .handler = led_off_handler
    };

    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_GET,
        .handler = save_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &on);
    httpd_register_uri_handler(server, &off);
    httpd_register_uri_handler(server, &save);

    ESP_LOGI(TAG, "HTTP server started");
}

/* ================= WIFI ================= */

static void wifi_init_ap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX-SETUP",
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Started AP: ESP-BOX-SETUP");

    start_webserver(); // Start immediately in AP mode
}

static void wifi_init_sta(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    ESP_LOGI(TAG, "Connecting to %s", ssid);
}

/* ================= EVENT HANDLER ================= */

static void on_ip_event(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi connected, starting web server...");
        start_webserver();
    }
}

/* ================= MAIN ================= */

void app_main(void)
{
    ESP_LOGI(TAG, "===== ESP-BOX WITH PROVISIONING =====");

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    // Register event handler
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &on_ip_event,
                                        NULL,
                                        &instance_any_id);

    char ssid[32];
    char pass[64];

    if (load_wifi_credentials(ssid, pass))
    {
        ESP_LOGI(TAG, "Found saved WiFi, connecting...");
        wifi_init_sta(ssid, pass);
    }
    else
    {
        ESP_LOGI(TAG, "No WiFi found → starting provisioning");
        wifi_init_ap();
    }
}
