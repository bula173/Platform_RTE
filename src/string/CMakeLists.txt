add_library(safeapi_string STATIC
    sapi_string.c
)

target_include_directories(safeapi_string
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(safeapi_string
    PUBLIC
        safeapi::buffer
        safeapi::cast
)

safeapi_set_warnings(safeapi_string)

add_library(safeapi::string ALIAS safeapi_string)
