cmake_minimum_required(VERSION 3.16)

# 🔥 FORCE CLEAN BUILD (ignore factory/demo components)
set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_box_s3_hub)
