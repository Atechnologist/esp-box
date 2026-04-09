#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "WEB";

/* ===================== HANDLERS ===================== */

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char resp[] =
        "<h1>ESP32-S3-BOX</h1>"
        "<p>Status: OK</p>"
        "<p><a href=\"/on\">Turn ON</a></p>"
        "<p><a href=\"/off\">Turn OFF</a></p>";

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t on_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "ON command received");
    httpd_resp_send(req, "ON", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t off_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OFF command received");
    httpd_resp_send(req, "OFF", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ===================== SERVER ===================== */

void web_server_start(void)
{
    ESP_LOGI(TAG, "Starting web server...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler
        };

        httpd_uri_t on = {
            .uri = "/on",
            .method = HTTP_GET,
            .handler = on_handler
        };

        httpd_uri_t off = {
            .uri = "/off",
            .method = HTTP_GET,
            .handler = off_handler
        };

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &on);
        httpd_register_uri_handler(server, &off);

        ESP_LOGI(TAG, "Web server started");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}
