add_library(safeapi_log STATIC
    sapi_log.c
)

target_include_directories(safeapi_log
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_log)

add_library(safeapi::log ALIAS safeapi_log)
