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

/* ================= GPIO ================= */
#define LED_GPIO 2   // Example LED GPIO
#define RELAY_GPIO 4 // Optional additional GPIO

/* ================= NVS ================= */
#define STORAGE_NAMESPACE "wifi"

void save_wifi_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "Saved Wi-Fi credentials");
    }
}

bool load_wifi_credentials(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 32;
    size_t pass_len = 64;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return false;

    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK ||
        nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK)
    {
        nvs_close(nvs);
        return false;
    }

    nvs_close(nvs);
    return true;
}

/* ================= HTTP SERVER ================= */
httpd_handle_t server = NULL;

esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h2>ESP-BOX Web Control</h2>"
        "<p><a href=\"/led/on\">LED ON</a></p>"
        "<p><a href=\"/led/off\">LED OFF</a></p>"
        "</body></html>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_on_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 1);
    httpd_resp_sendstr(req, "LED ON");
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 0);
    httpd_resp_sendstr(req, "LED OFF");
    return ESP_OK;
}

void start_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_uri_t led_on = {
            .uri = "/led/on",
            .method = HTTP_GET,
            .handler = led_on_handler,
            .user_ctx = NULL
        };
        httpd_uri_t led_off = {
            .uri = "/led/off",
            .method = HTTP_GET,
            .handler = led_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &led_on);
        httpd_register_uri_handler(server, &led_off);
        ESP_LOGI(TAG, "HTTP server started");
    }
}

/* ================= Wi-Fi ================= */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa(&event->ip_info.ip));
        start_server();  // Start server only after IP
    }
}

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
        }
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
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

/* ================= MAIN ================= */
void app_main(void)
{
    ESP_LOGI(TAG, "===== ESP-BOX START =====");

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register Wi-Fi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Initialize GPIOs
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);

    // Load Wi-Fi credentials
    char ssid[32];
    char pass[64];
    if (load_wifi_credentials(ssid, pass)) {
        ESP_LOGI(TAG, "Found saved Wi-Fi, connecting...");
        wifi_init_sta(ssid, pass);
    } else {
        ESP_LOGI(TAG, "No Wi-Fi found, starting AP provisioning...");
        wifi_init_ap();
        start_server();  // Start server immediately in AP mode
    }
}
