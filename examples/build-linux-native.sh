#!/bin/bash
# Build RteFramework for native Linux (POSIX OAL)
#
# This script builds the framework for the host Linux system.
# The framework will use POSIX APIs (pthreads, POSIX timers, etc.) for task and timer implementations.
#
# Usage:
#   ./examples/build-linux-native.sh                    # Debug build
#   ./examples/build-linux-native.sh release            # Release build
#   ./examples/build-linux-native.sh clean              # Clean build directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/linux-native"
BUILD_TYPE="${1:-debug}"

echo "=========================================="
echo "Building RteFramework for Linux (POSIX)"
echo "=========================================="
echo "Build Type: $BUILD_TYPE"
echo "Build Directory: $BUILD_DIR"

# Handle clean
if [ "$BUILD_TYPE" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    exit 0
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure with CMake preset (run from source dir to find CMakePresets.json)
if [ "$BUILD_TYPE" = "release" ]; then
    echo "Configuring with linux-release preset..."
    cmake --preset linux-release -S "$SCRIPT_DIR" -B "$BUILD_DIR"
    echo "Building..."
    cmake --build "$BUILD_DIR"
    echo "Running tests..."
    cd "$BUILD_DIR" && ctest
else
    echo "Configuring with linux-native preset..."
    cmake --preset linux-native -S "$SCRIPT_DIR" -B "$BUILD_DIR"
    echo "Building..."
    cmake --build "$BUILD_DIR"
    echo "Running tests..."
    cd "$BUILD_DIR" && ctest
fi

echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
echo ""
echo "Output:"
echo "  Headers: ${SCRIPT_DIR}/include/rte"
echo "  Libraries: ${BUILD_DIR}"
echo ""
echo "Install to system:"
echo "  cmake --install ${BUILD_DIR} --prefix /usr/local"
