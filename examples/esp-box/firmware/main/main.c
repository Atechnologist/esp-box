#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "ESPBOX";
static httpd_handle_t server = NULL;

/* ================= HTML ================= */
static const char *html_form =
"<!DOCTYPE html><html><body>"
"<h2>ESP-BOX WiFi Setup</h2>"
"<form action=\"/save\" method=\"get\">"
"SSID:<br><input name=\"s\"><br>"
"Password:<br><input name=\"p\" type=\"password\"><br><br>"
"<input type=\"submit\" value=\"Save\">"
"</form></body></html>";

/* ================= HANDLERS ================= */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_get_handler(httpd_req_t *req)
{
    char query[128];
    char ssid[32] = {0};
    char pass[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {

        httpd_query_key_value(query, "s", ssid, sizeof(ssid));
        httpd_query_key_value(query, "p", pass, sizeof(pass));

        ESP_LOGI(TAG, "SSID: %s", ssid);
        ESP_LOGI(TAG, "PASS: %s", pass);

        wifi_config_t wifi_config = {0};
        strcpy((char *)wifi_config.sta.ssid, ssid);
        strcpy((char *)wifi_config.sta.password, pass);

        // 🔥 CRITICAL FIX
        esp_wifi_disconnect();

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }

    httpd_resp_sendstr(req,
        "<html><body>"
        "<h3>Saved!</h3>"
        "Device is connecting...<br>"
        "Reconnect to your WiFi and open the new IP."
        "</body></html>");

    return ESP_OK;
}

/* ================= WEB SERVER ================= */
static void start_webserver(void)
{
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.stack_size = 8192;
    config.max_uri_handlers = 10;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler
        };

        httpd_uri_t save = {
            .uri = "/save",
            .method = HTTP_GET,
            .handler = save_get_handler
        };

        httpd_uri_t favicon = {
            .uri = "/favicon.ico",
            .method = HTTP_GET,
            .handler = favicon_handler
        };

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &save);
        httpd_register_uri_handler(server, &favicon);

        ESP_LOGI(TAG, "Web server started");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}

/* ================= WIFI EVENTS ================= */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Retrying connection...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        ESP_LOGI(TAG, "GOT IP: " IPSTR, IP2STR(&event->ip_info.ip));

        start_webserver(); // start server on STA network
    }
}

/* ================= WIFI INIT ================= */
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

    ESP_LOGI(TAG, "AP started: ESP-BOX (192.168.4.1)");
}

/* ================= MAIN ================= */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_init();

    // Start server for AP mode
    start_webserver();

    ESP_LOGI(TAG, "Connect to ESP-BOX and open http://192.168.4.1");
}
