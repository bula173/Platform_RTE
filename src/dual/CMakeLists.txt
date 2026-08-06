# Dual-Transfer State Negotiation Module (ADR-020)
# sapi_dual_state_t negotiation (own + peer) driven over a redundant,
# EN 50159-defended messaging channel ("Channel"/"DualChannel").

add_library(safeapi_dual STATIC
    sapi_dual_types.c
    sapi_dual_msgchannel.c
    sapi_dual_channel.c
    sapi_dual_negotiator.c
)

target_include_directories(safeapi_dual
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(safeapi_dual
    PUBLIC
        safeapi::status
        safeapi::types
        safeapi::netlink
        safeapi::checksum
        safeapi::timer
)

safeapi_set_warnings(safeapi_dual)

add_library(safeapi::dual ALIAS safeapi_dual)
