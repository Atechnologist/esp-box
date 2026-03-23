#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASS "YOUR_PASSWORD"
#define LED_GPIO 2

static const char *TAG = "ESP_BOX";

/* ---------------- WEB SERVER ---------------- */

static const char html_page[] =
"<html><body><h1>ESP BOX HUB</h1>"
"<p><a href='/on'><button>LED ON</button></a></p>"
"<p><a href='/off'><button>LED OFF</button></a></p>"
"</body></html>";

esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_on_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 1);
    httpd_resp_sendstr(req, "LED ON");
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 0);
    httpd_resp_sendstr(req, "LED OFF");
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };

    httpd_uri_t on = {
        .uri = "/on",
        .method = HTTP_GET,
        .handler = led_on_handler
    };

    httpd_uri_t off = {
        .uri = "/off",
        .method = HTTP_GET,
        .handler = led_off_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &on);
    httpd_register_uri_handler(server, &off);
}

/* ---------------- WIFI ---------------- */

void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        }
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

/* ---------------- MAIN ---------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "ESP BOX START");

    nvs_flash_init();

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    wifi_init();

    vTaskDelay(pdMS_TO_TICKS(5000)); // wait for IP

    start_webserver();

    ESP_LOGI(TAG, "Web server started");
}
