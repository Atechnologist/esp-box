#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "ESP_BOX";

// HTML page for AP portal
static const char *html_ap = 
"<html><body>"
"<h2>ESP-BOX Wi-Fi Setup</h2>"
"<form action=\"/save\" method=\"POST\">"
"SSID: <input type=\"text\" name=\"ssid\"><br>"
"Password: <input type=\"password\" name=\"pass\"><br>"
"<input type=\"submit\" value=\"Save\">"
"</form>"
"</body></html>";

// NVS keys
#define NVS_NAMESPACE "wifi_creds"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "pass"

// Forward declarations
static void start_webserver(void);
static void wifi_init_sta(const char *ssid, const char *pass);
static void wifi_init_ap(void);
static bool credentials_exist_in_nvs(char *ssid_out, char *pass_out);

// HTTP handler for saving credentials
static esp_err_t save_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // parse ssid and pass from form (very basic)
    char ssid[32] = {0};
    char pass[64] = {0};
    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    ESP_LOGI(TAG, "Saving SSID: %s, PASS: %s", ssid, pass);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, NVS_KEY_SSID, ssid);
        nvs_set_str(nvs, NVS_KEY_PASS, pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    httpd_resp_sendstr(req, "Saved! ESP will reboot and connect to Wi-Fi.");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

// HTTP handler for main page
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_ap, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    httpd_start(&server, &config);

    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_root);

    httpd_uri_t uri_save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_save);
}

// Check if credentials exist in NVS
static bool credentials_exist_in_nvs(char *ssid_out, char *pass_out) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return false;
    size_t ssid_len = 32, pass_len = 64;
    bool ok = nvs_get_str(nvs, NVS_KEY_SSID, ssid_out, &ssid_len) == ESP_OK &&
              nvs_get_str(nvs, NVS_KEY_PASS, pass_out, &pass_len) == ESP_OK;
    nvs_close(nvs);
    return ok;
}

// Initialize Wi-Fi in STA mode
static void wifi_init_sta(const char *ssid, const char *pass) {
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

// Initialize Wi-Fi in AP mode
static void wifi_init_ap(void) {
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.ap.ssid, "ESP_BOX_AP");
    wifi_config.ap.ssid_len = strlen("ESP_BOX_AP");
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

void app_main(void) {
    ESP_LOGI(TAG, "Initializing NVS");
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    char ssid[32] = {0};
    char pass[64] = {0};

    if (credentials_exist_in_nvs(ssid, pass)) {
        ESP_LOGI(TAG, "Found saved credentials, connecting to Wi-Fi");
        wifi_init_sta(ssid, pass);
        vTaskDelay(pdMS_TO_TICKS(5000)); // wait for DHCP
    } else {
        ESP_LOGI(TAG, "No saved credentials, starting AP portal");
        wifi_init_ap();
        start_webserver();
    }
}
