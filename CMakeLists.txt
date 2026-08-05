cmake_minimum_required(VERSION 3.16)
project(safeAPIFramework
    VERSION 0.1.0
    DESCRIPTION "Safe API Framework - layered API for safety-related applications (ERTMS RBC reference)"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global build settings
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 99)               # C99: matches typical qualified SIL3/4 toolchains
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(SAFEAPI_BUILD_TESTS "Build unit tests" ON)
option(SAFEAPI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# Each feature (ADR-007) is its own subdirectory with its own small static
# library target (safeapi::<feature>), built and linkable independently.
# Order matters only where a feature links another (string -> buffer, cast;
# checkpoint -> checksum/watchdog/vital_channel, ADR-017). checksum was
# never previously wired in here despite having its own CMakeLists.txt -
# added now because sapi_checkpoint (ADR-017) depends on it.
#
# NOTE: "channel" (ADR-008, a standalone 2-channel comparator written
# before sapi_vital_channel's fuller implementation was known about) is
# intentionally NOT included here. It currently references a safestate
# reason code that was reverted along with other clobbered files during
# reconciliation (see ADR-017 section 1's "Deferred" note) and will not
# build as-is; whether to fix and re-adopt it or retire it in favor of
# vital_channel + checkpoint is an open decision, not made by this file.
foreach(feature status types buffer cast safestate string
                timer nvm memory task ipc log reboot watchdog checksum
                vital_channel clocksync checkpoint posix_backend)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${feature}/CMakeLists.txt")
        add_subdirectory(src/${feature})
    endif()
endforeach()

# App manager depends on status and log, so add it last
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/appmanager/CMakeLists.txt")
    add_subdirectory(src/appmanager)
endif()

if(SAFEAPI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Static analysis: cppcheck with MISRA C:2012 addon
# ---------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE}
            --project=${CMAKE_BINARY_DIR}/compile_commands.json
            --addon=misra
            --std=c99
            --enable=all
            --inconclusive
            --suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck-suppressions
            -I ${CMAKE_CURRENT_SOURCE_DIR}/include
            --output-file=${CMAKE_BINARY_DIR}/cppcheck-report.txt
            --xml --xml-version=2 > ${CMAKE_BINARY_DIR}/cppcheck-report.xml 2>&1 || true
        COMMENT "Running cppcheck with MISRA C:2012 analysis..."
        VERBATIM
    )
    message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE}")
    message(STATUS "  Run 'cmake --build build --target cppcheck' to analyze code")
else()
    message(STATUS "cppcheck not found - MISRA static analysis disabled")
endif()

# ---------------------------------------------------------------------------
# Package and export configuration (for downstream projects)
# ---------------------------------------------------------------------------
# Define install paths
set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR lib)
set(CMAKE_INSTALL_DIR lib/cmake/safeAPIFramework)

# Install public headers
install(DIRECTORY include/safeapi
    DESTINATION ${INCLUDE_INSTALL_DIR}
    COMPONENT development
)

# Install CMake modules (for reuse by downstream projects)
install(FILES
    cmake/CompilerWarnings.cmake
    cmake/SafeAPIHelpers.cmake
    DESTINATION ${CMAKE_INSTALL_DIR}
    COMPONENT development
)

# Generate and install config files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/safeAPIFrameworkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/safeAPIFrameworkConfig.cmake
    @ONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/safeAPIFrameworkConfigVersion.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/safeAPIFrameworkConfigVersion.cmake
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/safeAPIFrameworkConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/safeAPIFrameworkConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_DIR}
    COMPONENT development
)

# Install library targets (types is header-only)
install(TARGETS
    safeapi_status safeapi_buffer safeapi_cast
    safeapi_safestate safeapi_string safeapi_timer safeapi_nvm
    safeapi_memory safeapi_task safeapi_ipc safeapi_log safeapi_reboot
    safeapi_watchdog safeapi_appmanager
    safeapi_checksum safeapi_vital_channel safeapi_clocksync safeapi_checkpoint
    $<$<TARGET_EXISTS:safeapi_posix_backend>:safeapi_posix_backend>
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    RUNTIME DESTINATION bin
    COMPONENT libraries
)

message(STATUS "safeAPIFramework package configuration ready")
message(STATUS "  Install with: cmake --install build")
message(STATUS "  Use in downstream projects: find_package(safeAPIFramework)")
