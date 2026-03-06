file(GLOB_RECURSE LV_DEMOS_SOURCES ./*.c)

idf_component_register(
    SRCS
        ${LV_DEMOS_SOURCES}
        wifi_connect.c
        web_server.c
        device_control.c

    INCLUDE_DIRS
        "."
        "app"
        "gui"
        "gui/font"
        "gui/image"
        "rmaker"

    REQUIRES
        esp_wifi
        esp_http_server
        nvs_flash
        esp_netif
        esp_event
        esp_timer
        esp-bsp
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    "-Wno-format"
    "-Wno-deprecated-declarations"
)

target_compile_definitions(${COMPONENT_TARGET} PRIVATE
    "-D RMAKER_DEMO_PROJECT_NAME=\"${CMAKE_PROJECT_NAME}\""
)

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS -DLV_LVGL_H_INCLUDE_SIMPLE
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
