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

# ---------------------------------------------------------------------------
# Version (physically compiled into libsafeapi_core.a - see
# sapi_core_get_version(), include/safeapi/core/sapi_core_version.h.in)
# ---------------------------------------------------------------------------
# scripts/compute_version.sh appends the git commit (+dirty flag) to
# PROJECT_VERSION, real SemVer 2.0 build-metadata syntax - falls back to
# "+unknown" outside a git checkout (e.g. a Docker build stage, where
# .git isn't in the build context - see .dockerignore). Computed once
# here, not re-run per target, so every consumer of this one configure
# sees the same version string.
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/compute_version.sh" "${PROJECT_VERSION}"
    OUTPUT_VARIABLE _safeapi_framework_version_computed
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# CACHE (not a plain variable) so a consumer that reaches this project via
# add_subdirectory() - safeAPIRBC2oo2's own default path - can still read
# it after the subdirectory call returns (plain variables set inside
# add_subdirectory() don't propagate to the parent scope; CACHE variables
# do). FORCE: always recompute fresh each configure, never stick with a
# stale value left over from an earlier configure's git state.
set(SAFEAPI_FRAMEWORK_VERSION "${_safeapi_framework_version_computed}" CACHE STRING
    "This build's version: <static-version>+<git-shortsha>[.dirty] - see scripts/compute_version.sh" FORCE)
message(STATUS "safeAPIFramework version: ${SAFEAPI_FRAMEWORK_VERSION}")

# Generated into the BUILD dir, not the source tree, so this configure's
# own version doesn't get committed/shared across the several parallel
# build-toolchain-*/ directories this workspace's own build scripts create
# (see build_toolchains.sh) - each gets its own generated copy.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/safeapi/core/sapi_core_version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated_include/safeapi/core/sapi_core_version.h"
    @ONLY
)

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
option(SAFEAPI_ENABLE_ASAN "Build with AddressSanitizer + LeakSanitizer instrumentation" OFF)
option(SAFEAPI_ENABLE_UBSAN "Build with UndefinedBehaviorSanitizer instrumentation" OFF)

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

if(SAFEAPI_ENABLE_ASAN)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "SAFEAPI_ENABLE_ASAN requires GCC or Clang (found ${CMAKE_C_COMPILER_ID})")
    endif()
    # -fno-omit-frame-pointer: ASan's leak/error reports need real stack
    # traces, not the shorter but frame-pointer-less traces the optimizer
    # would otherwise produce. Applies build-tree-wide, same rationale as
    # SAFEAPI_ENABLE_COVERAGE above - the instrumented libraries and the
    # test binaries linking them must agree on ABI/runtime.
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

if(SAFEAPI_ENABLE_UBSAN)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "SAFEAPI_ENABLE_UBSAN requires GCC or Clang (found ${CMAKE_C_COMPILER_ID})")
    endif()
    # -fno-sanitize-recover=undefined: a detected UB (e.g. signed overflow,
    # misaligned access) aborts immediately instead of printing a
    # diagnostic and continuing - matches this codebase's own
    # fail-fast/never-silently-continue posture (CLAUDE.md) rather than
    # UBSan's default "report and keep going" behavior, which would let a
    # real defect slide past a test that doesn't check its own output.
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined -g)
    add_link_options(-fsanitize=undefined)
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
# src/app/appmanager/CMakeLists.txt, added separately below since it depends
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
    src/utils/status/sapi_status.c
    src/utils/buffer/sapi_buffer.c
    src/utils/cast/sapi_cast.c
    src/utils/safestate/sapi_safestate.c
    src/utils/safestate/sapi_safety_violation.c
    src/utils/string/sapi_string.c
    src/utils/lifecycle/sapi_lifecycle.c
    src/core/sapi_core_version.c
)
target_include_directories(safeapi_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated_include>
    $<INSTALL_INTERFACE:include>
)
safeapi_set_warnings(safeapi_core)
set_target_properties(safeapi_core PROPERTIES EXPORT_NAME core)
add_library(safeapi::core ALIAS safeapi_core)

# --- safeapi_oal: OS Abstraction Layer, ADR-001/ADR-005 backend-dispatch
# services. Source list is gated per-module by the SAFEAPI_ENABLE_* options
# above (ADR-024). Deliberately never includes src/oal/ipc/sapi_ipc_pubsub.c or
# sapi_ipc_request_reply.c regardless of SAFEAPI_ENABLE_IPC - those
# pre-existing stub files are TODO-only (see their own headers) and were
# never wired into any target; only the base sapi_ipc.c queue API is real. ---
set(_safeapi_oal_sources "")
if(SAFEAPI_ENABLE_TIMER)
    list(APPEND _safeapi_oal_sources src/oal/timer/sapi_timer.c)
endif()
if(SAFEAPI_ENABLE_MUTEX)
    list(APPEND _safeapi_oal_sources src/oal/mutex/sapi_mutex.c)
endif()
if(SAFEAPI_ENABLE_NVM)
    list(APPEND _safeapi_oal_sources src/oal/nvm/sapi_nvm.c)
endif()
if(SAFEAPI_ENABLE_MEMORY)
    list(APPEND _safeapi_oal_sources
        src/oal/memory/sapi_memory.c
        src/oal/memory/sapi_safe_ptr.c
    )
endif()
if(SAFEAPI_ENABLE_TASK)
    list(APPEND _safeapi_oal_sources src/oal/task/sapi_task.c)
endif()
if(SAFEAPI_ENABLE_IPC)
    list(APPEND _safeapi_oal_sources src/oal/ipc/sapi_ipc.c)
endif()
if(SAFEAPI_ENABLE_NETLINK)
    list(APPEND _safeapi_oal_sources src/oal/netlink/sapi_netlink.c)
endif()
if(SAFEAPI_ENABLE_LOG)
    list(APPEND _safeapi_oal_sources src/oal/log/sapi_log.c)
endif()
if(SAFEAPI_ENABLE_REBOOT)
    list(APPEND _safeapi_oal_sources src/oal/reboot/sapi_reboot.c)
endif()
if(SAFEAPI_ENABLE_WATCHDOG)
    list(APPEND _safeapi_oal_sources src/redundancy/watchdog/sapi_watchdog.c)
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
    list(APPEND _safeapi_channels_sources src/redundancy/checksum/sapi_checksum.c)
endif()
if(SAFEAPI_ENABLE_CHANNEL_LINK)
    list(APPEND _safeapi_channels_sources
        src/redundancy/channel_link/sapi_channel.c
        src/redundancy/channel_service/sapi_channel_service.c
    )
endif()
if(SAFEAPI_ENABLE_VOTER)
    list(APPEND _safeapi_channels_sources src/redundancy/voter/sapi_voter.c)
endif()
if(SAFEAPI_ENABLE_CROSS_COMPARATOR)
    list(APPEND _safeapi_channels_sources src/redundancy/cross_comparator/sapi_cross_comparator.c)
endif()
if(SAFEAPI_ENABLE_CLOCKSYNC)
    list(APPEND _safeapi_channels_sources src/oal/clocksync/sapi_clocksync.c)
endif()
if(SAFEAPI_ENABLE_CHECKPOINT)
    list(APPEND _safeapi_channels_sources src/redundancy/checkpoint/sapi_checkpoint.c)
endif()
if(SAFEAPI_ENABLE_DUAL)
    list(APPEND _safeapi_channels_sources
        src/redundancy/dual/sapi_dual_types.c
        src/redundancy/dual/sapi_dual_msgchannel.c
        src/redundancy/dual/sapi_dual_channel.c
        src/redundancy/dual/sapi_dual_negotiator.c
    )
endif()
if(SAFEAPI_ENABLE_SAFECHANNEL)
    list(APPEND _safeapi_channels_sources src/redundancy/safechannel/sapi_safechannel.c)
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
# checkpoint (channels), so add it last - its own src/app/appmanager/CMakeLists.txt
# is unchanged in shape, only its target_link_libraries() line was
# updated to the 3 new names (ADR-023).
if(SAFEAPI_ENABLE_APPMANAGER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/appmanager/CMakeLists.txt")
    add_subdirectory(src/app/appmanager)
endif()

# Host-side build tool (not part of the shipped safety library) - see
# tools/sapi_crc64_tool.c's own header for why it exists. Needs
# sapi_checksum.c, which only exists in safeapi_channels when
# SAFEAPI_ENABLE_CHECKSUM is ON. Also needs a HOSTED C runtime (file I/O,
# exit(), etc.) - skip it for bare-metal/no-OS targets
# (CMAKE_SYSTEM_NAME "Generic" is CMake's own convention for exactly this,
# set by FreeRTOSToolchain/cmake/Toolchain-FreeRTOS.cmake). Found via a
# real link failure, not assumed: newlib's syscall stubs (_exit/_write/
# _read/_open/_close/_sbrk/_lseek/_isatty/_fstat/_kill/_getpid) are
# undefined for arm-none-eabi-gcc without a real board support package
# providing them - this tool never needed to run ON the target anyway, it
# runs on the BUILD HOST to compute/verify artifact checksums.
if(SAFEAPI_ENABLE_CHECKSUM AND NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    add_executable(sapi_crc64_tool tools/sapi_crc64_tool.c)
    target_link_libraries(sapi_crc64_tool PRIVATE safeapi::channels safeapi::core)
    safeapi_set_warnings(sapi_crc64_tool)
endif()

if(SAFEAPI_BUILD_TESTS)
    # include(CTest), not a bare enable_testing(): it calls enable_testing()
    # internally *and* generates the DartConfiguration.tcl that ctest's
    # built-in memcheck mode reads MEMORYCHECK_COMMAND/_OPTIONS from - the
    # standard CMake mechanism for "run the test suite under Valgrind"
    # rather than a hand-rolled wrapper script (contrast
    # SAFEAPI_ENABLE_COVERAGE above, which has no equivalent built-in ctest
    # mode and so does need scripts/coverage.sh). Introduces the standard
    # BUILD_TESTING cache option (default ON) alongside this project's own
    # SAFEAPI_BUILD_TESTS - the latter still gates add_subdirectory(tests)
    # below, so nothing about the existing opt-out behavior changes.
    include(CTest)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        set(MEMORYCHECK_COMMAND "${VALGRIND_EXECUTABLE}" CACHE FILEPATH "" FORCE)
        set(MEMORYCHECK_COMMAND_OPTIONS
            "--leak-check=full --error-exitcode=1 --track-origins=yes"
            CACHE STRING "" FORCE)
        message(STATUS "Valgrind found: ${VALGRIND_EXECUTABLE}")
        message(STATUS "  Run 'ctest -T memcheck' to run the test suite under Valgrind")
    endif()
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
# Documentation: Doxygen with Mermaid and PlantUML
# ---------------------------------------------------------------------------
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen (HTML, Mermaid, PlantUML)..."
        VERBATIM
    )
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
    message(STATUS "  Run 'cmake --build build --target doc' to generate documentation")
else()
    message(STATUS "Doxygen not found - documentation generation disabled")
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
    # Define install paths. Everything lands under dist/<platform>/ (install
    # with `cmake --install build --prefix .` from the repo root) - one
    # cross/multi-arch build's own output never overwrites another's, now
    # that this project genuinely gets built for more than one
    # CMAKE_SYSTEM_NAME/_PROCESSOR combination in the same checkout (Docker
    # buildx multi-platform builds, cmake/Toolchain-Linux.cmake cross-
    # compiles, QNX). <platform> is CMAKE_SYSTEM_NAME-CMAKE_SYSTEM_PROCESSOR
    # (e.g. Linux-aarch64, Linux-x86_64, Darwin-arm64, QNX-x86_64) - both
    # values are already known at this point (set by the toolchain file, or
    # by CMake itself from uname for a native build) and together uniquely
    # identify one build's own output. safeAPIRBC2oo2's own
    # install(TARGETS ... RUNTIME DESTINATION dist/...) uses this exact
    # same SAFEAPI_DIST_PLATFORM_DIR convention - keep both in sync. A
    # downstream consumer points CMAKE_PREFIX_PATH/find_package at this
    # dist/<platform>/ tree (see cmake/safeAPIFrameworkConfig.cmake.in,
    # which bakes its own platform subdir into the generated config file so
    # it doesn't need to re-detect it at find_package() time).
    set(SAFEAPI_DIST_PLATFORM_DIR "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

    # ADR-008 dual-channel build diversity: -DSAPI_CHANNEL_BUILD_ID=A|B (used
    # together with cmake/toolchain-channel-a.cmake / -b.cmake) rebuilds this
    # framework under a distinct compiler/flags per RBC channel. Rather than
    # suffixing SAFEAPI_DIST_PLATFORM_DIR itself, a channel build adds ONE
    # extra directory level below it - dist/<platform>/A/, dist/<platform>/B/
    # - so it nests under the same dist/<platform>/ root a default build
    # uses. safeAPIRBC2oo2's own SAFEAPI_APP_ROLE_SUBDIR/SAPI_FRAMEWORK_DIST_DIR
    # reconstruct this exact same path to find this channel's framework dist/
    # tree - keep both in sync. A build with no SAPI_CHANNEL_BUILD_ID uses no
    # extra level - dist/<platform>/ directly, unchanged from before this
    # feature existed.
    set(SAFEAPI_DIST_SUBPATH "${SAFEAPI_DIST_PLATFORM_DIR}")
    if(DEFINED SAPI_CHANNEL_BUILD_ID)
        if(NOT SAPI_CHANNEL_BUILD_ID MATCHES "^[AB]$")
            message(FATAL_ERROR "SAPI_CHANNEL_BUILD_ID must be 'A' or 'B' (got '${SAPI_CHANNEL_BUILD_ID}')")
        endif()
        string(APPEND SAFEAPI_DIST_SUBPATH "/${SAPI_CHANNEL_BUILD_ID}")
    endif()

    # Same nesting-subdirectory pattern as SAPI_CHANNEL_BUILD_ID above, an
    # independent optional axis: -DSAPI_LIBC_VARIANT=musl (used with a
    # LinuxMacOSToolchain/bin/<arch>/alpine/ musl cross-compiler) adds one
    # more directory level so a glibc and a musl build for the SAME
    # CMAKE_SYSTEM_PROCESSOR (Toolchain-Linux.cmake's LINUX_CROSS_COMPILE
    # table has no musl entries, so musl builds must pass
    # -DCMAKE_SYSTEM_PROCESSOR explicitly - see LinuxMacOSToolchain/CLAUDE.md)
    # don't collide under the same dist/<platform>/ path. Unset = today's
    # behavior (implicitly glibc/native), unchanged.
    if(DEFINED SAPI_LIBC_VARIANT)
        if(NOT SAPI_LIBC_VARIANT STREQUAL "musl")
            message(FATAL_ERROR "SAPI_LIBC_VARIANT only recognizes 'musl' (got '${SAPI_LIBC_VARIANT}') - leave unset for glibc/native")
        endif()
        string(APPEND SAFEAPI_DIST_SUBPATH "/${SAPI_LIBC_VARIANT}")
    endif()

    set(INCLUDE_INSTALL_DIR dist/${SAFEAPI_DIST_SUBPATH}/include)
    set(LIB_INSTALL_DIR dist/${SAFEAPI_DIST_SUBPATH}/lib)
    set(CMAKE_INSTALL_DIR dist/${SAFEAPI_DIST_SUBPATH}/lib/cmake/safeAPIFramework)

    # Install public (consumer-facing) headers. sapi_core_version.h.in is
    # the TEMPLATE, not the real header (that would install the literal
    # "@SAFEAPI_FRAMEWORK_VERSION@" placeholder, not this build's actual
    # version string) - excluded here, the real generated copy is
    # installed separately right below.
    install(DIRECTORY include/safeapi
        DESTINATION ${INCLUDE_INSTALL_DIR}
        COMPONENT development
        PATTERN "*.h.in" EXCLUDE
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated_include/safeapi/core/sapi_core_version.h"
        DESTINATION ${INCLUDE_INSTALL_DIR}/safeapi/core
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

    # The generated config file installs to
    # <prefix>/dist/${SAFEAPI_DIST_SUBPATH}/lib/cmake/safeAPIFramework/ - it
    # needs to know how many '..' get back to <prefix> to resolve
    # safeAPIFramework_INCLUDE_DIR/_LIB_DIR (see that file's own comments).
    # That depth isn't fixed any more: a default build's SAFEAPI_DIST_SUBPATH
    # is one segment (e.g. "Darwin-arm64"), a channel build's is two (e.g.
    # "Darwin-arm64/A") - compute it here instead of hardcoding a literal
    # that would only be correct for one of the two cases.
    string(REPLACE "/" ";" _safeapi_dist_subpath_segments "${SAFEAPI_DIST_SUBPATH}")
    list(LENGTH _safeapi_dist_subpath_segments _safeapi_dist_subpath_depth)
    math(EXPR _safeapi_config_relpath_depth "${_safeapi_dist_subpath_depth} + 4") # dist/ + lib/cmake/safeAPIFramework (3 levels)
    set(_safeapi_config_relpath_parts "")
    foreach(_safeapi_i RANGE 1 ${_safeapi_config_relpath_depth})
        list(APPEND _safeapi_config_relpath_parts "..")
    endforeach()
    list(JOIN _safeapi_config_relpath_parts "/" SAFEAPI_CONFIG_PREFIX_RELPATH)

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
