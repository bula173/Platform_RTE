add_library(safeapi_buffer STATIC
    sapi_buffer.c
)

target_include_directories(safeapi_buffer
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_buffer)

add_library(safeapi::buffer ALIAS safeapi_buffer)
