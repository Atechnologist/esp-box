#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_http_server.h"
#include "mqtt_client.h"
#include "cJSON.h"

/* ================= CONFIG ================= */
#define MQTT_BROKER "mqtt://broker.hivemq.com"
#define NVS_NAMESPACE "wifi"

static const char *TAG = "ESPBOX";

/* ================= STATE ================= */
static bool wifi_connected = false;
static httpd_handle_t server = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* MQTT topics */
static char device_id[32];
static char topic_cmd[64];
static char topic_state[64];
static char topic_status[64];

/* ================= NVS ================= */
static void save_wifi(const char *ssid, const char *pass)
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

static bool load_wifi(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t ssid_len = 32, pass_len = 64;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;

    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK ||
        nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK) {
        nvs_close(nvs);
        return false;
    }

    nvs_close(nvs);
    return true;
}

/* ================= WEB ================= */
static const char *html =
"<!DOCTYPE html><html><body>"
"<h2>ESP-BOX Setup</h2>"
"<form action=\"/save\">"
"SSID:<br><input name=\"s\"><br>"
"PASS:<br><input name=\"p\" type=\"password\"><br><br>"
"<input type=\"submit\" value=\"Save\">"
"</form></body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char query[128], ssid[32] = {0}, pass[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "s", ssid, sizeof(ssid));
        httpd_query_key_value(query, "p", pass, sizeof(pass));

        save_wifi(ssid, pass);
    }

    httpd_resp_sendstr(req, "Saved. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char resp[128];
    sprintf(resp, "{\"wifi\":%s}", wifi_connected ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);

    return ESP_OK;
}

static void start_web(void)
{
    if (server) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root = {
            .uri = "/", .method = HTTP_GET, .handler = root_handler
        };

        httpd_uri_t save = {
            .uri = "/save", .method = HTTP_GET, .handler = save_handler
        };

        httpd_uri_t status = {
            .uri = "/status", .method = HTTP_GET, .handler = status_handler
        };

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &save);
        httpd_register_uri_handler(server, &status);
    }
}

/* ================= MQTT ================= */

static void mqtt_publish_state(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "wifi", wifi_connected);
    cJSON_AddStringToObject(root, "id", device_id);

    char *msg = cJSON_PrintUnformatted(root);

    esp_mqtt_client_publish(mqtt_client, topic_state, msg, 0, 1, 0);

    cJSON_Delete(root);
    free(msg);
}

static void handle_command(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) return;

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cmd || !cJSON_IsString(cmd)) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(cmd->valuestring, "reboot") == 0) {
        esp_mqtt_client_publish(mqtt_client, topic_status, "REBOOTING", 0, 1, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    else if (strcmp(cmd->valuestring, "status") == 0) {
        mqtt_publish_state();
    }

    else if (strcmp(cmd->valuestring, "wifi_reset") == 0) {
        nvs_flash_erase();
        esp_restart();
    }

    else if (strcmp(cmd->valuestring, "ping") == 0) {
        esp_mqtt_client_publish(mqtt_client, topic_status, "pong", 0, 1, 0);
    }

    cJSON_Delete(root);
}

static void mqtt_event(void *args, esp_event_base_t base,
                       int32_t event_id, void *data)
{
    esp_mqtt_event_handle_t event = data;

    switch (event_id) {

    case MQTT_EVENT_CONNECTED:
        esp_mqtt_client_subscribe(mqtt_client, topic_cmd, 0);
        esp_mqtt_client_publish(mqtt_client, topic_status, "ONLINE", 0, 1, 1);
        mqtt_publish_state();
        break;

    case MQTT_EVENT_DATA:
        if (strncmp(event->topic, topic_cmd, event->topic_len) == 0) {
            handle_command(event->data, event->data_len);
        }
        break;

    default:
        break;
    }
}

static void mqtt_start(void)
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

    strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strncpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();
}

static void wifi_ap(void)
{
    wifi_config_t ap = {
        .ap = {
            .ssid = "ESP-BOX",
            .ssid_len = 7,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4
        }
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP mode: 192.168.4.1");
}

static void wifi_events(void *arg, esp_event_base_t base,
                        int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect();
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;

        start_web();
        mqtt_start();
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

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_events, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_events, NULL, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_ps(WIFI_PS_NONE);

    /* Device ID */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    sprintf(device_id, "espbox-%02X%02X%02X", mac[3], mac[4], mac[5]);

    sprintf(topic_cmd, "espbox/%s/cmd", device_id);
    sprintf(topic_state, "espbox/%s/state", device_id);
    sprintf(topic_status, "espbox/%s/status", device_id);

    char ssid[32] = {0}, pass[64] = {0};

    if (load_wifi(ssid, pass)) {
        wifi_connect(ssid, pass);
    } else {
        wifi_ap();
        start_web();
    }
}
