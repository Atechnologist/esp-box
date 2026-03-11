# Tell CMake this is a proper component
idf_component_register(
    SRCS "main.c"           # your main source file
    INCLUDE_DIRS "."        # header files location
)
