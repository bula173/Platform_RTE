# Unified channel factory (ADR-022): hides sapi_netlink/sapi_ipc from
# application code behind sapi_safechannel_open/send/receive/close.

add_library(safeapi_safechannel STATIC
    sapi_safechannel.c
)

target_include_directories(safeapi_safechannel
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(safeapi_safechannel
    PUBLIC
        safeapi::status
        safeapi::types
        safeapi::netlink
        safeapi::checksum
        safeapi::dual
        safeapi::vital_channel
)

safeapi_set_warnings(safeapi_safechannel)

add_library(safeapi::safechannel ALIAS safeapi_safechannel)
