# main/CMakeLists.txt

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_rainmaker
        esp_schedule
        espressif/esp-sr: 1.4.*
        led_strip
        qrcode
        ir_learn
        aht20
        at581x
)
