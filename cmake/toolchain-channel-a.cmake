# ADR-008: toolchain for producing the "channel A" vital binary.
#
# Channel A is treated as this project's baseline toolchain (GCC, whatever
# version is found on PATH) at a fixed, named optimization profile.
# Diversity from channel B comes primarily from using a genuinely
# different compiler for channel B (see toolchain-channel-b.cmake) and,
# secondarily, from a different optimization/codegen flag set even in the
# single-compiler fallback case - see that file's comments and ADR-008
# section 3 for why the fallback is documented as reduced, not equivalent,
# mitigation.
#
# Usage (configure a separate build directory per channel - do not reuse
# one build directory for both):
#   cmake -S . -B build-channel-a \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-channel-a.cmake \
#       -DSAPI_CHANNEL_BUILD_ID=A
#   cmake --build build-channel-a

find_program(SAPI_CHANNEL_A_C_COMPILER NAMES gcc)
if(SAPI_CHANNEL_A_C_COMPILER)
    set(CMAKE_C_COMPILER "${SAPI_CHANNEL_A_C_COMPILER}" CACHE FILEPATH "Channel A C compiler")
else()
    message(WARNING "toolchain-channel-a: gcc not found on PATH; falling back to CMake's default compiler search. Channel diversity from toolchain-channel-b.cmake will only hold if that file actually resolves to a different compiler than whatever is picked here.")
endif()

# Baseline optimization/codegen profile for channel A.
set(CMAKE_C_FLAGS_INIT "-O2")
