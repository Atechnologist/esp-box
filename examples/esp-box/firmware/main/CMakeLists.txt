idf_component_register(
    SRCS "main.c" "voice.c"
    INCLUDE_DIRS "."
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/model/model.bin")
    target_add_binary_data(${COMPONENT_LIB} "model/model.bin" BINARY)
else()
    message(WARNING "model.bin not found — skipping embedding")
endif()
