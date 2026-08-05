# ADR-008: toolchain for producing the "channel B" vital binary.
#
# Prefers a genuinely different compiler (Clang) from channel A's GCC, so a
# compiler-specific codegen fault is less likely to hit both channels
# identically (ADR-008 section 2.1, "diverse compilation"). If Clang is
# not available on this machine, falls back to GCC with a deliberately
# different optimization/codegen flag set from channel A's - a weaker form
# of diversity than a second compiler, but still not "build byte-for-byte
# the same way twice." ADR-008 section 3 documents this fallback case
# explicitly as reduced, not equivalent, mitigation: qualifying a real
# second compiler toolchain for the target hardware is project follow-up
# work this file cannot complete on its own.
#
# Usage (configure a separate build directory per channel - do not reuse
# one build directory for both):
#   cmake -S . -B build-channel-b \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-channel-b.cmake \
#       -DSAPI_CHANNEL_BUILD_ID=B
#   cmake --build build-channel-b

find_program(SAPI_CHANNEL_B_CLANG_COMPILER NAMES clang)
if(SAPI_CHANNEL_B_CLANG_COMPILER)
    set(CMAKE_C_COMPILER "${SAPI_CHANNEL_B_CLANG_COMPILER}" CACHE FILEPATH "Channel B C compiler")
    set(CMAKE_C_FLAGS_INIT "-O1")
else()
    message(WARNING "toolchain-channel-b: clang not found on PATH; falling back to GCC with a diverse flag set from channel A. This is a REDUCED mitigation - see ADR-008 section 3 - not a substitute for qualifying a real second compiler.")
    find_program(SAPI_CHANNEL_B_GCC_COMPILER NAMES gcc)
    if(SAPI_CHANNEL_B_GCC_COMPILER)
        set(CMAKE_C_COMPILER "${SAPI_CHANNEL_B_GCC_COMPILER}" CACHE FILEPATH "Channel B C compiler (GCC fallback)")
    endif()
    set(CMAKE_C_FLAGS_INIT "-O1 -fno-strict-aliasing")
endif()
