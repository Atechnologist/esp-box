# main/CMakeLists.txt

file(GLOB_RECURSE app_sources
    "*.c"
)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS
        "."
        "app"
    REQUIRES
        bsp
        esp_sr
        esp_rainmaker
        esp_schedule
        led_strip
        qrcode
        ir_learn
        aht20
        at581x
)
