# Minimum CMake version required
cmake_minimum_required(VERSION 3.5)

# Include the main ESP-IDF project CMake
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name
project(factory_demo)

# Extra component directories (local components not in IDF Component Manager)
set(EXTRA_COMPONENT_DIRS
    ../../components
)

# Compiler options (optional, for warnings/colors)
add_compile_options(
    -fdiagnostics-color=always
    -Wno-ignored-qualifiers
    -Wno-deprecated-declarations
    -Wno-unused-but-set-variable
)

# Register the main component
# ESP-IDF 5.x will automatically read idf_component.yml in main/
idf_component_register(
    SRCS "main.c" "other_sources.c"   # Replace with your source files
    INCLUDE_DIRS "include"            # Replace with your include directory
)
