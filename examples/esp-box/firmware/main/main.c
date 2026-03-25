#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

static const char *TAG = "wifi_prov_demo";

/* Optional: Callback when provisioning is complete */
static void wifi_prov_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    if (event_id == WIFI_PROV_STA_CONNECTED) {
        ESP_LOGI(TAG, "Provisioning successful! Connected to WiFi.");
    } else if (event_id == WIFI_PROV_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from WiFi.");
    }
}

/* Start WiFi provisioning in SoftAP mode */
static void start_provisioning(void) {
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    // Check if device is already provisioned
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning via SoftAP...");
        // Service name = ESP-BOX-PROV, no security key
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1,
            "ESP-BOX-PROV",   // SSID
            NULL               // password (optional)
        ));
    } else {
        ESP_LOGI(TAG, "Device already provisioned. Connecting to WiFi...");
        ESP_ERROR_CHECK(wifi_prov_mgr_cleanup());
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP-BOX WiFi Provisioning Demo");

    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize TCP/IP stack and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Initialize WiFi in station+AP mode
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_ap());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4. Start provisioning
    start_provisioning();

    ESP_LOGI(TAG, "Provisioning setup complete. Connect to ESP-BOX-PROV SSID to configure WiFi.");
}
