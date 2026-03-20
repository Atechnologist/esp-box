#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_http_server.h"
#include "driver/gpio.h"

/* ESP-BOX */
#include "bsp/esp_box.h"
#include "bsp/esp_box_lvgl.h"

#define MAX_APS 20

static const char *TAG = "ESP_BOX";
httpd_handle_t server = NULL;
static int retry_count = 0;

/* ================= NVS ================= */

void save_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

bool load_wifi(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 33;
    size_t pass_len = 65;

    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK)
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

/* ================= WIFI SCAN ================= */

char scan_result[1024];

void wifi_scan()
{
    uint16_t number = MAX_APS;
    wifi_ap_record_t ap_info[MAX_APS];

    memset(scan_result, 0, sizeof(scan_result));

    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&number, ap_info);

    strcat(scan_result, "[");

    for (int i = 0; i < number; i++) {
        char entry[64];
        snprintf(entry, sizeof(entry),
                 "\"%s\"%s",
                 ap_info[i].ssid,
                 (i < number - 1) ? "," : "");
        strcat(scan_result, entry);
    }

    strcat(scan_result, "]");
}

/* ================= WEB ================= */

esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h2>ESP-BOX Setup</h2>"

        "<button onclick='scanWifi()'>Scan WiFi</button><br><br>"
        "<select id='ssid'></select><br><br>"

        "Password:<br><input id='pass'><br><br>"
        "<button onclick='saveWifi()'>Save</button>"

        "<script>"
        "function scanWifi(){"
        "fetch('/scan').then(r=>r.json()).then(data=>{"
        "let s=document.getElementById('ssid');"
        "s.innerHTML='';"
        "data.forEach(w=>{"
        "let o=document.createElement('option');"
        "o.text=w; s.add(o);"
        "});"
        "});"
        "}"
        "function saveWifi(){"
        "let ssid=document.getElementById('ssid').value;"
        "let pass=document.getElementById('pass').value;"
        "location='/save?ssid='+ssid+'&pass='+pass;"
        "}"
        "</script>"

        "</body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t scan_handler(httpd_req_t *req)
{
    wifi_scan();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, scan_result, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t save_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));

    char ssid[32] = {0};
    char pass[64] = {0};

    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    save_wifi(ssid, pass);

    httpd_resp_send(req, "Saved! Rebooting...", HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

esp_err_t captive_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ================= WEB SERVER ================= */

void start_webserver()
{
    if (server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t root = {.uri="/", .method=HTTP_GET, .handler=root_handler};
    httpd_uri_t scan = {.uri="/scan", .method=HTTP_GET, .handler=scan_handler};
    httpd_uri_t save = {.uri="/save", .method=HTTP_GET, .handler=save_handler};

    httpd_uri_t captive1 = {.uri="/generate_204", .method=HTTP_GET, .handler=captive_handler};
    httpd_uri_t captive2 = {.uri="/hotspot-detect.html", .method=HTTP_GET, .handler=captive_handler};

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &scan);
    httpd_register_uri_handler(server, &save);
    httpd_register_uri_handler(server, &captive1);
    httpd_register_uri_handler(server, &captive2);

    ESP_LOGI(TAG, "Web server started");
}

/* ================= WIFI ================= */

void start_ap_mode()
{
    esp_netif_create_default_wifi_ap();

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

    start_webserver();
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {

        if (retry_count < 5) {
            retry_count++;
            esp_wifi_connect();
        } else {
            start_ap_mode();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));

        start_webserver();
    }
}

void start_sta_mode(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, NULL);

    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

/* ================= UI (ESP-BOX SCREEN) ================= */

void btn_event(lv_event_t *e)
{
    ESP_LOGI("UI", "Touch button pressed");
}

void ui_init()
{
    bsp_display_start();
    bsp_display_backlight_on();

    bsp_display_lock(0);

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "ESP-BOX READY");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 200, 80);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);

    lv_obj_add_event_cb(btn, btn_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "SCAN WIFI");
    lv_obj_center(btn_label);

    bsp_display_unlock();
}

/* ================= MAIN ================= */

void app_main(void)
{
    ESP_LOGI(TAG, "START");

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    lv_init();
    ui_init();

    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi(ssid, pass))
    {
        start_sta_mode(ssid, pass);
    }
    else
    {
        start_ap_mode();
    }
}
