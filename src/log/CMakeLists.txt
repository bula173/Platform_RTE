add_library(safeapi_log STATIC
    sapi_log.c
)

target_include_directories(safeapi_log
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# sapi_log_write_event() (ADR-019-era addition, structured message-trail
# logging) needs safeapi::string (bounded line formatting) and
# safeapi::timer (its TIMESTAMP field) - sapi_log_write() itself and the
# backend dispatch have neither dependency.
target_link_libraries(safeapi_log
    PRIVATE
        safeapi::string
        safeapi::timer
)

safeapi_set_warnings(safeapi_log)

add_library(safeapi::log ALIAS safeapi_log)
