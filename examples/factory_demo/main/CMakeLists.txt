# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS 
        "main.c"
        "app/app_wifi.c"
        "app/app_sr.c"
        "gui/ui_main.c"
        "gui/font/font_en_bold_10.c"
        "gui/font/font_en_64.c"
        "settings.c"
    INCLUDE_DIRS 
        "include"
        "app"
        "gui"
    REQUIRES
        esp-box
        esp-box-3
        esp-box-lite
        lvgl
        esp-dsp
        esp-lcd-ili9341
        esp-lcd-touch
        led_strip
        mdns
        wifi_provisioning
)
