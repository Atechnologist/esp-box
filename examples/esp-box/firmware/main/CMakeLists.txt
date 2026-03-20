idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES 
        esp_wifi 
        esp_http_server 
        nvs_flash 
        driver 
        esp_lcd 
        lvgl 
        esp_lvgl_port
        espressif__esp-box
)
