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
#include "nvs.h"
#include "driver/gpio.h"

#define LED_GPIO 2

static const char *TAG = "ESP_BOX";
httpd_handle_t server = NULL;

/* ================= NVS ================= */

void save_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

bool load_wifi(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 33;
    size_t pass_len = 65;

    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK)
        return false;

    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK ||
        nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK)
    {
        nvs_close(nvs);
        return false;
    }

    nvs_close(nvs);
    return true;
}

/* ================= WEB ================= */

esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h2>ESP-BOX</h2>"
        "<form action=\"/save\">"
        "SSID:<input name=\"ssid\"><br>"
        "PASS:<input name=\"pass\"><br>"
        "<input type=\"submit\" value=\"Save\">"
        "</form>"
        "<br><a href=\"/on\">LED ON</a>"
        "<br><a href=\"/off\">LED OFF</a>"
        "</body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t save_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));

    char ssid[32] = {0};
    char pass[64] = {0};

    sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass);

    ESP_LOGI(TAG, "Saving WiFi: %s", ssid);

    save_wifi(ssid, pass);

    httpd_resp_send(req, "Saved! Rebooting...", HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

esp_err_t led_on_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 1);
    httpd_resp_send(req, "LED ON", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 0);
    httpd_resp_send(req, "LED OFF", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver()
{
    if (server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t root = {.uri="/", .method=HTTP_GET, .handler=root_handler};
    httpd_uri_t save = {.uri="/save", .method=HTTP_GET, .handler=save_handler};
    httpd_uri_t on = {.uri="/on", .method=HTTP_GET, .handler=led_on_handler};
    httpd_uri_t off = {.uri="/off", .method=HTTP_GET, .handler=led_off_handler};

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
    httpd_register_uri_handler(server, &on);
    httpd_register_uri_handler(server, &off);

    ESP_LOGI(TAG, "Web server started");
}

/* ================= WIFI ================= */

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Retrying...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        start_webserver();  // ✅ always start here
    }
}

void wifi_sta(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, NULL);

    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

/* ================= MAIN ================= */

void app_main(void)
{
    ESP_LOGI(TAG, "ESP START");

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi(ssid, pass))
    {
        ESP_LOGI(TAG, "Using saved WiFi");
        wifi_sta(ssid, pass);
    }
    else
    {
        ESP_LOGI(TAG, "No WiFi → using default (edit in code)");

        wifi_sta("YOUR_WIFI", "YOUR_PASSWORD"); // fallback
    }
}
