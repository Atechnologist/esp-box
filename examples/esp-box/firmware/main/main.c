static void wifi_init(void)
{
    static bool initialized = false;

    if (initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return;
    }
    initialized = true;

    /* SAFE NETIF INIT */
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) esp_netif_create_default_wifi_ap();

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif) esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP-BOX-SETUP",
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    /* ===== SAFE NVS LOAD ===== */
    nvs_handle_t nvs;
    char ssid[32] = {0}, pass[64] = {0};

    if (nvs_open("wifi", NVS_READONLY, &nvs) == ESP_OK) {
        size_t ssid_len = sizeof(ssid);
        size_t pass_len = sizeof(pass);

        if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) == ESP_OK) {
            nvs_get_str(nvs, "pass", pass, &pass_len);

            wifi_config_t sta_config = {0};
            strcpy((char*)sta_config.sta.ssid, ssid);
            strcpy((char*)sta_config.sta.password, pass);

            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

            ESP_LOGI(TAG, "Connecting to saved WiFi: %s", ssid);

            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
            }
        }
        nvs_close(nvs);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
    wifi_scan();
}
