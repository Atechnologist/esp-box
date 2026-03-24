#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#define TAG "ESP_BOX"

//------------------------
// HTML pages
//------------------------
const char *html_ap = "<!DOCTYPE html><html><body>"
"<h2>ESP-BOX WiFi Setup</h2>"
"<form action=\"/save\" method=\"POST\">"
"SSID: <input type=\"text\" name=\"ssid\"><br>"
"PASS: <input type=\"password\" name=\"pass\"><br>"
"<button type=\"submit\">Save</button>"
"</form>"
"<h3>Available Networks:</h3>"
"<div id=\"scan\"></div>"
"<script>"
"fetch('/scan').then(resp=>resp.text()).then(data=>{document.getElementById('scan').innerHTML=data});"
"</script>"
"</body></html>";

//------------------------
// NVS save
//------------------------
void save_wifi_credentials(const char* ssid, const char* pass) {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "Credentials saved");
    }
}

//------------------------
// HTTP Handlers
//------------------------
esp_err_t save_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;

    char ssid[32] = {0}, pass[64] = {0};
    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    save_wifi_credentials(ssid, pass);

    const char* resp = "<html><body>Saved! Rebooting...</body></html>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

esp_err_t scan_handler(httpd_req_t *req) {
    wifi_scan_config_t scan_config = {0};
    scan_config.show_hidden = true;

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 20) ap_count = 20;

    wifi_ap_record_t ap_info[20];
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    char result[1024] = "";
    for (int i = 0; i < ap_count; i++) {
        strcat(result, (char*)ap_info[i].ssid);
        strcat(result, "<br>");
    }

    if (ap_count == 0) strcpy(result, "No networks found");
    httpd_resp_send(req, result, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

//------------------------
// Start Web Server
//------------------------
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_index = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = [](httpd_req_t *req) { httpd_resp_send(req, html_ap, HTTPD_RESP_USE_STRLEN); return ESP_OK; }
        };
        httpd_register_uri_handler(server, &uri_index);

        httpd_uri_t uri_scan = {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = scan_handler
        };
        httpd_register_uri_handler(server, &uri_scan);

        httpd_uri_t uri_save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_handler
        };
        httpd_register_uri_handler(server, &uri_save);
    }

    return server;
}

//------------------------
// Start AP
//------------------------
void start_ap(void) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.ap.ssid, "ESP-BOX");
    wifi_config.ap.ssid_len = strlen("ESP-BOX");
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started, SSID: ESP-BOX");
    start_webserver();
}

//------------------------
// Start STA from NVS
//------------------------
void start_sta(void) {
    char ssid[32] = {0};
    char pass[64] = {0};

    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGI(TAG, "No saved WiFi");
        start_ap();
        return;
    }

    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);

    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK) {
        ESP_LOGI(TAG, "No SSID found");
        nvs_close(nvs);
        start_ap();
        return;
    }

    nvs_get_str(nvs, "pass", pass, &pass_len);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Connecting to: %s", ssid);

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, pass);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "STA started, connecting...");
}

//------------------------
// Main
//------------------------
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    start_sta(); // Try STA first
}
