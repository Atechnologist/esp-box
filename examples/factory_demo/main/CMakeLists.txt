# main/CMakeLists.txt
idf_component_register(
    SRCS
        "app/app_wifi.c"
        "gui/font/font_en_bold_10.c"
        "gui/font/font_en_64.c"
        "gui/ui_main.c"
        "gui/ui_net_config.c"
        # Add other .c files in main here
    INCLUDE_DIRS
        "."       # main/include or current folder
        "include"
    REQUIRES
        esp_box
        esp_box_3
        esp_box_lite
        esp_lcd_ili9341
        esp_lcd_touch
        esp_lcd_touch_gt911
        esp_lcd_touch_tt21100
        esp_lvgl_port
        lvgl
        esp_sr
        wifi_provisioning
        # Add other required components here
)
