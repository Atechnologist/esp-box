cmake_minimum_required(VERSION 3.24)

# Include ESP-IDF project setup
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Optional: add extra components folder
set(EXTRA_COMPONENT_DIRS
    components
)

# Suppress compiler warnings
add_compile_options(
    -fdiagnostics-color=always
    -Wno-ignored-qualifiers
    -Wno-deprecated-declarations
    -Wno-unused-but-set-variable
)

project(factory_demo)
