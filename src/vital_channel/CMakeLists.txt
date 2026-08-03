add_library(safeapi_vital_channel STATIC
    sapi_vital_channel.c
)

target_link_libraries(safeapi_vital_channel
    PUBLIC safeapi::status safeapi::log safeapi::safestate
)

target_include_directories(safeapi_vital_channel
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_vital_channel)

add_library(safeapi::vital_channel ALIAS safeapi_vital_channel)
