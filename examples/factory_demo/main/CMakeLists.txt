# main/CMakeLists.txt

idf_component_register(
    SRCS
        main.c
        app/app_sr.c
        app/app_wifi.c
        app/app_button.c
        gui/ui_main.c
        gui/ui_net_config.c
        gui/font/font_en_bold_10.c
        gui/font/font_en_64.c
    INCLUDE_DIRS
        .                # main folder
        app              # headers in app/
        gui              # headers in gui/
        gui/font         # headers in gui/font/
    REQUIRES
        esp_sr           # speech recognition component
        bsp              # board support package
        lvgl             # LVGL graphics library
        wifi_provisioning # Wi-Fi provisioning library
        esp_box          # ESP-BOX core support
        esp_box_3        # ESP-BOX-3 support
        esp_box_lite     # ESP-BOX-LITE support
)
