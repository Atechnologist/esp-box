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
static bool wifi_connected = false;

/* ================= MQTT ================= */
#define MQTT_BROKER "mqtt://broker.hivemq.com"
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* ================= NVS ================= */
#define NVS_NAMESPACE "wifi"

static void save_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

static bool load_wifi(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 32, pass_len = 64;

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
static const char *html =
"<!DOCTYPE html><html><body>"
"<h2>ESP-BOX Setup</h2>"
"<form action=\"/save\">"
"SSID:<br><input name=\"s\"><br>"
"PASS:<br><input name=\"p\"><br><br>"
"<input type=\"submit\" value=\"Save\">"
"</form></body></html>";

/* ================= WEB ================= */
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char query[128];
    char ssid[32] = {0};
    char pass[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "s", ssid, sizeof(ssid));
        httpd_query_key_value(query, "p", pass, sizeof(pass));

        ESP_LOGI(TAG, "Saving SSID: %s", ssid);
        save_wifi(ssid, pass);
    }

    httpd_resp_sendstr(req, "Saved. Rebooting...");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();   // 🔥 safest method

    return ESP_OK;
}

static void start_web(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };

    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_GET,
        .handler = save_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);

    ESP_LOGI(TAG, "Web server started");
}

/* ================= MQTT ================= */
static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;

    if (event->event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI("MQTT", "Connected");
        esp_mqtt_client_publish(mqtt_client, "espbox/status", "ONLINE", 0, 1, 0);
    }
}

static void start_mqtt(void)
{
    if (mqtt_client) return;

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event, NULL);
    esp_mqtt_client_start(mqtt_client);
}

/* ================= WIFI ================= */
static void wifi_connect(const char *ssid, const char *pass)
{
    wifi_config_t cfg = {0};

    strcpy((char*)cfg.sta.ssid, ssid);
    strcpy((char*)cfg.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();

    esp_wifi_connect();   // ✅ ONLY PLACE CONNECT IS CALLED
}

static void wifi_ap(void)
{
    wifi_config_t ap = {
        .ap = {
            .ssid = "ESP-BOX",
            .ssid_len = strlen("ESP-BOX"),
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        }
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP Mode → 192.168.4.1");
}

/* ================= EVENTS ================= */
static void wifi_events(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGI(TAG, "Reconnecting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ip_event_got_ip_t *e = data;

        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));

        start_web();
        start_mqtt();
    }
}

/* ================= MAIN ================= */
void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_events, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_events, NULL, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi(ssid, pass)) {
        ESP_LOGI(TAG, "Connecting to saved WiFi...");
        wifi_connect(ssid, pass);
    } else {
        ESP_LOGI(TAG, "Starting AP...");
        wifi_ap();
        start_web();
    }
}
