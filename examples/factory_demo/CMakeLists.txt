# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    ../../components
    )

add_compile_options(-fdiagnostics-color=always
                    -Wno-ignored-qualifiers
                    -Wno-deprecated-declarations
                    -Wno-unused-but-set-variable)
                    
idf_component_register(
    SRCS "main.c"
         "wifi_connect.c"
         "web_server.c"
         "device_control.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_http_server nvs_flash
)
project(factory_demo)
