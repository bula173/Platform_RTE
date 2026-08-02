cmake_minimum_required(VERSION 3.16)
project(railway-message-processor-linux C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Find safeAPIFramework
find_package(safeAPIFramework REQUIRED)

# Example executable
add_executable(railway-processor main.c)

# Link safeAPIFramework modules
target_link_libraries(railway-processor PRIVATE
    safeapi::status
    safeapi::types
    safeapi::buffer
    safeapi::string
    safeapi::log
    safeapi::timer
    safeapi::safestate
)

# POSIX-specific: link pthread
target_link_libraries(railway-processor PRIVATE pthread)

# Compiler flags
target_compile_options(railway-processor PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

# Installation
install(TARGETS railway-processor DESTINATION bin)
