add_library(safeapi_reboot STATIC
    sapi_reboot.c
)

target_include_directories(safeapi_reboot
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_reboot)

add_library(safeapi::reboot ALIAS safeapi_reboot)
