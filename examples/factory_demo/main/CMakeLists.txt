file(GLOB_RECURSE app_sources "*.c")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS
        "."
        "app"
        "gui"
        "gui/font"
    REQUIRES
        esp_lvgl_port
        bsp
        esp_rainmaker
        esp_schedule
        led_strip
        qrcode
        ir_learn
        aht20
        at581x
)
