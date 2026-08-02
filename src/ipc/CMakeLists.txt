add_library(safeapi_ipc STATIC
    sapi_ipc.c
)

target_include_directories(safeapi_ipc
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_ipc)

add_library(safeapi::ipc ALIAS safeapi_ipc)
