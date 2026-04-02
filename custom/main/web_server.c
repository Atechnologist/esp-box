#include "esp_http_server.h"
#include "esp_log.h"

// Factory device drivers (adjust names if needed)
#include "app_driver_light.h"
#include "app_driver_fan.h"
#include "app_driver_switch.h"
#include "app_driver_air.h"

static const char *TAG = "WEB";

// Simple HTML UI
static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h1>ESP-BOX HUB</h1>"

        "<h2>Light</h2>"
        "<a href=\"/light/on\">ON</a><br>"
        "<a href=\"/light/off\">OFF</a><br>"

        "<h2>Fan</h2>"
        "<a href=\"/fan/on\">ON</a><br>"
        "<a href=\"/fan/off\">OFF</a><br>"

        "<h2>Switch</h2>"
        "<a href=\"/switch/on\">ON</a><br>"
        "<a href=\"/switch/off\">OFF</a><br>"

        "<h2>Air</h2>"
        "<a href=\"/air/on\">ON</a><br>"
        "<a href=\"/air/off\">OFF</a><br>"

        "</body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// LIGHT
static esp_err_t light_on(httpd_req_t *req)
{
    app_driver_light_set(true);
    return httpd_resp_send(req, "Light ON", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t light_off(httpd_req_t *req)
{
    app_driver_light_set(false);
    return httpd_resp_send(req, "Light OFF", HTTPD_RESP_USE_STRLEN);
}

// FAN
static esp_err_t fan_on(httpd_req_t *req)
{
    app_driver_fan_set(true);
    return httpd_resp_send(req, "Fan ON", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t fan_off(httpd_req_t *req)
{
    app_driver_fan_set(false);
    return httpd_resp_send(req, "Fan OFF", HTTPD_RESP_USE_STRLEN);
}

// SWITCH
static esp_err_t switch_on(httpd_req_t *req)
{
    app_driver_switch_set(true);
    return httpd_resp_send(req, "Switch ON", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t switch_off(httpd_req_t *req)
{
    app_driver_switch_set(false);
    return httpd_resp_send(req, "Switch OFF", HTTPD_RESP_USE_STRLEN);
}

// AIR
static esp_err_t air_on(httpd_req_t *req)
{
    app_driver_air_set(true);
    return httpd_resp_send(req, "Air ON", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t air_off(httpd_req_t *req)
{
    app_driver_air_set(false);
    return httpd_resp_send(req, "Air OFF", HTTPD_RESP_USE_STRLEN);
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };

        httpd_uri_t uris[] = {
            {"/light/on", HTTP_GET, light_on},
            {"/light/off", HTTP_GET, light_off},
            {"/fan/on", HTTP_GET, fan_on},
            {"/fan/off", HTTP_GET, fan_off},
            {"/switch/on", HTTP_GET, switch_on},
            {"/switch/off", HTTP_GET, switch_off},
            {"/air/on", HTTP_GET, air_on},
            {"/air/off", HTTP_GET, air_off},
        };

        httpd_register_uri_handler(server, &root);

        for (int i = 0; i < sizeof(uris)/sizeof(uris[0]); i++)
        {
            httpd_register_uri_handler(server, &uris[i]);
        }

        ESP_LOGI(TAG, "Web server started");
    }
}
