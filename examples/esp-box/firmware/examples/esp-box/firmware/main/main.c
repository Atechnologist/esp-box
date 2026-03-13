#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

static const char *TAG = "ESP_BOX";

// Example GPIOs for relay, light, buzzer
#define RELAY_GPIO  2
#define LIGHT_GPIO  4
#define BUZZER_GPIO 5

// Simple handler for root URL
esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* resp_str = "<html><body>"
                           "<h1>ESP-Box Control</h1>"
                           "<p><a href=\"/relay/on\">Relay ON</a></p>"
                           "<p><a href=\"/relay/off\">Relay OFF</a></p>"
                           "<p><a href=\"/light/on\">Light ON</a></p>"
                           "<p><a href=\"/light/off\">Light OFF</a></p>"
                           "<p><a href=\"/buzzer/on\">Buzzer ON</a></p>"
                           "<p><a href=\"/buzzer/off\">Buzzer OFF</a></p>"
                           "</body></html>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Generic GPIO control handler
esp_err_t gpio_control_handler(httpd_req_t *req)
{
    char buf[32];
    size_t len = httpd_req_get_url_query_len(req) + 1;
    if (len > sizeof(buf)) len = sizeof(buf);
    if (httpd_req_get_url_query_str(req, buf, len) == ESP_OK) {
        ESP_LOGI(TAG, "Query: %s", buf);
    }

    const char* path = req->uri;
    if (strcmp(path, "/relay/on") == 0) {
        gpio_set_level(RELAY_GPIO, 1);
    } else if (strcmp(path, "/relay/off") == 0) {
        gpio_set_level(RELAY_GPIO, 0);
    } else if (strcmp(path, "/light/on") == 0) {
        gpio_set_level(LIGHT_GPIO, 1);
    } else if (strcmp(path, "/light/off") == 0) {
        gpio_set_level(LIGHT_GPIO, 0);
    } else if (strcmp(path, "/buzzer/on") == 0) {
        gpio_set_level(BUZZER_GPIO, 1);
    } else if (strcmp(path, "/buzzer/off") == 0) {
        gpio_set_level(BUZZER_GPIO, 0);
    }
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Start the web server
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root);

        const char* uris[] = {"/relay/on","/relay/off","/light/on","/light/off","/buzzer/on","/buzzer/off"};
        for (int i = 0; i < 6; i++) {
            httpd_uri_t uri_handler = {
                .uri = uris[i],
                .method = HTTP_GET,
                .handler = gpio_control_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &uri_handler);
        }
    }
    return server;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-Box starting...");

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi started");

    // Initialize GPIOs
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LIGHT_GPIO);
    gpio_set_direction(LIGHT_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "GPIOs initialized");

    // Start web server
    start_webserver();
    ESP_LOGI(TAG, "Web server started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
