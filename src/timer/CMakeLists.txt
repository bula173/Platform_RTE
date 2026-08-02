add_library(safeapi_timer STATIC
    sapi_timer.c
)

target_include_directories(safeapi_timer
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_timer)

add_library(safeapi::timer ALIAS safeapi_timer)
