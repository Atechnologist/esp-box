cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/component.cmake)

idf_component_register(
    SRCS "main.c"      # <-- change this to match your file exactly
    INCLUDE_DIRS "."
)
