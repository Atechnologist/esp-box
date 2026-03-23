#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define TAG "ESP_BOX"

#define WIFI_AP_SSID "ESP-BOX-Setup"
#define WIFI_AP_PASS "12345678"

#define LED_GPIO 2

#define NVS_NAMESPACE "wifi"
#define KEY_SSID "ssid"
#define KEY_PASS "pass"

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* ---------------- NVS ---------------- */

bool load_wifi(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 32;
    size_t pass_len = 64;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;

    if (nvs_get_str(nvs, KEY_SSID, ssid, &ssid_len) != ESP_OK ||
        nvs_get_str(nvs, KEY_PASS, pass, &pass_len) != ESP_OK) {
        nvs_close(nvs);
        return false;
    }

    nvs_close(nvs);
    return true;
}

void save_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_set_str(nvs, KEY_SSID, ssid);
    nvs_set_str(nvs, KEY_PASS, pass);
    nvs_commit(nvs);
    nvs_close(nvs);
}

/* ---------------- WIFI EVENTS ---------------- */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {

        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Connecting...");
            esp_wifi_connect();
        }

        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Retrying WiFi...");
            esp_wifi_connect();
        }

        else if (event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "SoftAP started");
        }
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "GOT IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ---------------- WIFI BASE INIT ---------------- */

void wifi_init_base()
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
}

/* ---------------- START AP ---------------- */

void start_ap()
{
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .password = WIFI_AP_PASS,
            .ssid_len = strlen(WIFI_AP_SSID),
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP MODE: %s", WIFI_AP_SSID);
}

/* ---------------- START STA ---------------- */

void start_sta(char *ssid, char *pass)
{
    esp_netif_create_default_wifi_sta();

    wifi_config_t sta_config = {0};

    strcpy((char *)sta_config.sta.ssid, ssid);
    strcpy((char *)sta_config.sta.password, pass);

    sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ---------------- WEB SERVER ---------------- */

esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<h1>ESP BOX</h1>"
        "<a href='/on'>LED ON</a><br>"
        "<a href='/off'>LED OFF</a>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t on_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 1);
    httpd_resp_sendstr(req, "LED ON");
    return ESP_OK;
}

esp_err_t off_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 0);
    httpd_resp_sendstr(req, "LED OFF");
    return ESP_OK;
}

void start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
        httpd_uri_t on   = { .uri = "/on", .method = HTTP_GET, .handler = on_handler };
        httpd_uri_t off  = { .uri = "/off", .method = HTTP_GET, .handler = off_handler };

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &on);
        httpd_register_uri_handler(server, &off);

        ESP_LOGI(TAG, "Web server started");
    }
}

/* ---------------- MAIN ---------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "BOOT");

    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    wifi_init_base();

    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi(ssid, pass)) {

        ESP_LOGI(TAG, "Connecting to saved WiFi...");
        start_sta(ssid, pass);

        xEventGroupWaitBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(10000)
        );

        if (!(xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT)) {
            ESP_LOGW(TAG, "WiFi failed → fallback AP");
            start_ap();
        }

    } else {
        ESP_LOGI(TAG, "No WiFi → AP mode");
        start_ap();
    }

    start_webserver();
}
