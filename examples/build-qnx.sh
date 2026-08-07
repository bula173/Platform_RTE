#!/bin/bash
# Build safeAPIFramework for QNX RTOS target
#
# QNX RTOS is a real-time operating system commonly used in automotive and industrial systems.
# This script cross-compiles the framework for a QNX target.
#
# Prerequisites:
#   1. QNX Momentics IDE or QNX SDP (Software Development Platform) installed
#   2. Environment variables set:
#      - QNX_HOST: path to QNX host tools (e.g., /opt/qnx7.0.0/host/linux/x86_64)
#      - QNX_TARGET: path to QNX target (e.g., /opt/qnx7.0.0/target/qnx7.0.0/x86_64)
#
# Usage:
#   ./examples/build-qnx.sh                    # Debug build (uses QNX_HOST/QNX_TARGET env vars)
#   ./examples/build-qnx.sh release            # Release build
#   ./examples/build-qnx.sh clean              # Clean build directory
#
# Example with QNX 7.0.0:
#   export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
#   export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64
#   ./examples/build-qnx.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/qnx"
BUILD_TYPE="${1:-debug}"

# Verify QNX environment variables
if [ -z "$QNX_HOST" ] || [ -z "$QNX_TARGET" ]; then
    echo "ERROR: QNX_HOST and QNX_TARGET environment variables not set"
    echo ""
    echo "Set them before running this script:"
    echo "  export QNX_HOST=/path/to/qnx/host/linux/x86_64"
    echo "  export QNX_TARGET=/path/to/qnx/target/qnx7.0.0/x86_64"
    echo ""
    exit 1
fi

echo "=========================================="
echo "Building safeAPIFramework for QNX RTOS"
echo "=========================================="
echo "Build Type: $BUILD_TYPE"
echo "Build Directory: $BUILD_DIR"
echo "QNX_HOST: $QNX_HOST"
echo "QNX_TARGET: $QNX_TARGET"

# Verify QNX tools exist
if [ ! -f "$QNX_HOST/usr/bin/qcc" ]; then
    echo "ERROR: QCC compiler not found at $QNX_HOST/usr/bin/qcc"
    echo "Verify QNX_HOST is correct"
    exit 1
fi

# Handle clean
if [ "$BUILD_TYPE" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    exit 0
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake preset
if [ "$BUILD_TYPE" = "release" ]; then
    echo "Configuring with qnx-release preset..."
    cmake --preset qnx-release -S "$SCRIPT_DIR"
    echo "Building..."
    cmake --build --preset qnx-release
else
    echo "Configuring with qnx preset..."
    cmake --preset qnx -S "$SCRIPT_DIR"
    echo "Building..."
    cmake --build --preset qnx
    # Note: QNX builds skip tests (tests disabled in preset)
fi

echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
echo ""
echo "Output:"
echo "  Headers: ${SCRIPT_DIR}/include/safeapi"
echo "  Libraries: ${BUILD_DIR}"
echo ""
echo "Next steps:"
echo "  1. Deploy to QNX target:"
echo "     scp ${BUILD_DIR}/*.a ${BUILD_DIR}/src/appmanager/*.a target_user@qnx_target:/opt/app/lib/"
echo "     scp -r ${SCRIPT_DIR}/include/safeapi target_user@qnx_target:/opt/app/include/"
echo ""
echo "  2. In QNX application CMakeLists.txt (ADR-023: 4 consolidated"
echo "     libraries - safeapi::core, safeapi::oal, safeapi::channels,"
echo "     safeapi::appmanager - instead of one per feature):"
echo "     find_package(safeAPIFramework REQUIRED)"
echo "     target_link_libraries(my_app safeapi::core safeapi::oal ...)"
