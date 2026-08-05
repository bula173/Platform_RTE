add_library(safeapi_task STATIC
    sapi_task.c
)

target_include_directories(safeapi_task
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

safeapi_set_warnings(safeapi_task)

add_library(safeapi::task ALIAS safeapi_task)
