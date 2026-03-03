#include "device_control.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

static const char *TAG = "device_control";

// Example GPIO pins
#define PIN_LIGHT  4
#define PIN_RELAY  5
#define PIN_FAN    6

static esp_err_t device_handler(httpd_req_t *req)
{
    char buf[32];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Received: %s", buf);

    if (strcmp(buf, "light_on") == 0) {
        gpio_set_level(PIN_LIGHT, 1);
    } else if (strcmp(buf, "light_off") == 0) {
        gpio_set_level(PIN_LIGHT, 0);
    } else if (strcmp(buf, "relay_on") == 0) {
        gpio_set_level(PIN_RELAY, 1);
    } else if (strcmp(buf, "relay_off") == 0) {
        gpio_set_level(PIN_RELAY, 0);
    } else if (strcmp(buf, "fan_on") == 0) {
        gpio_set_level(PIN_FAN, 1);
    } else if (strcmp(buf, "fan_off") == 0) {
        gpio_set_level(PIN_FAN, 0);
    }

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

void register_device_routes(void)
{
    static httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri = {
            .uri      = "/device",
            .method   = HTTP_POST,
            .handler  = device_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri);
        ESP_LOGI(TAG, "Device control route registered at /device");
    } else {
        ESP_LOGE(TAG, "Failed to start device HTTP server");
    }

    // Configure GPIO outputs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL<<PIN_LIGHT)|(1ULL<<PIN_RELAY)|(1ULL<<PIN_FAN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
}
