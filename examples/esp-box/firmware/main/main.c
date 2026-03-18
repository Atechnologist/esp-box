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
#include "driver/gpio.h"

static const char *TAG = "ESP_BOX";

/* GPIO pins */
#define RELAY_GPIO  2
#define LIGHT_GPIO  4
#define BUZZER_GPIO 5

/* NVS storage namespace */
#define STORAGE_NAMESPACE "wifi"

/* Web server handle */
httpd_handle_t server = NULL;

/* ---------------- NVS helpers ---------------- */

void save_wifi_credentials(const char *ssid, const char *pass) {
    nvs_handle_t nvs;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "WiFi credentials saved");
    }
}

bool load_wifi_credentials(char *ssid, char *pass) {
    nvs_handle_t nvs;
    size_t ssid_len = 32;
    size_t pass_len = 64;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;
    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK ||
        nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK) {
        nvs_close(nvs);
        return false;
    }
    nvs_close(nvs);
    return true;
}

/* ---------------- HTTP Handlers ---------------- */

esp_err_t root_handler(httpd_req_t *req) {
    const char *html =
        "<html><body>"
        "<h1>ESP-BOX Control</h1>"
        "<p><a href='/relay/on'>Relay ON</a></p>"
        "<p><a href='/relay/off'>Relay OFF</a></p>"
        "<p><a href='/light/on'>Light ON</a></p>"
        "<p><a href='/light/off'>Light OFF</a></p>"
        "<p><a href='/buzzer/on'>Buzzer ON</a></p>"
        "<p><a href='/buzzer/off'>Buzzer OFF</a></p>"
        "</body></html>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t gpio_control_handler(httpd_req_t *req) {
    const char* path = req->uri;

    if (strcmp(path, "/relay/on") == 0) gpio_set_level(RELAY_GPIO, 1);
    else if (strcmp(path, "/relay/off") == 0) gpio_set_level(RELAY_GPIO, 0);
    else if (strcmp(path, "/light/on") == 0) gpio_set_level(LIGHT_GPIO, 1);
    else if (strcmp(path, "/light/off") == 0) gpio_set_level(LIGHT_GPIO, 0);
    else if (strcmp(path, "/buzzer/on") == 0) gpio_set_level(BUZZER_GPIO, 1);
    else if (strcmp(path, "/buzzer/off") == 0) gpio_set_level(BUZZER_GPIO, 0);

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ---------------- HTTP server ---------------- */

void start_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    /* Root page */
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };
    httpd_register_uri_handler(server, &root);

    /* GPIO handlers */
    const char* uris[] = {"/relay/on","/relay/off","/light/on","/light/off","/buzzer/on","/buzzer/off"};
    for (int i = 0; i < 6; i++) {
        httpd_uri_t uri_handler = {
            .uri = uris[i],
            .method = HTTP_GET,
            .handler = gpio_control_handler
        };
        httpd_register_uri_handler(server, &uri_handler);
    }

    ESP_LOGI(TAG, "HTTP server started");
}

/* ---------------- Wi-Fi ---------------- */

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected, IP: %s", ip4addr_ntoa(&event->ip_info.ip));
        start_server();  // Start HTTP server after STA IP is ready
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    }
}

void wifi_init_sta(const char *ssid, const char *pass) {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password)-1);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to Wi-Fi: %s", ssid);
}

void wifi_init_ap(void) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX-SETUP",
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Started AP: ESP-BOX-SETUP");
}

/* ---------------- Main ---------------- */

void app_main(void) {
    ESP_LOGI(TAG, "ESP-BOX Starting...");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize GPIOs */
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LIGHT_GPIO);
    gpio_set_direction(LIGHT_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    char ssid[32], pass[64];
    if (load_wifi_credentials(ssid, pass)) {
        ESP_LOGI(TAG, "Found saved Wi-Fi, starting STA mode...");
        wifi_init_sta(ssid, pass);
    } else {
        ESP_LOGI(TAG, "No Wi-Fi saved, starting AP provisioning...");
        wifi_init_ap();
        start_server();
    }
}
