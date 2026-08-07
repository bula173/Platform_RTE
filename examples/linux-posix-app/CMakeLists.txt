cmake_minimum_required(VERSION 3.16)
project(railway-message-processor-linux C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Find safeAPIFramework
find_package(safeAPIFramework REQUIRED)

# Example executable
add_executable(railway-processor main.c)

# Link safeAPIFramework modules (ADR-023: status/types/buffer/string/
# safestate live in safeapi::core, log/timer live in safeapi::oal - 22
# per-feature targets were consolidated into 4 grouped libraries).
target_link_libraries(railway-processor PRIVATE
    safeapi::core
    safeapi::oal
)

# POSIX-specific: link pthread
target_link_libraries(railway-processor PRIVATE pthread)

# Compiler flags
target_compile_options(railway-processor PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

# Installation
install(TARGETS railway-processor DESTINATION bin)
