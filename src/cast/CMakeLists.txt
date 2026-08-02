add_library(safeapi_cast STATIC
    sapi_cast.c
)

target_include_directories(safeapi_cast
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_cast)

add_library(safeapi::cast ALIAS safeapi_cast)
