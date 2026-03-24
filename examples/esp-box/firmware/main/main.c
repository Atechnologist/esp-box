#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

/* ---------------- WIFI INIT ---------------- */

void wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

/* ---------------- AP MODE ---------------- */

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

    ESP_LOGI(TAG, "AP MODE READY");
}

/* ---------------- ROOT PAGE ---------------- */

esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
    "<h1>ESP-BOX V2</h1>"
    "<p>If you see this → NEW firmware is running ✅</p>"

    "<button onclick='scanWifi()'>Scan WiFi</button><br><br>"

    "<div id='list'>No scan yet</div><br>"

    "SSID:<input id='ssid'><br>"
    "PASS:<input id='pass' type='password'><br><br>"

    "<button onclick='saveWifi()'>SAVE WIFI</button><br><br>"

    "<hr>"

    "<a href='/on'>LED ON</a><br>"
    "<a href='/off'>LED OFF</a>"

    "<script>"

    "function scanWifi(){"
    "fetch('/scan').then(r=>r.text()).then(t=>{"
    "document.getElementById('list').innerHTML = t;"
    "});"
    "}"

    "function saveWifi(){"
    "let s = document.getElementById('ssid').value;"
    "let p = document.getElementById('pass').value;"

    "fetch('/save',{"
    "method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:'ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)"
    "}).then(r=>r.text()).then(t=>alert(t));"
    "}"

    "</script>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ---------------- WIFI SCAN ---------------- */

esp_err_t scan_handler(httpd_req_t *req)
{
    uint16_t ap_count = 20;
    wifi_ap_record_t ap_info[20];

    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    char result[1024] = "";

    for (int i = 0; i < ap_count; i++) {
        strcat(result, ap_info[i].ssid);
        strcat(result, "<br>");
    }

    httpd_resp_send(req, result, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ---------------- SAVE WIFI ---------------- */

esp_err_t save_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) {
        httpd_resp_sendstr(req, "ERROR");
        return ESP_FAIL;
    }

    buf[len] = '\0';

    ESP_LOGI(TAG, "RAW: %s", buf);

    char ssid[32] = {0};
    char pass[64] = {0};

    sscanf(buf, "ssid=%31[^&]&pass=%63[^&]", ssid, pass);

    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "PASS: %s", pass);

    nvs_handle_t nvs;
    nvs_open("wifi", NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", pass);
    nvs_commit(nvs);
    nvs_close(nvs);

    httpd_resp_sendstr(req, "Saved! Rebooting...");

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

/* ---------------- LED ---------------- */

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

/* ---------------- WEB SERVER ---------------- */

void start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
        httpd_uri_t scan = { .uri = "/scan", .method = HTTP_GET, .handler = scan_handler };
        httpd_uri_t save = { .uri = "/save", .method = HTTP_POST, .handler = save_handler };
        httpd_uri_t on   = { .uri = "/on", .method = HTTP_GET, .handler = on_handler };
        httpd_uri_t off  = { .uri = "/off", .method = HTTP_GET, .handler = off_handler };

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &scan);
        httpd_register_uri_handler(server, &save);
        httpd_register_uri_handler(server, &on);
        httpd_register_uri_handler(server, &off);

        ESP_LOGI(TAG, "Web server started");
    }
}

/* ---------------- MAIN ---------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "BOOT V2");

    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    wifi_init();
    start_ap();

    vTaskDelay(pdMS_TO_TICKS(2000)); // stabilize

    start_webserver();
}
