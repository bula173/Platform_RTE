add_library(safeapi_channel STATIC
    sapi_channel.c
)

target_include_directories(safeapi_channel
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(safeapi_channel
    PUBLIC
        safeapi::buffer
        safeapi::safestate
)

# ADR-008 / REQ-COMMON-CHANNEL-001: every translation unit that includes
# sapi_channel.h must see exactly one of SAPI_CHANNEL_BUILD_A/_B. PUBLIC so
# it also propagates to anything that links safeapi::channel (including
# the test executable), not just this library's own sources.
target_compile_definitions(safeapi_channel
    PUBLIC
        SAPI_CHANNEL_BUILD_${SAPI_CHANNEL_BUILD_ID}
)

safeapi_set_warnings(safeapi_channel)

add_library(safeapi::channel ALIAS safeapi_channel)
