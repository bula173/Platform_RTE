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

# Required for the cppcheck/MISRA target below (--project=compile_commands.json):
# without this, compile_commands.json is never generated, that target's
# cppcheck invocation fails at the "open project file" step, and the
# MISRA check silently never actually ran despite `cppcheck found` and
# `Built target cppcheck` both printing successfully - discovered while
# verifying this project's own compliance-report process actually works.
# Must be a CACHE variable, not a plain set(): a plain set() here is
# scoped to this directory and its descendants only, so when this
# framework is add_subdirectory()'d by a downstream project (e.g.
# safeAPIExample), targets defined in the downstream project's own
# CMakeLists.txt (a directory-tree sibling/ancestor of this one, not a
# descendant) would silently NOT get compile_commands.json entries -
# exactly what was observed (compile_commands.json had all 19 framework
# targets but none of safeAPIExample's own). CACHE variables are visible
# project-tree-wide regardless of which directory sets them first.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json for MISRA/cppcheck tooling" FORCE)

option(SAFEAPI_BUILD_TESTS "Build unit tests" ON)
option(SAFEAPI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# ADR-023: the 21 per-feature CMake targets ADR-007 originally created
# (one static library per src/<feature>/ directory) are consolidated into
# 3 grouped libraries here, plus appmanager as a 4th (its own
# src/appmanager/CMakeLists.txt, added separately below since it depends
# on all three). This is a pure build-target regrouping - no file moved,
# no #include path changed, no public API changed; every feature's
# source still lives at exactly src/<feature>/sapi_<feature>.c per
# ADR-007 section 2.1, which is unaffected. See ADR-023 for the full
# rationale (why 22 targets collapsed to 4, why these specific groupings,
# and what was deliberately left out).
#
# NOTE: "channel" (ADR-008, a standalone 2-channel comparator written
# before sapi_vital_channel's fuller implementation was known about) is
# intentionally NOT included in any of the 3 libraries below, exactly as
# it was excluded from the old per-feature foreach() this replaces. It
# currently references a safestate reason code that was reverted along
# with other clobbered files during reconciliation (see ADR-017 section
# 1's "Deferred" note) and will not build as-is; whether to fix and
# re-adopt it or retire it in favor of vital_channel + checkpoint is an
# open decision, not made by this file.

# --- safeapi_core: zero-OS-dependency primitives (ADR-002 section 4) ---
# types (sapi_types.h) is header-only - no .c file - and needs no
# separate target: its header is already reachable through this
# library's own PUBLIC include directory below, same as every other
# feature's header.
add_library(safeapi_core STATIC
    src/status/sapi_status.c
    src/buffer/sapi_buffer.c
    src/cast/sapi_cast.c
    src/safestate/sapi_safestate.c
    src/string/sapi_string.c
)
target_include_directories(safeapi_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
safeapi_set_warnings(safeapi_core)
add_library(safeapi::core ALIAS safeapi_core)

# --- safeapi_oal: OS Abstraction Layer, ADR-001/ADR-005 backend-dispatch
# services. log needs string (line formatting) + timer (its TIMESTAMP
# field, ADR-019-era sapi_log_write_event()); watchdog needs
# status/safestate (core) + log/timer (same library). Deliberately does
# NOT include src/ipc/sapi_ipc_pubsub.c or sapi_ipc_request_reply.c -
# those pre-existing stub files (including a nonexistent safeapi/log.h
# path) were never wired into the old safeapi_ipc target either; this
# ADR preserves that, doesn't fix it. ---
add_library(safeapi_oal STATIC
    src/timer/sapi_timer.c
    src/nvm/sapi_nvm.c
    src/memory/sapi_memory.c
    src/task/sapi_task.c
    src/ipc/sapi_ipc.c
    src/netlink/sapi_netlink.c
    src/log/sapi_log.c
    src/reboot/sapi_reboot.c
    src/watchdog/sapi_watchdog.c
)
target_include_directories(safeapi_oal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(safeapi_oal PUBLIC safeapi_core)
safeapi_set_warnings(safeapi_oal)
add_library(safeapi::oal ALIAS safeapi_oal)

# --- safeapi_channels: safety-comms / channel layer (ADR-008 vital_channel
# lineage, ADR-017 checkpoint/clocksync, ADR-020 dual, ADR-022
# safechannel). Reaches into both core (status/types/safestate/buffer)
# and oal (log/timer/netlink), so links both. ---
add_library(safeapi_channels STATIC
    src/checksum/sapi_checksum.c
    src/vital_channel/sapi_vital_channel.c
    src/clocksync/sapi_clocksync.c
    src/checkpoint/sapi_checkpoint.c
    src/dual/sapi_dual_types.c
    src/dual/sapi_dual_msgchannel.c
    src/dual/sapi_dual_channel.c
    src/dual/sapi_dual_negotiator.c
    src/safechannel/sapi_safechannel.c
)
target_include_directories(safeapi_channels PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(safeapi_channels PUBLIC safeapi_core safeapi_oal)
safeapi_set_warnings(safeapi_channels)
add_library(safeapi::channels ALIAS safeapi_channels)

# App manager depends on status (core), log (oal), and (ADR-019)
# checkpoint (channels), so add it last - its own src/appmanager/CMakeLists.txt
# is unchanged in shape, only its target_link_libraries() line was
# updated to the 3 new names (ADR-023).
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
# Only meaningful - and only run - when safeAPIFramework is the top-level
# project being configured directly. A downstream project that pulls this
# in via add_subdirectory() (e.g. safeAPIExample) shares this same build
# tree's CMAKE_INSTALL_PREFIX; without this guard, `cmake --install` from
# the downstream project would also install a full copy of every SAPI
# header and static library into the downstream project's own install
# tree (e.g. safeAPIExample/include/safeapi/..., safeAPIExample/lib/*.a)
# alongside - and easily confusable with - that project's own sources.
# CMAKE_PROJECT_NAME is the top-level project's name for the whole build;
# PROJECT_NAME is this CMakeLists.txt's own project() name - they're only
# equal when this file is the one that was configured directly.
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Define install paths
    set(INCLUDE_INSTALL_DIR include)
    set(LIB_INSTALL_DIR lib)
    set(CMAKE_INSTALL_DIR lib/cmake/safeAPIFramework)

    # Install public (consumer-facing) headers.
    install(DIRECTORY include/safeapi
        DESTINATION ${INCLUDE_INSTALL_DIR}
        COMPONENT development
    )

    # Install OS-backend adaptation headers (ADR-021) as their own component -
    # a real application never includes anything under safeapi_backend/; only
    # the platform integrator wiring a concrete backend does. Kept as a
    # separate top-level tree (not a subfolder of include/safeapi) so the
    # two audiences are visibly, physically separate, not just conventionally
    # so. Currently only the timer module has been split this way (pilot);
    # more modules move here incrementally.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/safeapi_backend")
        install(DIRECTORY include/safeapi_backend
            DESTINATION ${INCLUDE_INSTALL_DIR}
            COMPONENT backend-development
        )
    endif()

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

    # Install library targets (ADR-023: 4 consolidated libraries instead
    # of 20 per-feature ones; types is header-only and was never its own
    # installed archive even before this change).
    install(TARGETS
        safeapi_core safeapi_oal safeapi_channels safeapi_appmanager
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        RUNTIME DESTINATION bin
        COMPONENT libraries
    )

    message(STATUS "safeAPIFramework package configuration ready")
    message(STATUS "  Install with: cmake --install build")
    message(STATUS "  Use in downstream projects: find_package(safeAPIFramework)")
else()
    message(STATUS "safeAPIFramework: consumed via add_subdirectory() by '${CMAKE_PROJECT_NAME}' - "
                    "skipping this project's own install()/package export rules "
                    "(the consuming project's own install() controls what gets installed)")
endif()
