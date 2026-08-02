# Toolchain file for QNX RTOS target
# Usage: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-QNX.cmake \
#              -DQNX_HOST=/path/to/qnx/host/linux/x86_64 \
#              -DQNX_TARGET=/path/to/qnx/target/qnx7.0.0/x86_64

# QNX environment variables (passed via -D flags)
if(NOT QNX_HOST)
    message(FATAL_ERROR "QNX_HOST not set. Provide via: -DQNX_HOST=/path/to/qnx/host/...")
endif()

if(NOT QNX_TARGET)
    message(FATAL_ERROR "QNX_TARGET not set. Provide via: -DQNX_TARGET=/path/to/qnx/target/...")
endif()

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# QNX compilers (from QNX_HOST)
set(CMAKE_C_COMPILER ${QNX_HOST}/usr/bin/qcc)
set(CMAKE_CXX_COMPILER ${QNX_HOST}/usr/bin/qcc)

# QNX target directory
set(CMAKE_FIND_ROOT_PATH ${QNX_TARGET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# QNX compiler flags
# qcc is a GCC wrapper for QNX; -Vgcc_ntoarmv7le specifies target variant
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Vgcc_ntoX86_64 -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Vgcc_ntoX86_64 -fPIC")

# QNX-specific settings
add_definitions(-D__QNX__)

# Position-independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

message(STATUS "Toolchain: QNX RTOS")
message(STATUS "  QNX_HOST: ${QNX_HOST}")
message(STATUS "  QNX_TARGET: ${QNX_TARGET}")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Processor: ${CMAKE_SYSTEM_PROCESSOR}")
