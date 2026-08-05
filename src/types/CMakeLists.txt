# sapi_types.h (sapi_duration_ms_t, sapi_timestamp_ms_t, SAFEAPI_DECLARE_STORAGE,
# alignment macros, ...) is header-only - there is no sapi_types.c. This is
# still given a real INTERFACE library target (rather than relying on
# consumers picking up the header transitively through some other target)
# so that `target_link_libraries(... safeapi::types)` - which several
# feature CMakeLists.txt files already call, on the assumption this target
# exists - actually resolves. No compiled sources means safeapi_set_warnings()
# (which uses PRIVATE compile options, invalid on INTERFACE libraries) does
# not apply here.
add_library(safeapi_types INTERFACE)

target_include_directories(safeapi_types
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

add_library(safeapi::types ALIAS safeapi_types)
