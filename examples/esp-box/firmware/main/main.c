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

#define STORAGE_NAMESPACE "wifi"
#define LED_GPIO 2

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
        "<h2>ESP-BOX Setup</h2>"
        "<form action=\"/save\">"
        "SSID:<br><input name=\"ssid\"><br>"
        "Password:<br><input name=\"pass\" type=\"password\"><br><br>"
        "<input type=\"submit\" value=\"Save\">"
        "</form>"
        "<br><a href=\"/on\">LED ON</a>"
        "<br><a href=\"/off\">LED OFF</a>"
        "</body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t save_handler(httpd_req_t *req)
{
    char buf[128];
    httpd_req_get_url_query_str(req, buf, sizeof(buf));

    char ssid[32], pass[64];
    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    save_wifi_credentials(ssid, pass);

    httpd_resp_send(req, "Saved! Rebooting...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

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
    gpio
