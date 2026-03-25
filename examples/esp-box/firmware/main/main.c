#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "ESPBOX";

/* -------- HTML PAGE -------- */
static const char *html_form =
"<!DOCTYPE html><html><body>"
"<h2>ESP-BOX WiFi Setup</h2>"
"<form action=\"/save\" method=\"post\">"
"SSID:<br><input name=\"s\"><br>"
"Password:<br><input name=\"p\" type=\"password\"><br><br>"
"<input type=\"submit\" value=\"Save\">"
"</form></body></html>";

/* -------- HANDLERS -------- */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    char ssid[32] = {0};
    char pass[64] = {0};

    sscanf(buf, "s=%31[^&]&p=%63s", ssid, pass);

    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "PASS: %s", pass);

    wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, pass);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    httpd_resp_sendstr(req, "Saved! Connecting... Check serial for IP.");

    return ESP_OK;
}

/* -------- WEB SERVER -------- */
static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler
    };

    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
}

/* -------- WIFI INIT -------- */
static void wifi_init(void)
{
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX",
            .ssid_len = strlen("ESP-BOX"),
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started: ESP-BOX");
}

/* -------- MAIN -------- */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();
    start_webserver();

    ESP_LOGI(TAG, "Open http://192.168.4.1");
}
