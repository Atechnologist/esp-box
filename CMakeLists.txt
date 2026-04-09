cmake_minimum_required(VERSION 3.16)

# 🔥 FORCE ONLY MAIN COMPONENT (IGNORE BSP / FACTORY / EXAMPLES)
set(COMPONENT_DIRS "main")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_box_s3_hub)
