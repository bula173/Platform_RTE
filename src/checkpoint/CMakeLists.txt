# Checkpoint Rendezvous Module (ADR-017)
# Bounded cross-channel synchronization built on vital_channel + checksum.

add_library(safeapi_checkpoint STATIC
    sapi_checkpoint.c
)

target_include_directories(safeapi_checkpoint
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(safeapi_checkpoint
    PUBLIC
        safeapi::status
        safeapi::types
        safeapi::buffer
        safeapi::timer
        safeapi::safestate
        safeapi::checksum
        safeapi::watchdog
        safeapi::vital_channel
)

safeapi_set_warnings(safeapi_checkpoint)

add_library(safeapi::checkpoint ALIAS safeapi_checkpoint)
