add_library(safeapi_watchdog STATIC sapi_watchdog.c)

target_include_directories(safeapi_watchdog
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(safeapi_watchdog PUBLIC safeapi_status safeapi_log)

safeapi_set_warnings(safeapi_watchdog)

add_library(safeapi::watchdog ALIAS safeapi_watchdog)
