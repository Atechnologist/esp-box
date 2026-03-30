#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"

#include "esp_http_server.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "driver/gpio.h"

/* ================= CONFIG ================= */
#define MQTT_BROKER "mqtt://broker.hivemq.com"
#define NVS_NAMESPACE "wifi"

#define RELAY_GPIO 41
#define RELAY_ON  0   // Active LOW
#define RELAY_OFF 1

static const char *TAG = "ESPBOX";

/* ================= STATE ================= */
static bool wifi_connected = false;
static int retry_count = 0;
static bool relay_state = false;

static httpd_handle_t server = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* MQTT topics */
static char device_id[32];
static char topic_cmd[64];
static char topic_state[64];

/* ================= RELAY ================= */
static void relay_set(bool on)
{
    relay_state = on;

    gpio_set_level(RELAY_GPIO, on ? RELAY_ON : RELAY_OFF);

    ESP_LOGI(TAG, "Relay: %s", on ? "ON" : "OFF");

    if (mqtt_client) {
        char msg[32];
        sprintf(msg, "{\"relay\":%s}", relay_state ? "true":"false");
        esp_mqtt_client_publish(mqtt_client, topic_state, msg, 0, 1, 0);
    }
}

/* ================= VOICE HOOK ================= */
void voice_command_handler(const char *cmd)
{
    ESP_LOGI("VOICE", "Command: %s", cmd);

    if (strcmp(cmd, "turn on relay") == 0) {
        relay_set(true);
    }
    else if (strcmp(cmd, "turn off relay") == 0) {
        relay_set(false);
    }
}

/* ================= WEB ================= */
static const char *html =
"<!DOCTYPE html><html><body>"
"<h2>ESP-BOX HUB</h2>"
"<h3>Relay: <span id='state'>...</span></h3>"
"<button onclick=\"fetch('/relay?state=1')\">ON</button>"
"<button onclick=\"fetch('/relay?state=0')\">OFF</button>"
"<script>"
"setInterval(async ()=>{"
"let r=await fetch('/status');"
"let j=await r.json();"
"document.getElementById('state').innerText=j.relay?'ON':'OFF';"
"},1000);"
"</script>"
"</body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char resp[64];
    sprintf(resp, "{\"relay\":%s}", relay_state ? "true":"false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

static esp_err_t relay_handler(httpd_req_t *req)
{
    char query[32], val[8];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "state", val, sizeof(val));
        relay_set(strcmp(val, "1") == 0);
    }

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static void start_web(void)
{
    if (server) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root = {.uri="/", .method=HTTP_GET, .handler=root_handler};
        httpd_uri_t relay = {.uri="/relay", .method=HTTP_GET, .handler=relay_handler};
        httpd_uri_t status = {.uri="/status", .method=HTTP_GET, .handler=status_handler};

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &relay);
        httpd_register_uri_handler(server, &status);
    }
}

/* ================= MQTT ================= */
static void handle_command(const char *data, int len)
{
    if (strstr(data, "on")) relay_set(true);
    if (strstr(data, "off")) relay_set(false);
}

static void mqtt_event(void *args, esp_event_base_t base,
                       int32_t event_id, void *data)
{
    esp_mqtt_event_handle_t event = data;

    if (event_id == MQTT_EVENT_CONNECTED) {
        esp_mqtt_client_subscribe(mqtt_client, topic_cmd, 0);
    }

    if (event_id == MQTT_EVENT_DATA) {
        handle_command(event->data, event->data_len);
    }
}

static void mqtt_start(void)
{
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
    esp_wifi_connect();
}

static void wifi_ap(void)
{
    wifi_config_t ap = {
        .ap = {
            .ssid = "ESP-BOX",
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4
        }
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();
}

static void wifi_events(void *arg, esp_event_base_t base,
                        int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        retry_count++;
        if (retry_count > 10) {
            wifi_ap();
            start_web();
        } else {
            esp_wifi_connect();
        }
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

    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_GPIO, RELAY_OFF);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(device_id, "espbox-%02X%02X%02X", mac[3], mac[4], mac[5]);

    sprintf(topic_cmd, "espbox/%s/cmd", device_id);
    sprintf(topic_state, "espbox/%s/state", device_id);

    wifi_ap();       // start in AP mode
    start_web();

    // Voice init will be added later
}
