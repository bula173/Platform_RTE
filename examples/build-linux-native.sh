#!/bin/bash
# Build safeAPIFramework for native Linux (POSIX OAL)
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
echo "Building safeAPIFramework for Linux (POSIX)"
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
cd "$BUILD_DIR"

# Configure with CMake preset
if [ "$BUILD_TYPE" = "release" ]; then
    echo "Configuring with linux-release preset..."
    cmake --preset linux-release -S "$SCRIPT_DIR"
    echo "Building..."
    cmake --build --preset linux-release
    echo "Running tests..."
    ctest --preset linux-release
else
    echo "Configuring with linux-native preset..."
    cmake --preset linux-native -S "$SCRIPT_DIR"
    echo "Building..."
    cmake --build --preset linux-native
    echo "Running tests..."
    ctest --preset linux-native
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
echo "Install to system:"
echo "  cmake --install ${BUILD_DIR} --prefix /usr/local"
