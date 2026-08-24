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
# safeAPIRBC2oo2), targets defined in the downstream project's own
# CMakeLists.txt (a directory-tree sibling/ancestor of this one, not a
# descendant) would silently NOT get compile_commands.json entries -
# exactly what was observed (compile_commands.json had all 19 framework
# targets but none of safeAPIRBC2oo2's own). CACHE variables are visible
# project-tree-wide regardless of which directory sets them first.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json for MISRA/cppcheck tooling" FORCE)

option(SAFEAPI_BUILD_TESTS "Build unit tests" ON)
option(SAFEAPI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(SAFEAPI_ENABLE_COVERAGE "Build with gcov/gcovr coverage instrumentation (see scripts/coverage.sh)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(SafeAPIHelpers)

if(SAFEAPI_ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "SAFEAPI_ENABLE_COVERAGE requires GCC or Clang (found ${CMAKE_C_COMPILER_ID})")
    endif()
    # -O0 keeps line/branch counts meaningful (no inlining/dead-branch-elision
    # by the optimizer skewing what gcov reports); applies build-tree-wide so
    # both the libraries and the test binaries that exercise them are
    # instrumented identically - see scripts/coverage.sh.
    add_compile_options(--coverage -O0)
    add_link_options(--coverage)
endif()

# ---------------------------------------------------------------------------
# Feature selection (ADR-024): choose which modules get compiled in
# ---------------------------------------------------------------------------
# CORE (status/types/buffer/cast/safestate/string, below) is always
# compiled - it has no OS/backend dependencies of its own and every other
# module depends on it, directly or transitively. Everything below is
# opt-out, default ON, matching this project's historical
# always-build-everything behavior so existing consumers (e.g.
# safeAPIRBC2oo2) need no CMake changes to keep building. Turning a module
# OFF removes it from its static library's source list entirely - it is
# never compiled, not merely dead-stripped at link time - which matters for
# SIL verification scope: a module you never compile is a module this build
# configuration never has to verify. See ADR-024 for the full dependency
# table and rationale (edges below were derived from actual #include usage,
# not guessed).

# --- OAL layer (each depends only on CORE unless noted) ---
option(SAFEAPI_ENABLE_TIMER    "Enable sapi_timer (periodic/oneshot timer service)" ON)
option(SAFEAPI_ENABLE_MUTEX    "Enable sapi_mutex (non-recursive blocking mutual exclusion, ADR-033)" ON)
option(SAFEAPI_ENABLE_NVM      "Enable sapi_nvm (non-volatile storage service)" ON)
option(SAFEAPI_ENABLE_MEMORY   "Enable sapi_memory (static memory region service)" ON)
option(SAFEAPI_ENABLE_TASK     "Enable sapi_task (task/thread scheduling service)" ON)
option(SAFEAPI_ENABLE_NETLINK  "Enable sapi_netlink (network transport service)" ON)
option(SAFEAPI_ENABLE_IPC      "Enable sapi_ipc (inter-process queue service)" ON)
option(SAFEAPI_ENABLE_REBOOT   "Enable sapi_reboot (controlled reboot service)" ON)
option(SAFEAPI_ENABLE_LOG      "Enable sapi_log (structured logging) - needs TIMER" ON)
option(SAFEAPI_ENABLE_WATCHDOG "Enable sapi_watchdog (liveness watchdog) - needs LOG, TIMER" ON)

# --- Channels layer ---
option(SAFEAPI_ENABLE_CLOCKSYNC     "Enable sapi_clocksync (diagnostic wall-clock offset query)" ON)
option(SAFEAPI_ENABLE_CHECKSUM      "Enable sapi_checksum (CRC-64 data integrity) - needs TIMER" ON)
option(SAFEAPI_ENABLE_CHANNEL_LINK "Enable sapi_channel (single redundant link) - needs LOG" ON)
option(SAFEAPI_ENABLE_VOTER         "Enable sapi_voter (N-way 2oo2/2oo3/NMR voting over channel_link links, ADR-025) - needs CHANNEL_LINK" ON)
option(SAFEAPI_ENABLE_CROSS_COMPARATOR "Enable sapi_cross_comparator (2-way channel comparison, ADR-025) - needs CHANNEL_LINK, VOTER" ON)
option(SAFEAPI_ENABLE_CHECKPOINT    "Enable sapi_checkpoint (cross-channel rendezvous) - needs CHECKSUM, TIMER, VOTER, WATCHDOG" ON)
option(SAFEAPI_ENABLE_DUAL          "Enable sapi_dual (dual-transfer state negotiation) - needs CHECKSUM, NETLINK, TIMER" ON)
option(SAFEAPI_ENABLE_SAFECHANNEL   "Enable sapi_safechannel (unified channel factory) - needs CHECKSUM, DUAL, NETLINK, VOTER" ON)

# --- App layer ---
option(SAFEAPI_ENABLE_APPMANAGER "Enable sapi_appmanager (cyclic-executive lifecycle) - needs VOTER, WATCHDOG, CHECKPOINT" ON)

safeapi_require_feature(SAFEAPI_ENABLE_LOG          DEPENDS SAFEAPI_ENABLE_TIMER)
safeapi_require_feature(SAFEAPI_ENABLE_WATCHDOG     DEPENDS SAFEAPI_ENABLE_LOG SAFEAPI_ENABLE_TIMER)
safeapi_require_feature(SAFEAPI_ENABLE_CHECKSUM     DEPENDS SAFEAPI_ENABLE_TIMER)
safeapi_require_feature(SAFEAPI_ENABLE_CHANNEL_LINK DEPENDS SAFEAPI_ENABLE_LOG)
safeapi_require_feature(SAFEAPI_ENABLE_VOTER        DEPENDS SAFEAPI_ENABLE_CHANNEL_LINK SAFEAPI_ENABLE_LOG)
safeapi_require_feature(SAFEAPI_ENABLE_CROSS_COMPARATOR DEPENDS SAFEAPI_ENABLE_CHANNEL_LINK SAFEAPI_ENABLE_VOTER SAFEAPI_ENABLE_LOG)
safeapi_require_feature(SAFEAPI_ENABLE_CHECKPOINT   DEPENDS SAFEAPI_ENABLE_CHECKSUM SAFEAPI_ENABLE_TIMER SAFEAPI_ENABLE_VOTER SAFEAPI_ENABLE_WATCHDOG)
safeapi_require_feature(SAFEAPI_ENABLE_DUAL         DEPENDS SAFEAPI_ENABLE_CHECKSUM SAFEAPI_ENABLE_NETLINK SAFEAPI_ENABLE_TIMER)
safeapi_require_feature(SAFEAPI_ENABLE_SAFECHANNEL  DEPENDS SAFEAPI_ENABLE_CHECKSUM SAFEAPI_ENABLE_DUAL SAFEAPI_ENABLE_NETLINK SAFEAPI_ENABLE_VOTER)
safeapi_require_feature(SAFEAPI_ENABLE_APPMANAGER   DEPENDS SAFEAPI_ENABLE_VOTER SAFEAPI_ENABLE_WATCHDOG SAFEAPI_ENABLE_CHECKPOINT)

message(STATUS "safeAPIFramework feature selection:")
foreach(_sapi_feat TIMER NVM MEMORY TASK NETLINK IPC REBOOT LOG WATCHDOG
                    CLOCKSYNC CHECKSUM CHANNEL_LINK VOTER CROSS_COMPARATOR
                    CHECKPOINT DUAL SAFECHANNEL APPMANAGER)
    message(STATUS "  SAFEAPI_ENABLE_${_sapi_feat}: ${SAFEAPI_ENABLE_${_sapi_feat}}")
endforeach()

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
# The old ADR-008 standalone "channel" 2-channel comparator (excluded
# from every build since before this file existed, never compiled)
# has been removed outright as of ADR-025: sapi_cross_comparator now
# covers that exact use case (register-2-channels, pairwise compare)
# as a real, tested, buildable module - see ADR-025 for the retirement
# rationale.

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
    src/lifecycle/sapi_lifecycle.c
)
target_include_directories(safeapi_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
safeapi_set_warnings(safeapi_core)
set_target_properties(safeapi_core PROPERTIES EXPORT_NAME core)
add_library(safeapi::core ALIAS safeapi_core)

# --- safeapi_oal: OS Abstraction Layer, ADR-001/ADR-005 backend-dispatch
# services. Source list is gated per-module by the SAFEAPI_ENABLE_* options
# above (ADR-024). Deliberately never includes src/ipc/sapi_ipc_pubsub.c or
# sapi_ipc_request_reply.c regardless of SAFEAPI_ENABLE_IPC - those
# pre-existing stub files are TODO-only (see their own headers) and were
# never wired into any target; only the base sapi_ipc.c queue API is real. ---
set(_safeapi_oal_sources "")
if(SAFEAPI_ENABLE_TIMER)
    list(APPEND _safeapi_oal_sources src/timer/sapi_timer.c)
endif()
if(SAFEAPI_ENABLE_MUTEX)
    list(APPEND _safeapi_oal_sources src/mutex/sapi_mutex.c)
endif()
if(SAFEAPI_ENABLE_NVM)
    list(APPEND _safeapi_oal_sources src/nvm/sapi_nvm.c)
endif()
if(SAFEAPI_ENABLE_MEMORY)
    list(APPEND _safeapi_oal_sources src/memory/sapi_memory.c)
endif()
if(SAFEAPI_ENABLE_TASK)
    list(APPEND _safeapi_oal_sources src/task/sapi_task.c)
endif()
if(SAFEAPI_ENABLE_IPC)
    list(APPEND _safeapi_oal_sources src/ipc/sapi_ipc.c)
endif()
if(SAFEAPI_ENABLE_NETLINK)
    list(APPEND _safeapi_oal_sources src/netlink/sapi_netlink.c)
endif()
if(SAFEAPI_ENABLE_LOG)
    list(APPEND _safeapi_oal_sources src/log/sapi_log.c)
endif()
if(SAFEAPI_ENABLE_REBOOT)
    list(APPEND _safeapi_oal_sources src/reboot/sapi_reboot.c)
endif()
if(SAFEAPI_ENABLE_WATCHDOG)
    list(APPEND _safeapi_oal_sources src/watchdog/sapi_watchdog.c)
endif()
if(NOT _safeapi_oal_sources)
    message(FATAL_ERROR "safeAPIFramework: every SAFEAPI_ENABLE_<oal-feature> option "
                         "is OFF - safeapi_oal would have no sources. Enable at least one.")
endif()

add_library(safeapi_oal STATIC ${_safeapi_oal_sources})
target_include_directories(safeapi_oal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(safeapi_oal PUBLIC safeapi_core)
safeapi_set_warnings(safeapi_oal)
set_target_properties(safeapi_oal PROPERTIES EXPORT_NAME oal)
add_library(safeapi::oal ALIAS safeapi_oal)

# --- safeapi_channels: safety-comms / channel layer (ADR-008 lineage,
# ADR-025 sapi_channel/voter/cross_comparator split, ADR-017
# checkpoint/clocksync, ADR-020 dual, ADR-022 safechannel). Source list
# gated per-module by the SAFEAPI_ENABLE_* options above (ADR-024).
# Reaches into both core and oal, so links both. ---
set(_safeapi_channels_sources "")
if(SAFEAPI_ENABLE_CHECKSUM)
    list(APPEND _safeapi_channels_sources src/checksum/sapi_checksum.c)
endif()
if(SAFEAPI_ENABLE_CHANNEL_LINK)
    list(APPEND _safeapi_channels_sources
        src/channel_link/sapi_channel.c
        src/channel_service/sapi_channel_service.c
    )
endif()
if(SAFEAPI_ENABLE_VOTER)
    list(APPEND _safeapi_channels_sources src/voter/sapi_voter.c)
endif()
if(SAFEAPI_ENABLE_CROSS_COMPARATOR)
    list(APPEND _safeapi_channels_sources src/cross_comparator/sapi_cross_comparator.c)
endif()
if(SAFEAPI_ENABLE_CLOCKSYNC)
    list(APPEND _safeapi_channels_sources src/clocksync/sapi_clocksync.c)
endif()
if(SAFEAPI_ENABLE_CHECKPOINT)
    list(APPEND _safeapi_channels_sources src/checkpoint/sapi_checkpoint.c)
endif()
if(SAFEAPI_ENABLE_DUAL)
    list(APPEND _safeapi_channels_sources
        src/dual/sapi_dual_types.c
        src/dual/sapi_dual_msgchannel.c
        src/dual/sapi_dual_channel.c
        src/dual/sapi_dual_negotiator.c
    )
endif()
if(SAFEAPI_ENABLE_SAFECHANNEL)
    list(APPEND _safeapi_channels_sources src/safechannel/sapi_safechannel.c)
endif()
if(NOT _safeapi_channels_sources)
    message(FATAL_ERROR "safeAPIFramework: every SAFEAPI_ENABLE_<channels-feature> option "
                         "is OFF - safeapi_channels would have no sources. Enable at least one.")
endif()

add_library(safeapi_channels STATIC ${_safeapi_channels_sources})
target_include_directories(safeapi_channels PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(safeapi_channels PUBLIC safeapi_core safeapi_oal)
safeapi_set_warnings(safeapi_channels)
set_target_properties(safeapi_channels PROPERTIES EXPORT_NAME channels)
add_library(safeapi::channels ALIAS safeapi_channels)

# App manager depends on status (core), log (oal), and (ADR-019)
# checkpoint (channels), so add it last - its own src/appmanager/CMakeLists.txt
# is unchanged in shape, only its target_link_libraries() line was
# updated to the 3 new names (ADR-023).
if(SAFEAPI_ENABLE_APPMANAGER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/appmanager/CMakeLists.txt")
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
# in via add_subdirectory() (e.g. safeAPIRBC2oo2) shares this same build
# tree's CMAKE_INSTALL_PREFIX; without this guard, `cmake --install` from
# the downstream project would also install a full copy of every SAPI
# header and static library into the downstream project's own install
# tree (e.g. safeAPIRBC2oo2/include/safeapi/..., safeAPIRBC2oo2/lib/*.a)
# alongside - and easily confusable with - that project's own sources.
# CMAKE_PROJECT_NAME is the top-level project's name for the whole build;
# PROJECT_NAME is this CMakeLists.txt's own project() name - they're only
# equal when this file is the one that was configured directly.
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Define install paths. Everything lands under dist/ (install with
    # `cmake --install build --prefix .` from the repo root) so that
    # "what this project hands to another SAPI project" is always
    # <project>/dist/ - the same convention safeAPIRBC2oo2's own
    # install(TARGETS ... RUNTIME DESTINATION dist/bin) already uses.
    # A downstream consumer points CMAKE_PREFIX_PATH/find_package at
    # this dist/ tree (see cmake/safeAPIFrameworkConfig.cmake.in).
    set(INCLUDE_INSTALL_DIR dist/include)
    set(LIB_INSTALL_DIR dist/lib)
    set(CMAKE_INSTALL_DIR dist/lib/cmake/safeAPIFramework)

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
    # installed archive even before this change). appmanager is only in the
    # target list when SAFEAPI_ENABLE_APPMANAGER actually created it
    # (ADR-024). EXPORT ties this into the safeAPIFrameworkTargets.cmake
    # generated below, so find_package(safeAPIFramework) consumers get real
    # imported targets (safeapi::core etc.), not just raw path variables -
    # previously this file only exposed safeAPIFramework_INCLUDE_DIR/_LIB_DIR
    # and a stale, pre-ADR-023 library name list.
    set(_safeapi_install_targets safeapi_core safeapi_oal safeapi_channels)
    if(TARGET safeapi_appmanager)
        list(APPEND _safeapi_install_targets safeapi_appmanager)
    endif()
    install(TARGETS ${_safeapi_install_targets}
        EXPORT safeAPIFrameworkTargets
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        RUNTIME DESTINATION dist/bin
        COMPONENT libraries
    )
    install(EXPORT safeAPIFrameworkTargets
        NAMESPACE safeapi::
        FILE safeAPIFrameworkTargets.cmake
        DESTINATION ${CMAKE_INSTALL_DIR}
        COMPONENT development
    )

    message(STATUS "safeAPIFramework package configuration ready")
    message(STATUS "  Install with: cmake --install build")
    message(STATUS "  Use in downstream projects: find_package(safeAPIFramework)")
else()
    message(STATUS "safeAPIFramework: consumed via add_subdirectory() by '${CMAKE_PROJECT_NAME}' - "
                    "skipping this project's own install()/package export rules "
                    "(the consuming project's own install() controls what gets installed)")
endif()
