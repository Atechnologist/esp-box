#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "web";

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Webserver started");
    }

    return server;
}
