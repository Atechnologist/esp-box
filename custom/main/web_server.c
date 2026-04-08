#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "WEB";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char resp[] =
        "<h1>ESP32-S3-BOX</h1>"
        "<p>Web Server OK</p>";

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void web_server_start(void)
{
    ESP_LOGI(TAG, "Starting web server...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &uri);

        ESP_LOGI(TAG, "Web server started");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}
