#include "mqtt_client.h"

static esp_mqtt_client_handle_t client;

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com"
    };

    client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_start(client);
}
