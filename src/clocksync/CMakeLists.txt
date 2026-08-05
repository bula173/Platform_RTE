# Clock Synchronization Module (ADR-017)
# Pluggable, diagnostic/timeout-sizing-only clock offset backend.

add_library(safeapi_clocksync STATIC
    sapi_clocksync.c
)

target_include_directories(safeapi_clocksync
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(safeapi_clocksync
    PUBLIC
        safeapi::status
        safeapi::types
)

safeapi_set_warnings(safeapi_clocksync)

add_library(safeapi::clocksync ALIAS safeapi_clocksync)
