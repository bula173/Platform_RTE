# Toolchain file for Linux target (native or cross-compilation)
# Usage: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Linux.cmake
#
# Cross-compiling: set LINUX_CROSS_COMPILE to the target triplet's GCC prefix
# (e.g. -DLINUX_CROSS_COMPILE=aarch64-linux-gnu). CMAKE_SYSTEM_PROCESSOR and
# target-appropriate codegen flags are derived from that same prefix below -
# do not introduce a separate arch-selection variable.
#
# Recognized prefixes (map to CMAKE_SYSTEM_PROCESSOR + flags):
#   aarch64-linux-gnu       -> aarch64, -march=armv8-a
#   arm-linux-gnueabihf     -> arm,     -march=armv7-a -mfpu=neon -mfloat-abi=hard
#   x86_64-linux-gnu        -> x86_64,  -march=x86-64
#   i686-linux-gnu          -> i686,    -m32 -march=i686
# An unrecognized prefix still sets CMAKE_C_COMPILER/CMAKE_CXX_COMPILER but
# warns and leaves CMAKE_SYSTEM_PROCESSOR/flags unset - pass
# -DCMAKE_SYSTEM_PROCESSOR=... yourself in that case.
#
# None of the 4 mappings above are exercised by a real cross-gcc as part of
# this project's own CI/dev verification today (no aarch64-linux-gnu-gcc/
# arm-linux-gnueabihf-gcc/etc. toolchain is installed anywhere this has been
# tested) - they are reviewed-correct, not independently proven. The
# genuinely verified multi-architecture path is safeAPIExample's own Docker
# buildx/QEMU build (safeAPIExample/safeAPITestEnv/etc/scripts/build_multiarch.sh),
# which compiles natively inside an emulated container per target arch
# instead of cross-compiling from this toolchain file - see
# docs/CROSS_COMPILATION.md's own "Multi-Architecture Docker Builds" section.

set(CMAKE_SYSTEM_NAME Linux)

if(DEFINED LINUX_CROSS_COMPILE)
    # Cross-compilation to Linux from another OS/arch
    set(CMAKE_C_COMPILER ${LINUX_CROSS_COMPILE}-gcc)
    set(CMAKE_CXX_COMPILER ${LINUX_CROSS_COMPILE}-g++)

    if(LINUX_CROSS_COMPILE STREQUAL "aarch64-linux-gnu")
        set(CMAKE_SYSTEM_PROCESSOR aarch64)
        set(SAFEAPI_LINUX_ARCH_FLAGS "-march=armv8-a")
    elseif(LINUX_CROSS_COMPILE STREQUAL "arm-linux-gnueabihf")
        set(CMAKE_SYSTEM_PROCESSOR arm)
        set(SAFEAPI_LINUX_ARCH_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard")
    elseif(LINUX_CROSS_COMPILE STREQUAL "x86_64-linux-gnu")
        set(CMAKE_SYSTEM_PROCESSOR x86_64)
        set(SAFEAPI_LINUX_ARCH_FLAGS "-march=x86-64")
    elseif(LINUX_CROSS_COMPILE STREQUAL "i686-linux-gnu")
        set(CMAKE_SYSTEM_PROCESSOR i686)
        set(SAFEAPI_LINUX_ARCH_FLAGS "-m32 -march=i686")
    else()
        message(WARNING "Toolchain-Linux.cmake: unrecognized LINUX_CROSS_COMPILE "
                         "prefix '${LINUX_CROSS_COMPILE}' - CMAKE_SYSTEM_PROCESSOR "
                         "not set automatically; pass -DCMAKE_SYSTEM_PROCESSOR=... "
                         "yourself if the build needs it.")
        set(SAFEAPI_LINUX_ARCH_FLAGS "")
    endif()
else()
    # Native Linux build (gcc/clang available on system). CMAKE_HOST_SYSTEM_PROCESSOR
    # is populated by CMake itself (from uname -m) before this file runs, and is
    # correct for whatever host actually runs the build - do not hardcode one arch
    # (found live: the previous version of this file hardcoded x86_64 here
    # unconditionally, wrong on any non-x86_64 native host, e.g. this project's own
    # arm64 macOS dev machines and Apple Silicon/ARM64 Linux CI runners).
    set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
    set(SAFEAPI_LINUX_ARCH_FLAGS "")
endif()

# Linux-specific compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SAFEAPI_LINUX_ARCH_FLAGS} -fPIC -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SAFEAPI_LINUX_ARCH_FLAGS} -fPIC -pthread")

# Enable position-independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Disable stack protector if embedded-focused
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector")

# Skip rpath so built binaries are portable
set(CMAKE_SKIP_RPATH ON)

message(STATUS "Toolchain: Linux (POSIX-based OAL)")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Processor: ${CMAKE_SYSTEM_PROCESSOR}")
