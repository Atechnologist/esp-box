#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#define MAX_APs 15

static const char *TAG = "ESP_BOX";

/* ================= DATA ================= */
typedef struct {
    char ssid[32];
    int rssi;
} wifi_ap_t;

static wifi_ap_t wifi_list[MAX_APs];
static int wifi_count = 0;
static char sta_ip[16] = "Not connected";

/* ================= WIFI EVENTS ================= */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        uint16_t ap_num = MAX_APs;
        wifi_ap_record_t ap_info[MAX_APs];

        if (esp_wifi_scan_get_ap_records(&ap_num, ap_info) == ESP_OK) {
            wifi_count = ap_num;

            for (int i = 0; i < ap_num; i++) {
                strncpy(wifi_list[i].ssid, (char*)ap_info[i].ssid, sizeof(wifi_list[i].ssid) - 1);
                wifi_list[i].rssi = ap_info[i].rssi;
            }
        }

        ESP_LOGI(TAG, "Scan done: %d APs", wifi_count);
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(sta_ip, sizeof(sta_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connected! IP: %s", sta_ip);
    }
}

/* ================= WIFI SCAN ================= */
static void wifi_scan(void)
{
    wifi_scan_config_t scan_config = {0};
    esp_wifi_scan_start(&scan_config, true);
}

/* ================= HTML ================= */
static char html[4096];

static void build_html(void)
{
    int len = snprintf(html, sizeof(html),
        "<html><body><h2>ESP-BOX WiFi Setup</h2>"
        "<form action=\"/save\" method=\"POST\">"
        "SSID: <select name=\"ssid\">");

    for (int i = 0; i < wifi_count; i++) {
        len += snprintf(html + len, sizeof(html) - len,
            "<option value=\"%s\">%s (%ddBm)</option>",
            wifi_list[i].ssid,
            wifi_list[i].ssid,
            wifi_list[i].rssi);

        if (len >= sizeof(html)) break;
    }

    snprintf(html + len, sizeof(html) - len,
        "</select><br>"
        "Password: <input type=\"password\" name=\"pass\"><br>"
        "<input type=\"submit\" value=\"Save\">"
        "</form><br>"
        "<b>STA IP: %s</b>"
        "</body></html>", sta_ip);
}

/* ================= HTTP ================= */
static esp_err_t root_handler(httpd_req_t *req)
{
    build_html();
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[128];

    int len = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    if (len <= 0) return ESP_FAIL;

    buf[len] = 0;

    char ssid[32] = {0}, pass[64] = {0};
    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    ESP_LOGI(TAG, "Saving SSID: %s", ssid);

    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    httpd_resp_sendstr(req, "Saved. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();

    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_handler
        };
        httpd_register_uri_handler(server, &save);
    }

    ESP_LOGI(TAG, "Webserver started");
}

/* ================= WIFI INIT ================= */
static void wifi_init(void)
{
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX-SETUP",
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    /* START WIFI FIRST */
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Load saved credentials */
    nvs_handle_t nvs;
    char ssid[32] = {0}, pass[64] = {0};

    if (nvs_open("wifi", NVS_READONLY, &nvs) == ESP_OK) {
        size_t ssid_len = sizeof(ssid);
        size_t pass_len = sizeof(pass);

        if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) == ESP_OK) {
            nvs_get_str(nvs, "pass", pass, &pass_len);

            wifi_config_t sta_config = {0};
            strcpy((char*)sta_config.sta.ssid, ssid);
            strcpy((char*)sta_config.sta.password, pass);

            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
            esp_wifi_connect();

            ESP_LOGI(TAG, "Connecting to saved WiFi: %s", ssid);
        }
        nvs_close(nvs);
    }

    /* Delay before scan (important) */
    vTaskDelay(pdMS_TO_TICKS(2000));
    wifi_scan();
}

/* ================= MAIN ================= */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();
    start_webserver();

    ESP_LOGI(TAG, "READY: Connect to ESP-BOX-SETUP → http://192.168.4.1");
}
