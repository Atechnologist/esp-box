#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include <string.h>

#define LED_GPIO 2  // Change per your ESP-BOX LED pin
#define TAG "ESP_BOX"

#define WIFI_AP_SSID "ESP-BOX-Setup"
#define WIFI_AP_PASS "setup1234"
#define MAX_STA_CONN 4

// NVS keys
#define NVS_NAMESPACE "wifi_creds"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "pass"

// Simple LED state
static bool led_state = false;

// HTTP Server handle
static httpd_handle_t server = NULL;

// Forward declarations
esp_err_t led_on_handler(httpd_req_t *req);
esp_err_t led_off_handler(httpd_req_t *req);
esp_err_t wifi_scan_handler(httpd_req_t *req);

// ---------------- NVS helpers ----------------
static void save_wifi_credentials(const char* ssid, const char* pass) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, NVS_KEY_SSID, ssid);
        nvs_set_str(handle, NVS_KEY_PASS, pass);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "WiFi credentials saved: %s", ssid);
    }
}

static bool load_wifi_credentials(char* ssid, size_t ssid_len, char* pass, size_t pass_len) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        if (nvs_get_str(handle, NVS_KEY_SSID, ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(handle, NVS_KEY_PASS, pass, &pass_len) == ESP_OK) {
            nvs_close(handle);
            ESP_LOGI(TAG, "Loaded WiFi credentials: %s", ssid);
            return true;
        }
        nvs_close(handle);
    }
    return false;
}

// ---------------- WiFi ----------------
static void start_softap(void) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_ap_config = {0};
    strcpy((char*)wifi_ap_config.ap.ssid, WIFI_AP_SSID);
    wifi_ap_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    strcpy((char*)wifi_ap_config.ap.password, WIFI_AP_PASS);
    wifi_ap_config.ap.max_connection = MAX_STA_CONN;
    wifi_ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "SoftAP started: SSID=%s PASS=%s", WIFI_AP_SSID, WIFI_AP_PASS);
}

static void connect_sta(const char* ssid, const char* pass) {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_sta_config = {0};
    strncpy((char*)wifi_sta_config.sta.ssid, ssid, sizeof(wifi_sta_config.sta.ssid));
    strncpy((char*)wifi_sta_config.sta.password, pass, sizeof(wifi_sta_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    esp_wifi_start();
    esp_wifi_connect();
    ESP_LOGI(TAG, "Connecting to WiFi SSID=%s", ssid);
}

// ---------------- HTTP Server Handlers ----------------
esp_err_t led_on_handler(httpd_req_t *req) {
    gpio_set_level(LED_GPIO, 1);
    led_state = true;
    httpd_resp_sendstr(req, "LED ON");
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req) {
    gpio_set_level(LED_GPIO, 0);
    led_state = false;
    httpd_resp_sendstr(req, "LED OFF");
    return ESP_OK;
}

esp_err_t wifi_scan_handler(httpd_req_t *req) {
    uint16_t ap_count = 20;
    wifi_ap_record_t ap_info[20];
    memset(ap_info, 0, sizeof(ap_info));
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    char response[1024] = {0};
    strcat(response, "[");
    for (int i = 0; i < ap_count; i++) {
        strcat(response, "\"");
        strcat(response, (char*)ap_info[i].ssid);
        strcat(response, "\"");
        if (i < ap_count - 1) strcat(response, ",");
    }
    strcat(response, "]");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// ---------------- HTTP Server Init ----------------
static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t led_on_uri = {
            .uri = "/led/on",
            .method = HTTP_GET,
            .handler = led_on_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &led_on_uri);

        httpd_uri_t led_off_uri = {
            .uri = "/led/off",
            .method = HTTP_GET,
            .handler = led_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &led_off_uri);

        httpd_uri_t wifi_scan_uri = {
            .uri = "/wifi_scan",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_scan_uri);
    }
    return server;
}

// ---------------- Main ----------------
void app_main(void) {
    ESP_LOGI(TAG, "ESP-BOX S3 starting...");

    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Init GPIO
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    // Start WiFi
    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        connect_sta(ssid, pass);
    } else {
        start_softap();
    }

    // Start webserver
    server = start_webserver();
}
