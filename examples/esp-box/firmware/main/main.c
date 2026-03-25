#include <stdio.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "ESP_BOX";

/* ========== HTML PAGE ========== */
static const char *html_form =
"<html><body>"
"<h2>ESP-BOX WiFi Setup</h2>"
"<form method=\"POST\" action=\"/connect\">"
"SSID:<br><input name=\"ssid\"><br>"
"Password:<br><input name=\"pass\" type=\"password\"><br><br>"
"<input type=\"submit\" value=\"Connect\">"
"</form>"
"</body></html>";

/* ========== WEB HANDLERS ========== */

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t connect_post(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf)-1);
    buf[len] = 0;

    char ssid[32] = {0};
    char pass[64] = {0};

    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    ESP_LOGI(TAG, "SSID: %s", ssid);

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();

    httpd_resp_sendstr(req, "Connecting... device will switch to your WiFi");

    return ESP_OK;
}

/* ========== WEB SERVER ========== */

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get
    };

    httpd_uri_t connect = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = connect_post
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &connect);
}

/* ========== WIFI INIT ========== */

void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX-PROV",
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Connect to ESP-BOX-PROV and open http://192.168.4.1");

    start_webserver();
}
