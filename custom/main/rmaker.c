#include "esp_rmaker_core.h"
#include "esp_rmaker_standard_devices.h"

void rmaker_start(void)
{
    esp_rmaker_config_t cfg = {
        .enable_time_sync = false,
    };

    esp_rmaker_node_t *node = esp_rmaker_node_init(&cfg, "ESP-BOX-S3", "Hub");

    esp_rmaker_device_t *light = esp_rmaker_device_create("Light", NULL, NULL);
    esp_rmaker_node_add_device(node, light);

    esp_rmaker_start();
}
