cmake_minimum_required(VERSION 3.16)
project(railway-control-server-qnx C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# QNX RTOS requires the toolchain file
# Pass with: -DCMAKE_TOOLCHAIN_FILE=../../cmake/Toolchain-QNX.cmake
if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
    message(FATAL_ERROR "This example requires QNX RTOS. Use: -DCMAKE_TOOLCHAIN_FILE=../../cmake/Toolchain-QNX.cmake")
endif()

# Find safeAPIFramework
# Framework must be built for QNX first
find_package(safeAPIFramework REQUIRED)

# QNX server executable
add_executable(railway-server main.c)

# Link safeAPIFramework modules (ADR-023: status/safestate live in
# safeapi::core, log lives in safeapi::oal - 22 per-feature targets were
# consolidated into 4 grouped libraries).
target_link_libraries(railway-server PRIVATE
    safeapi::core
    safeapi::oal
)

# QNX-specific libraries
target_link_libraries(railway-server PRIVATE
    c
    m
)

# Compiler flags for QNX
target_compile_options(railway-server PRIVATE
    -Wall -Wextra -Wpedantic
)

# QNX-specific preprocessor defines
target_compile_definitions(railway-server PRIVATE
    __QNX__
)

# Installation
install(TARGETS railway-server DESTINATION bin)

# Documentation
message(STATUS "QNX RTOS Railway Server Example")
message(STATUS "  Build with: cmake --preset qnx && cmake --build --preset qnx")
message(STATUS "  Deploy to QNX target")
message(STATUS "  Run on target: /opt/rbc/bin/railway-server")
