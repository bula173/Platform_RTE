add_library(safeapi_appmanager STATIC sapi_appmanager.c)

target_include_directories(safeapi_appmanager
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Link against core safeAPI modules that appmanager depends on
target_link_libraries(safeapi_appmanager PUBLIC safeapi_status safeapi_log)

safeapi_set_warnings(safeapi_appmanager)

add_library(safeapi::appmanager ALIAS safeapi_appmanager)
