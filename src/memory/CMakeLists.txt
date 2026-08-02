add_library(safeapi_memory STATIC
    sapi_memory.c
)

target_include_directories(safeapi_memory
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_memory)

add_library(safeapi::memory ALIAS safeapi_memory)
