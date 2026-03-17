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

/* ================= PROVISIONING WEB ================= */

esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h2>ESP-BOX WiFi Setup</h2>"
        "<form action=\"/save\">"
        "SSID:<br><input name=\"ssid\"><br>"
        "Password:<br><input name=\"pass\" type=\"password\"><br><br>"
        "<input type=\"submit\" value=\"Save\">"
        "</form></body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
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

/* ================= WEB SERVER ================= */

void start_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };

    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_GET,
        .handler = save_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
}

/* ================= MAIN ================= */

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-BOX START");

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    char ssid[32];
    char pass[64];

    if (load_wifi_credentials(ssid, pass))
    {
        ESP_LOGI(TAG, "Found saved WiFi, connecting...");
        wifi_init_sta(ssid, pass);
    }
    else
    {
        ESP_LOGI(TAG, "No WiFi found, starting provisioning...");
        wifi_init_ap();
        start_server();
    }
}
