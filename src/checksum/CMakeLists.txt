# Checksum & CRC-64 Module
# Provides data integrity checking for redundant channels

add_library(safeapi_checksum STATIC
    sapi_checksum.c
)

target_include_directories(safeapi_checksum PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

# Link against other SAPI modules this depends on
target_link_libraries(safeapi_checksum PUBLIC
    safeapi_status
    safeapi_types
    safeapi_log
    safeapi_timer
    safeapi_safestate
)

# MISRA C:2012 compliance
target_compile_options(safeapi_checksum PRIVATE
    ${SAPI_COMPILE_FLAGS}
)

# Create alias for consistency
add_library(safeapi::checksum ALIAS safeapi_checksum)

# Installation is handled once, centrally, by the top-level CMakeLists.txt's
# "Package and export configuration" block (install(DIRECTORY include/safeapi
# ...) already covers this header; install(TARGETS ... safeapi_checksum ...)
# already covers this library) - guarded there to only run when
# safeAPIFramework is the top-level project being configured directly, not
# when pulled in via add_subdirectory() by a downstream project. Duplicate,
# unguarded install() rules here previously bypassed that guard and leaked
# this header/library into any downstream project's own install prefix.

# Add tests if testing is enabled
if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()
