# examples/factory_demo/main/CMakeLists.txt

# Minimum CMake version
cmake_minimum_required(VERSION 3.5)

# Include the ESP-IDF project macro
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Add common compiler flags
add_compile_options(
    -fdiagnostics-color=always
    -Wno-ignored-qualifiers
    -Wno-deprecated-declarations
    -Wno-unused-but-set-variable
)

# Register the project
project(factory_demo)

# If you want to add local components manually (optional)
# set(EXTRA_COMPONENT_DIRS ../../components)

# idf_component.yml will handle all dependencies automatically
