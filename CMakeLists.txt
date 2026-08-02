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
# Order matters only where a feature links another (string -> buffer, cast).
foreach(feature status types buffer cast safestate string
                timer nvm memory task ipc log reboot)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${feature}/CMakeLists.txt")
        add_subdirectory(src/${feature})
    endif()
endforeach()

if(SAFEAPI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
