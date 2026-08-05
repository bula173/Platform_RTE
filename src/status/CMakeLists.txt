add_library(safeapi_status STATIC
    sapi_status.c
)

target_include_directories(safeapi_status
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_status)

add_library(safeapi::status ALIAS safeapi_status)
