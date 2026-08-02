# Toolchain file for Linux target (native or cross-compilation)
# Usage: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Linux.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# If cross-compiling, set the C and CXX compilers
# For native Linux build, these will be found automatically
if(DEFINED LINUX_CROSS_COMPILE)
    # Cross-compilation to Linux from another OS
    set(CMAKE_C_COMPILER ${LINUX_CROSS_COMPILE}-gcc)
    set(CMAKE_CXX_COMPILER ${LINUX_CROSS_COMPILE}-g++)
else()
    # Native Linux build (gcc/clang available on system)
    # CMake will find them automatically
endif()

# Linux-specific compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -pthread")

# Enable position-independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Disable stack protector if embedded-focused
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector")

# Skip rpath so built binaries are portable
set(CMAKE_SKIP_RPATH ON)

message(STATUS "Toolchain: Linux (POSIX-based OAL)")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Processor: ${CMAKE_SYSTEM_PROCESSOR}")
