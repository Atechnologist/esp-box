# main/CMakeLists.txt

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_rainmaker
        esp_schedule
        esp_speech_recognition
        led_strip
        qrcode
        ir_learn
        aht20
        at581x
)
