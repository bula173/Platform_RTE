# Cross-Compilation Guide

This document describes how to build safeAPIFramework for different target platforms (Linux, QNX, etc.).

## Overview

The framework uses CMake toolchain files to support cross-compilation. Three levels of configuration are provided:

1. **CMake Presets** — Standard, preconfigured build profiles
2. **Toolchain Files** — Platform-specific compiler and flag configuration
3. **Build Scripts** — Ready-to-use shell scripts for common scenarios

---

## Quick Start

### Linux (Native or Cross-compile)

**Native Linux build (your current system):**
```bash
./examples/build-linux-native.sh
```

**Linux release build:**
```bash
./examples/build-linux-native.sh release
```

**Clean build:**
```bash
./examples/build-linux-native.sh clean
```

### QNX RTOS

**Prerequisites:**
1. QNX Momentics IDE or QNX SDP installed
2. Environment variables set:
```bash
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64
```

**Build:**
```bash
./examples/build-qnx.sh
```

**Release build:**
```bash
./examples/build-qnx.sh release
```

---

## Detailed Build Instructions

### Linux (POSIX OAL)

#### 1. Native Linux Build

The framework will use POSIX APIs (`pthreads`, `POSIX timers`, etc.) for OAL implementations.

**Method 1: Using preset script**
```bash
./examples/build-linux-native.sh debug
```

**Method 2: Using CMake directly**
```bash
cmake --preset linux-native
cmake --build --preset linux-native
ctest --preset linux-native
```

**Method 3: Manual CMake**
```bash
cmake -S . -B build/linux \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Linux.cmake \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/linux
ctest --test-dir build/linux
```

#### 2. Linux Release Build

Optimized build with all tests enabled:
```bash
./examples/build-linux-native.sh release
```

#### 3. Install to System

After building, install headers and libraries:
```bash
cmake --install build/linux-native --prefix /usr/local
```

This installs:
- Headers: `/usr/local/include/safeapi/`
- Libraries: `/usr/local/lib/libsafeapi_*.a`
- CMake config: `/usr/local/lib/cmake/safeAPIFramework/`

#### 4. Use in Your Application

Create a `CMakeLists.txt` for your app:
```cmake
cmake_minimum_required(VERSION 3.16)
project(my_rbc_app C)

find_package(safeAPIFramework REQUIRED)

add_executable(my_app main.c)
target_link_libraries(my_app
    safeapi::core
    safeapi::oal
    safeapi::channels
    safeapi::appmanager
    pthread
)

# For POSIX OAL, link pthread
target_link_libraries(my_app pthread)
```

Build your app:
```bash
cmake -S . -B build
cmake --build build
./build/my_app
```

---

### Multi-Architecture Docker Builds (Platform_RTE example app layer)

`cmake/Toolchain-Linux.cmake`'s own `LINUX_CROSS_COMPILE` variable (above) is for a real host
cross-compiler toolchain (e.g. `aarch64-linux-gnu-gcc` installed on a Linux build machine or CI
runner) - it has not been exercised end to end against a real one anywhere this framework has
actually been built and tested; the mappings it sets up are reviewed-correct, not proven.

The genuinely working, verified multi-architecture path today is different, and lives among this
framework's sibling projects in the workspace (`RBC_GP` and the three Python sims, direct
workspace-root siblings - see root `CLAUDE.md`): **Docker buildx with QEMU emulation**. Every one of
those Dockerfiles installs its own toolchain via plain `apt-get`/`pip` with no arch-specific
package names or triplets - under `docker buildx build --platform <target>`, that just installs
the *target* arch's own native compiler/interpreter inside an emulated container and compiles
there. No cross-compiler is involved at all; it is genuinely native compilation, just running
under emulation.

```bash
cd RBC_Test_Env
etc/scripts/build_multiarch.sh -i rbc2oo2 -p linux/arm64,linux/386
docker run --rm --entrypoint uname safeapi-rbc2oo2:latest-386 -m   # -> i686/i386
```

Supports `linux/amd64`, `linux/arm64`, `linux/386`, `linux/arm/v7` (Docker Desktop's own
`desktop-linux` buildx builder already has QEMU emulators registered for all four - check with
`docker buildx ls`). `docker buildx build --load` only loads one platform's image locally per
invocation (a real multi-platform manifest needs a registry push, not configured for this
project), so the script builds one `{image, platform}` pair at a time, tagging each with its own
arch suffix (`:latest-arm64`, `:latest-386`, ...) so they coexist in `docker images`.

This path proves the *application layer* (`RBC_GP`, built via this framework's POSIX OAL
backend) runs correctly on ARM64/x86/32-bit/64-bit Linux - it does not exercise
`Toolchain-Linux.cmake`'s own cross-compile branch above, since buildx never cross-compiles.

---

### QNX RTOS

#### 1. Prerequisites

**QNX Environment:**
- QNX Momentics IDE 7.0 or later, OR
- QNX SDP (Software Development Platform)
- CMake 3.16+

**Verify Installation:**
```bash
echo $QNX_HOST
echo $QNX_TARGET
ls $QNX_HOST/usr/bin/qcc          # Should exist
ls $QNX_TARGET/usr/include/errno.h # Should exist
```

#### 2. Set Environment Variables

```bash
# QNX 7.0.0 example
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64

# Verify
qcc --version
```

#### 3. Build for QNX

**Method 1: Using preset script**
```bash
./examples/build-qnx.sh debug
```

**Method 2: Using CMake presets**
```bash
cmake --preset qnx
cmake --build --preset qnx
```

**Method 3: Manual CMake**
```bash
cmake -S . -B build/qnx \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-QNX.cmake \
  -DQNX_HOST=$QNX_HOST \
  -DQNX_TARGET=$QNX_TARGET \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSAFEAPI_BUILD_TESTS=OFF

cmake --build build/qnx
```

**Note:** Tests are disabled for QNX targets (cannot run cross-compiled binaries on host).

#### 4. Deploy to QNX Target

After building, deploy to your QNX system:

```bash
# Copy libraries
scp build/qnx/src/*/*.a user@qnx-target:/opt/rbc/lib/

# Copy headers
scp -r include/safeapi user@qnx-target:/opt/rbc/include/

# Copy CMake config (optional, if using find_package)
scp -r build/qnx/cmake user@qnx-target:/opt/rbc/lib/cmake/
```

#### 5. Use in Your QNX Application

**QNX app `CMakeLists.txt`:**
```cmake
cmake_minimum_required(VERSION 3.16)
project(my_rbc_qnx C)

# Toolchain file (use same as framework)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/Toolchain-QNX.cmake)

# Find framework
find_package(safeAPIFramework REQUIRED)

add_executable(my_rbc_app main.c)
target_link_libraries(my_rbc_app
    safeapi::core
    safeapi::oal
    safeapi::channels
    safeapi::appmanager
)

# QNX requires certain libraries
target_link_libraries(my_rbc_app pthread socket)
```

**Build your QNX app:**
```bash
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64

cmake -S . -B build/qnx \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-QNX.cmake

cmake --build build/qnx
```

**Deploy and run on QNX target:**
```bash
scp build/qnx/my_rbc_app user@qnx-target:/opt/rbc/bin/
ssh user@qnx-target /opt/rbc/bin/my_rbc_app
```

---

## CMake Presets Reference

### Linux Presets

| Preset | Description | Notes |
|--------|-------------|-------|
| `linux-native` | Debug build for native Linux | Uses native system compiler; tests enabled |
| `linux-release` | Optimized release build | `-O2` optimizations; tests enabled |

**Usage:**
```bash
cmake --preset linux-native
cmake --build --preset linux-native
ctest --preset linux-native
```

### QNX Presets

| Preset | Description | Notes |
|--------|-------------|-------|
| `qnx` | Debug build for QNX RTOS | Requires QNX_HOST, QNX_TARGET env vars; tests disabled |
| `qnx-release` | Release build for QNX | Optimized; tests disabled |

**Usage:**
```bash
export QNX_HOST=... QNX_TARGET=...
cmake --preset qnx
cmake --build --preset qnx
```

---

## Toolchain Files

### Toolchain-Linux.cmake

Used for building on/for Linux systems. Enables:
- Position-independent code (`-fPIC`)
- POSIX threading (`-pthread`)
- Optional: Stack protector, address sanitizer

**Key variables:**
- `LINUX_CROSS_COMPILE` — Set to GCC prefix if cross-compiling (e.g., `arm-linux-gnueabihf`)

**Example: Cross-compile to ARM Linux**
```bash
cmake -S . -B build/arm-linux \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Linux.cmake \
  -DLINUX_CROSS_COMPILE=arm-linux-gnueabihf
```

### Toolchain-QNX.cmake

Cross-compiles for QNX RTOS. Requires:
- `QNX_HOST` — Path to QNX host tools
- `QNX_TARGET` — Path to QNX target root

Configures:
- `qcc` compiler (QNX's GCC wrapper)
- Target-specific flags (`-Vgcc_ntoX86_64`)
- `__QNX__` preprocessor define
- PIC code for shared objects

**Required environment:**
```bash
export QNX_HOST=/path/to/qnx/host/...
export QNX_TARGET=/path/to/qnx/target/...
```

---

## Troubleshooting

### "CMake not found" / "qcc not found"

**Solution:** Ensure tools are installed and in PATH:
```bash
which cmake
which qcc
which arm-linux-gnueabihf-gcc  # For ARM cross-compile
```

### "QNX_HOST and QNX_TARGET not set"

**Solution:** Set environment variables:
```bash
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64
```

Or pass via CMake:
```bash
cmake -S . -B build \
  -DQNX_HOST=$QNX_HOST \
  -DQNX_TARGET=$QNX_TARGET
```

### Tests fail on cross-compiled targets

**Reason:** Cross-compiled binaries cannot run on build host.

**Solution:** Tests are disabled automatically for QNX (SAFEAPI_BUILD_TESTS=OFF). For other cross-compiles, disable manually:
```bash
cmake -S . -B build -DSAFEAPI_BUILD_TESTS=OFF
```

### "Compiler warnings treated as errors"

**Solution:** Build in less-strict mode:
```bash
cmake -S . -B build -DSAFEAPI_WARNINGS_AS_ERRORS=OFF
```

Or use a different preset (e.g., `minimal`):
```bash
cmake --preset minimal
```

---

## Adding New Targets

To support a new platform (e.g., ARM Cortex-M, INTEGRITY OS):

1. **Create toolchain file:** `cmake/Toolchain-{Platform}.cmake`
   ```cmake
   set(CMAKE_SYSTEM_NAME MyOS)
   set(CMAKE_C_COMPILER my-compiler)
   # ... platform-specific config
   ```

2. **Add CMake presets:** Edit `CMakePresets.json`
   ```json
   {
     "name": "myos",
     "cacheVariables": {
       "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-MyOS.cmake"
     }
   }
   ```

3. **Create build script:** `examples/build-myos.sh`

4. **Document in this file** under "Detailed Build Instructions"

---

## References

- [CMake Toolchain Files](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- QNX Documentation: https://www.qnx.com/developers/docs/
- [README.md](templates/README.md) — General build instructions
