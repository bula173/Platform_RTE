add_library(safeapi_appmanager STATIC sapi_appmanager.c)

target_include_directories(safeapi_appmanager
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Link against the 3 consolidated safeAPI libraries (ADR-023) appmanager
# depends on: safeapi_core (status), safeapi_oal (log), safeapi_channels
# (checkpoint - required by the optional ADR-019 checkpoint integration,
# sapi_channel_checkpoint() is called from sapi_appmanager_run() whenever
# config->checkpoint is non-NULL).
target_link_libraries(safeapi_appmanager PUBLIC safeapi_core safeapi_oal safeapi_channels)

safeapi_set_warnings(safeapi_appmanager)

add_library(safeapi::appmanager ALIAS safeapi_appmanager)
