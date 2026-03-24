#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#define MAX_APs 20
static const char *TAG = "ESP_BOX";

typedef struct {
    char ssid[32];
    int rssi;
} wifi_ap_t;

static wifi_ap_t wifi_list[MAX_APs];
static int wifi_count = 0;

/* Forward declarations */
static void wifi_init_ap(void);
static void wifi_init_sta(const char *ssid, const char *pass);
static void scan_wifi(void);
static esp_err_t index_get_handler(httpd_req_t *req);
static esp_err_t save_post_handler(httpd_req_t *req);
static void start_webserver(void);

/* HTML page with dropdown of scanned Wi-Fi networks */
static char html_ap[2048];
static void build_html_page(void) {
    strcpy(html_ap,
        "<html><body><h2>ESP-BOX WiFi Config</h2>"
        "<form action=\"/save\" method=\"POST\">"
        "SSID: <select name=\"ssid\">");

    for (int i = 0; i < wifi_count; i++) {
        char option[128];
        snprintf(option, sizeof(option),
                 "<option value=\"%s\">%s (%ddBm)</option>",
                 wifi_list[i].ssid, wifi_list[i].ssid, wifi_list[i].rssi);
        strcat(html_ap, option);
    }

    strcat(html_ap,
        "</select><br>"
        "Password: <input type=\"password\" name=\"pass\"><br>"
        "<input type=\"submit\" value=\"Save\">"
        "</form></body></html>");
}

/* HTTP GET handler for "/" */
static esp_err_t index_get_handler(httpd_req_t *req) {
    build_html_page();
    httpd_resp_send(req, html_ap, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* HTTP POST handler for "/save" */
static esp_err_t save_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret, remaining = req->content_len;
    char ssid[32] = {0}, pass[64] = {0};

    // Read POST data
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)-1))) <= 0) {
            return ESP_FAIL;
        }
        buf[ret] = 0;
        remaining -= ret;

        char *ssid_ptr = strstr(buf, "ssid=");
        char *pass_ptr = strstr(buf, "pass=");
        if (ssid_ptr) sscanf(ssid_ptr, "ssid=%31[^&]", ssid);
        if (pass_ptr) sscanf(pass_ptr, "pass=%63s", pass);
    }

    ESP_LOGI(TAG, "Selected SSID: %s, PASS: %s", ssid, pass);

    // Save to NVS
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    httpd_resp_sendstr(req, "Credentials saved! Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

/* Start simple HTTP server */
static void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_index = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_get_handler
        };
        httpd_register_uri_handler(server, &uri_index);

        httpd_uri_t uri_save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_post_handler
        };
        httpd_register_uri_handler(server, &uri_save);
    }
}

/* Wi-Fi event handler */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch(event_id) {
            case WIFI_EVENT_SCAN_DONE: {
                uint16_t ap_num = MAX_APs;
                wifi_ap_record_t ap_info[MAX_APs];
                esp_wifi_scan_get_ap_records(&ap_num, ap_info);
                wifi_count = ap_num;
                for (int i=0; i<ap_num; i++) {
                    strncpy(wifi_list[i].ssid, (char *)ap_info[i].ssid, sizeof(wifi_list[i].ssid)-1);
                    wifi_list[i].rssi = ap_info[i].rssi;
                }
                ESP_LOGI(TAG, "Scan done: %d APs found", wifi_count);
                break;
            }
            default: break;
        }
    }
}

/* Scan Wi-Fi networks */
static void scan_wifi(void) {
    wifi_scan_config_t scan_config = {0};
    esp_wifi_scan_start(&scan_config, true);
}

/* Initialize Wi-Fi AP for configuration */
static void wifi_init_ap(void) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_wifi_set_mode(WIFI_MODE_APSTA);  // Allow both AP + STA

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX-SETUP",
            .ssid_len = 0,
            .channel = 1,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    // Scan networks
    scan_wifi();
}

/* Initialize Wi-Fi STA if credentials saved */
static void wifi_init_sta_from_nvs(void) {
    nvs_handle_t nvs;
    char ssid[32] = {0}, pass[64] = {0};

    if (nvs_open("wifi", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_str(nvs, "ssid", ssid, sizeof(ssid));
        nvs_get_str(nvs, "pass", pass, sizeof(pass));
        nvs_close(nvs);
    }

    if (strlen(ssid) > 0) {
        ESP_LOGI(TAG, "Connecting to STA SSID: %s", ssid);
        esp_netif_create_default_wifi_sta();
        wifi_config_t sta_config = {0};
        strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid)-1);
        strncpy((char *)sta_config.sta.password, pass, sizeof(sta_config.sta.password)-1);
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        esp_wifi_start();
        esp_wifi_connect();
    }
}

void app_main(void) {
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Init TCP/IP and event loop
    esp_netif_init();
    esp_event_loop_create_default();

    // Start AP + scan
    wifi_init_ap();

    // Try STA mode from saved credentials
    wifi_init_sta_from_nvs();

    // Start webserver
    start_webserver();

    ESP_LOGI(TAG, "ESP-BOX ready! Connect to AP 'ESP-BOX-SETUP' and open browser to configure Wi-Fi.");
}
