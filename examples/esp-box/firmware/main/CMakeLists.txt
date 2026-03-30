idf_component_register(
    SRCS "main.c" "voice.c"
    INCLUDE_DIRS "."
    EMBED_FILES "model/model.bin"
)
