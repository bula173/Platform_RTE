add_library(safeapi_nvm STATIC
    sapi_nvm.c
)

target_include_directories(safeapi_nvm
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_nvm)

add_library(safeapi::nvm ALIAS safeapi_nvm)
