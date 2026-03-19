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
#include "driver/gpio.h"

static const char *TAG = "ESP_BOX";

/* ================= STORAGE ================= */
#define STORAGE_NAMESPACE "wifi"

/* ================= GLOBAL ================= */
httpd_handle_t server = NULL;
#define LED_GPIO 2

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

/* ================= WEB SERVER ================= */

esp_err_t root_handler(httpd_req_t *req)
{
    const char *resp =
        "<html><body>"
        "<h1>ESP-BOX HUB</h1>"
        "<p><a href=\"/on\">LED ON</a></p>"
        "<p><a href=\"/off\">LED OFF</a></p>"
        "</body></html>";

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_on_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 1);
    httpd_resp_send(req, "LED ON", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 0);
    httpd_resp_send(req, "LED OFF", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver()
{
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

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &on);
    httpd_register_uri_handler(server, &off);

    ESP_LOGI(TAG, "Web server started");
}

/* ================= WIFI EVENT ================= */

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // 🔥 START SERVER HERE (FIX)
        start_webserver();
    }
}

/* ================= WIFI ================= */

void wifi_init_sta(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL);

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    ESP_LOGI(TAG, "Connecting to %s", ssid);
}

/* ================= MAIN ================= */

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-BOX START");

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    char ssid[32];
    char pass[64];

    if (load_wifi_credentials(ssid, pass))
    {
        ESP_LOGI(TAG, "Connecting to saved WiFi...");
        wifi_init_sta(ssid, pass);
    }
    else
    {
        ESP_LOGI(TAG, "No WiFi found!");
    }
}
