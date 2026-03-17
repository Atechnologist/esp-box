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

/* WiFi credentials (temporary for testing) */
#define WIFI_SSID "glaatos"
#define WIFI_PASS "aatos2023"

/* Example GPIOs */
#define RELAY_GPIO  2
#define LIGHT_GPIO  4
#define BUZZER_GPIO 5


/* ================= WIFI ================= */

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "GOT IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void wifi_init_sta(void)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}


/* ================= WEB SERVER ================= */

esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* resp =
        "<html><body>"
        "<h1>ESP-Box Control</h1>"
        "<p><a href=\"/relay/on\">Relay ON</a></p>"
        "<p><a href=\"/relay/off\">Relay OFF</a></p>"
        "<p><a href=\"/light/on\">Light ON</a></p>"
        "<p><a href=\"/light/off\">Light OFF</a></p>"
        "<p><a href=\"/buzzer/on\">Buzzer ON</a></p>"
        "<p><a href=\"/buzzer/off\">Buzzer OFF</a></p>"
        "</body></html>";

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t gpio_control_handler(httpd_req_t *req)
{
    const char* path = req->uri;

    if (strcmp(path, "/relay/on") == 0) {
        gpio_set_level(RELAY_GPIO, 1);
    }
    else if (strcmp(path, "/relay/off") == 0) {
        gpio_set_level(RELAY_GPIO, 0);
    }
    else if (strcmp(path, "/light/on") == 0) {
        gpio_set_level(LIGHT_GPIO, 1);
    }
    else if (strcmp(path, "/light/off") == 0) {
        gpio_set_level(LIGHT_GPIO, 0);
    }
    else if (strcmp(path, "/buzzer/on") == 0) {
        gpio_set_level(BUZZER_GPIO, 1);
    }
    else if (strcmp(path, "/buzzer/off") == 0) {
        gpio_set_level(BUZZER_GPIO, 0);
    }

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &root);

        const char* uris[] =
        {
            "/relay/on",
            "/relay/off",
            "/light/on",
            "/light/off",
            "/buzzer/on",
            "/buzzer/off"
        };

        for (int i = 0; i < 6; i++)
        {
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


/* ================= MAIN ================= */

void app_main(void)
{
    ESP_LOGI(TAG, "ESP BOX HUB STARTING");

    /* Initialize NVS */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Initialize network stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Start WiFi */
    wifi_init_sta();

    /* Initialize GPIOs */
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LIGHT_GPIO);
    gpio_set_direction(LIGHT_GPIO, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "GPIOs initialized");

    /* Start Web Server */
    start_webserver();

    ESP_LOGI(TAG, "Web server started");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
