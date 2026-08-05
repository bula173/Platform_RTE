add_library(safeapi_netlink STATIC
    sapi_netlink.c
)

target_include_directories(safeapi_netlink
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_netlink)

add_library(safeapi::netlink ALIAS safeapi_netlink)
