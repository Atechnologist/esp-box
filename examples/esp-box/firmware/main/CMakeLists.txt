idf_component_register(
    SRCS "main.c" "voice.c"
    INCLUDE_DIRS "."
)

# 🔥 SAFE EMBED (only if file exists)
set(MODEL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/model/model.bin")

if(EXISTS ${MODEL_FILE})
    message(STATUS "Embedding model.bin")
    target_add_binary_data(${COMPONENT_TARGET} "model/model.bin" BINARY)
else()
    message(WARNING "model.bin not found at configure time")
endif()
