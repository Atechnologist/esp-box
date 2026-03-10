# SPDX-License-Identifier: Apache-2.0

# List all source files for this component
set(srcs
    bsp_board.c
    bsp_led.c
    bsp_button.c
)

# List include directories for this component
set(includes
    include
)

# Register component with IDF
# Add REQUIRES for all dependencies that provide headers this component uses
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${includes}
    REQUIRES 
        espressif__esp-box 
        espressif__esp-box-3 
        espressif__esp-box-lite
        lvgl         # if you use LVGL GUI
        wifi_provisioning  # if using Wi-Fi provisioning
)

# Optional: suppress warnings as errors if needed
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
