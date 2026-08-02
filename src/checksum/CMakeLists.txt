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

# Export for installation
install(
    FILES "${PROJECT_SOURCE_DIR}/include/safeapi/checksum/sapi_checksum.h"
    DESTINATION include/safeapi/checksum
)

install(
    TARGETS safeapi_checksum
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Add tests if testing is enabled
if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()
