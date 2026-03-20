idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_wifi 
        esp_http_server 
        nvs_flash 
        driver 
        esp_lcd 
        lvgl
        espressif__esp-box
)
