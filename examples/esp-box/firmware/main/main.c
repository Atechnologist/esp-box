#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "mqtt_client.h"

static const char *TAG = "ESPBOX";
static httpd_handle_t server = NULL;

/* ================= STATE ================= */
static bool wifi_connected = false;

/* ================= MQTT ================= */
#define MQTT_BROKER "mqtt://broker.hivemq.com"
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* ================= NVS ================= */
#define NVS_NAMESPACE "wifi"

/* ================= NVS SAVE ================= */
static void save_wifi_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "WiFi saved");
    }
}

/* ================= NVS LOAD ================= */
static bool load_wifi_credentials(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 32;
    size_t pass_len = 64;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;

    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK) {
        nvs_close(nvs);
        return false;
    }

    if (nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK) {
        nvs_close(nvs);
        return false;
    }

    nvs_close(nvs);
    return true;
}

/* ================= HTML ================= */
static const char *html_form =
"<!DOCTYPE html><html><body>"
"<h2>ESP-BOX WiFi Setup</h2>"
"<form action=\"/save\" method=\"get\">"
"SSID:<br><input name=\"s\"><br>"
"Password:<br><input name=\"p\" type=\"password\"><br><br>"
"<input type=\"submit\" value=\"Save\">"
"</form></body></html>";

/* ================= WEB ================= */
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

        save_wifi_credentials(ssid, pass);
    }

    httpd_resp_sendstr(req,
        "<html><body><h3>Saved!</h3>"
        "Device will reboot and connect.<br>"
        "</body></html>");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();   // 🔥 safest way

    return ESP_OK;
}

/* ================= WEB SERVER ================= */
static void start_webserver(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

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
    }
}

/* ================= MQTT ================= */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "Connected");
            esp_mqtt_client_subscribe(mqtt_client, "espbox/control", 0);
            esp_mqtt_client_publish(mqtt_client, "espbox/status", "ESP-BOX ONLINE", 0, 1, 0);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI("MQTT", "Received: %.*s", event->data_len, event->data);
            break;

        default:
            break;
    }
}

static void mqtt_start(void)
{
    if (mqtt_client) return;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

/* ================= WIFI ================= */
static void wifi_connect_sta(const char *ssid, const char *pass)
{
    wifi_config_t wifi_config = {0};

    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, pass);

    ESP_LOGI(TAG, "Starting WiFi...");

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    if (!wifi_connected) {
        ESP_LOGI(TAG, "Connecting...");
        esp_wifi_connect();   // 🔥 ONLY HERE
    }
}

static void wifi_start_ap(void)
{
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX",
            .ssid_len = strlen("ESP-BOX"),
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP Mode → 192.168.4.1");
}

/* ================= EVENTS ================= */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {

        wifi_connected = false;

        ESP_LOGI(TAG, "Disconnected → reconnecting...");

        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        wifi_connected = true;

        ESP_LOGI(TAG, "GOT IP: " IPSTR, IP2STR(&event->ip_info.ip));

        start_webserver();
        mqtt_start();
    }
}

/* ================= MAIN ================= */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi_credentials(ssid, pass)) {
        ESP_LOGI(TAG, "Loaded WiFi: %s", ssid);
        wifi_connect_sta(ssid, pass);
    } else {
        ESP_LOGI(TAG, "No WiFi → AP mode");
        wifi_start_ap();
        start_webserver();
    }
}
