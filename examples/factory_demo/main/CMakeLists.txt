# examples/factory_demo/CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

# Set the project name and the ESP-IDF target
set(PROJECT_NAME "factory_demo")
project(${PROJECT_NAME} LANGUAGES C CXX ASM)

# Register the main component
idf_component_register(
    SRCS 
        "main/main.c"
        "main/app/app_wifi.c"
        "main/app/app_sr.c"
        "main/gui/ui_main.c"
        "main/gui/font/font_en_bold_10.c"
        "main/gui/font/font_en_64.c"
        # add any other source files here
    INCLUDE_DIRS 
        "main/include"
        "main/app"
        "main/gui"
    REQUIRES
        espressif/esp_lcd_ili9341
        espressif/esp_lcd_touch
        espressif/esp_lcd_touch_gt911
        lvgl/lvgl
        espressif/esp_dsp
        espressif/mdns
        espressif/led_strip
        espressif/esp_codec_dev
        espressif/ir_learn
        espressif/qrcode
        espressif/esp_rainmaker
        wifi_provisioning
        esp-box-3
)
