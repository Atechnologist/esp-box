cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF component functions
include($ENV{IDF_PATH}/tools/cmake/component.cmake)

# Register this component
idf_component_register(
    SRCS "main.c"      # Match your actual filename exactly
    INCLUDE_DIRS "."   # Include current directory for headers
)
