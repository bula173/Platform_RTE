add_library(safeapi_safestate STATIC
    sapi_safestate.c
)

target_include_directories(safeapi_safestate
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_safestate)

add_library(safeapi::safestate ALIAS safeapi_safestate)
