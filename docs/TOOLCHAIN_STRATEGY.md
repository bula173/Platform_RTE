# Toolchain Strategy & Multi-Platform Support

**Document:** Toolchain Architecture & Cross-Compilation Strategy  
**Version:** 1.0  
**Date:** 2026-08-02  
**Status:** APPROVED  

---

> **Implementation status (2026-08-25):** This document is aspirational/design-reference only.
> Despite the "Status: APPROVED" label above and the "Phase 1 ... DONE" claim below, none of the
> toolchain files, `backends/` tree, or `SAFEAPI_ARCH_*`/`SAFEAPI_LITTLE_ENDIAN`/
> `SAFEAPI_BIG_ENDIAN`/`SAFEAPI_STRICT_ALIGNMENT` macros named in this document exist in the repo
> today - only `cmake/Toolchain-Linux.cmake` and `cmake/Toolchain-QNX.cmake` do, under different
> names and without the macros this document proposes (checked directly: no real source file in
> `include/` or `src/` reads or would benefit from those macros - endianness is already handled
> per-call via `sapi_buffer_write_u16_le()`/`_be()` etc., not compile-time branching). The
> genuinely working, verified multi-architecture story today is Docker buildx/QEMU for the
> safeAPIExample app layer - see `docs/CROSS_COMPILATION.md`'s own "Multi-Architecture Docker
> Builds" section and `safeAPIExample/safeAPITestEnv/etc/scripts/build_multiarch.sh`. Treat
> everything below this notice as a forward-looking design reference, not a status report.

---

## 1. Overview

safeAPIFramework supports multiple **combinations** of:
- **Operating Systems** (Linux, QNX, VxWorks, INTEGRITY, FreeRTOS, bare-metal)
- **Architectures** (x86_64, x86, ARM32, ARM64, PowerPC, MIPS)
- **Endianness** (little-endian, big-endian)
- **Compilers** (GCC, Clang, IAR Embedded Workbench, DIAB, TASKING)

This document defines the supported matrix and provides toolchain files for each combination.

---

## 2. Support Matrix

### 2.1 Operating Systems & RTOS

| OS/RTOS | Target | Status | Backend | Toolchain |
|---------|--------|--------|---------|-----------|
| **Linux (POSIX)** | Development/Testing | ✅ Supported | POSIX OAL | GCC, Clang |
| **QNX Neutrino** | Railway SIL 3/4 | ✅ Supported | QNX OAL | QCC (GCC-based) |
| **VxWorks** | Railway SIL 3/4 | ⏳ Planned | VxWorks OAL | WindRiver compiler |
| **INTEGRITY** | Railway SIL 3/4 | ⏳ Planned | INTEGRITY OAL | GreenHills compiler |
| **FreeRTOS** | Embedded | ⏳ Planned | FreeRTOS OAL | GCC, Clang |
| **Bare Metal** | Embedded | ⏳ Planned | None (direct HW) | GCC, Clang |

### 2.2 Architecture Support

| Architecture | Endianness | Linux | QNX | VxWorks | FreeRTOS | Status |
|--------------|-----------|-------|-----|---------|----------|--------|
| **x86_64** | LE | ✅ | ✅ | ✅ | ✅ | Fully tested |
| **x86** | LE | ✅ | ✅ | ✅ | ✅ | Supported |
| **ARM64 (AArch64)** | LE | ✅ | ✅ | ✅ | ✅ | Fully tested |
| **ARM32 (ARMv7)** | LE | ✅ | ✅ | ✅ | ✅ | Fully tested |
| **ARM32 (ARMv7)** | BE | ⏳ | ⏳ | ✅ | ⏳ | In progress |
| **PowerPC 32** | BE | ⏳ | ⏳ | ✅ | ⏳ | In progress |
| **PowerPC 64** | BE | ⏳ | ⏳ | ✅ | ⏳ | In progress |
| **MIPS32** | LE/BE | ⏳ | ⏳ | ✅ | ⏳ | Planned |

### 2.3 Compiler Support

| Compiler | Linux | QNX | VxWorks | FreeRTOS | Status |
|----------|-------|-----|---------|----------|--------|
| **GCC** (9.x - 12.x) | ✅ | ✅ | ✅ | ✅ | Full support |
| **Clang** (12.x - 16.x) | ✅ | ✅ | ⏳ | ✅ | Full support (Linux/FreeRTOS) |
| **IAR Embedded** | ⏳ | ⏳ | ✅ | ✅ | Commercial systems |
| **DIAB** (WindRiver) | ⏳ | ⏳ | ✅ | ⏳ | VxWorks systems |
| **GreenHills** | ⏳ | ⏳ | ⏳ | ✅ | INTEGRITY systems |

---

## 3. Toolchain Directory Structure

```
cmake/
├── Toolchain-Linux-x86_64-gcc.cmake       POSIX x86_64, GCC
├── Toolchain-Linux-x86_64-clang.cmake     POSIX x86_64, Clang
├── Toolchain-Linux-x86-gcc.cmake          POSIX x86, GCC
├── Toolchain-Linux-ARM64-gcc.cmake        ARM64 (AArch64), GCC
├── Toolchain-Linux-ARM64-clang.cmake      ARM64, Clang
├── Toolchain-Linux-ARM32-gcc.cmake        ARM32 (ARMv7-LE), GCC
├── Toolchain-Linux-ARM32-clang.cmake      ARM32, Clang
├── Toolchain-Linux-ARM32BE-gcc.cmake      ARM32 (ARMv7-BE), GCC
│
├── Toolchain-QNX-x86_64.cmake             QNX x86_64
├── Toolchain-QNX-x86.cmake                QNX x86
├── Toolchain-QNX-ARM64.cmake              QNX ARM64
├── Toolchain-QNX-ARM32.cmake              QNX ARM32
├── Toolchain-QNX-ARM32BE.cmake            QNX ARM32 big-endian
├── Toolchain-QNX-PowerPC32.cmake          QNX PowerPC32
│
├── Toolchain-VxWorks-PPC32.cmake          VxWorks PowerPC32
├── Toolchain-VxWorks-PPC64.cmake          VxWorks PowerPC64
├── Toolchain-VxWorks-ARM32.cmake          VxWorks ARM32
├── Toolchain-VxWorks-ARM64.cmake          VxWorks ARM64
│
├── Toolchain-FreeRTOS-ARM32.cmake         FreeRTOS ARM32
├── Toolchain-FreeRTOS-ARM64.cmake         FreeRTOS ARM64
│
├── CompilerWarnings.cmake                 Shared warning flags
└── CMakeLists-Arch.cmake                  Architecture detection

backends/
├── posix/                                 Linux/POSIX backend
│   ├── src/
│   └── include/
├── qnx/                                   QNX backend
│   ├── src/
│   └── include/
├── vxworks/                               VxWorks backend (stub)
├── integrity/                             INTEGRITY backend (stub)
└── freertos/                              FreeRTOS backend (stub)
```

---

## 4. Example Toolchain Files

### 4.1 Linux with GCC (Little-Endian x86_64)

**File: `cmake/Toolchain-Linux-x86_64-gcc.cmake`**

```cmake
# Linux x86_64 (little-endian) with GCC
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_AR ar)
set(CMAKE_RANLIB ranlib)

# Architecture flags
set(CMAKE_C_FLAGS "-m64 -march=x86-64" CACHE STRING "")

# Endianness detection
set(SAFEAPI_ENDIAN LITTLE CACHE STRING "System endianness")
add_compile_definitions(SAFEAPI_LITTLE_ENDIAN)

# Backend selection
add_compile_definitions(SAFEAPI_BACKEND_POSIX)

# Enable testing
enable_testing()
```

### 4.2 Linux with Clang (Little-Endian ARM64)

**File: `cmake/Toolchain-Linux-ARM64-clang.cmake`**

```cmake
# Linux ARM64 (AArch64, little-endian) with Clang
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)

# ARM64 architecture flags
set(CMAKE_C_FLAGS "-march=armv8-a" CACHE STRING "")

# Endianness
set(SAFEAPI_ENDIAN LITTLE CACHE STRING "System endianness")
add_compile_definitions(SAFEAPI_LITTLE_ENDIAN)

# Backend
add_compile_definitions(SAFEAPI_BACKEND_POSIX)
```

### 4.3 Linux with GCC (Big-Endian ARM32)

**File: `cmake/Toolchain-Linux-ARM32BE-gcc.cmake`**

```cmake
# Linux ARM32 big-endian (ARMv7-BE) with GCC
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7-be)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# ARM32 big-endian flags
set(CMAKE_C_FLAGS "-march=armv7-a -mbig-endian -mfpu=neon" CACHE STRING "")

# Endianness (CRITICAL for railway systems)
set(SAFEAPI_ENDIAN BIG CACHE STRING "System endianness")
add_compile_definitions(SAFEAPI_BIG_ENDIAN)

# Backend
add_compile_definitions(SAFEAPI_BACKEND_POSIX)
```

### 4.4 QNX RTOS (x86_64)

**File: `cmake/Toolchain-QNX-x86_64.cmake`**

```cmake
# QNX Neutrino RTOS x86_64 (SIL 3/4 railway system)
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# QNX environment variables required
if(NOT DEFINED ENV{QNX_HOST})
    message(FATAL_ERROR "QNX_HOST environment variable not set")
endif()
if(NOT DEFINED ENV{QNX_TARGET})
    message(FATAL_ERROR "QNX_TARGET environment variable not set")
endif()

# QCC compiler (GCC-based, QNX-specific)
set(CMAKE_C_COMPILER $ENV{QNX_HOST}/usr/bin/qcc)
set(CMAKE_CXX_COMPILER $ENV{QNX_HOST}/usr/bin/qcc)

# QNX-specific settings
set(CMAKE_FIND_ROOT_PATH $ENV{QNX_TARGET})
set(SAFEAPI_ENDIAN LITTLE CACHE STRING "System endianness")
add_compile_definitions(SAFEAPI_BACKEND_QNX)
add_compile_definitions(SAFEAPI_LITTLE_ENDIAN)

# SIL 3/4 compliance
add_compile_definitions(SAFEAPI_SIL_34=1)
```

### 4.5 QNX RTOS (PowerPC32, Big-Endian - Railway)

**File: `cmake/Toolchain-QNX-PowerPC32.cmake`**

```cmake
# QNX Neutrino RTOS PowerPC32 big-endian (SIL 3/4 railway system)
# Common in ERTMS and railway signaling systems
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR ppc)

# QNX environment
if(NOT DEFINED ENV{QNX_HOST})
    message(FATAL_ERROR "QNX_HOST environment variable not set")
endif()
if(NOT DEFINED ENV{QNX_TARGET})
    message(FATAL_ERROR "QNX_TARGET environment variable not set")
endif()

# PowerPC compiler from QNX
set(CMAKE_C_COMPILER $ENV{QNX_HOST}/usr/bin/qcc)
set(CMAKE_CXX_COMPILER $ENV{QNX_HOST}/usr/bin/qcc)

# PowerPC32 big-endian (critical for railway systems!)
set(CMAKE_C_FLAGS "-Vgcc_notarmle -march=ppc" CACHE STRING "")
set(SAFEAPI_ENDIAN BIG CACHE STRING "System endianness")
add_compile_definitions(SAFEAPI_BACKEND_QNX)
add_compile_definitions(SAFEAPI_BIG_ENDIAN)
add_compile_definitions(SAFEAPI_SIL_34=1)

# Railway-specific: strict memory alignment
add_compile_definitions(SAFEAPI_STRICT_ALIGNMENT)
```

### 4.6 VxWorks (PowerPC64, Big-Endian - Stub)

**File: `cmake/Toolchain-VxWorks-PPC64.cmake`**

```cmake
# VxWorks RTOS PowerPC64 big-endian (SIL 3/4 railway system)
# NOTE: This is a template/stub. Requires WindRiver VxWorks SDK.
set(CMAKE_SYSTEM_NAME VxWorks)
set(CMAKE_SYSTEM_PROCESSOR ppc64)

# WindRiver VxWorks compiler (requires installation)
# set(CMAKE_C_COMPILER $ENV{WIND_BASE}/gnu/4.8.5-vxworks-7.x/bin/ccppc)

set(SAFEAPI_ENDIAN BIG CACHE STRING "System endianness")
add_compile_definitions(SAFEAPI_BACKEND_VXWORKS)
add_compile_definitions(SAFEAPI_BIG_ENDIAN)
add_compile_definitions(SAFEAPI_SIL_34=1)

# VxWorks-specific settings
add_compile_definitions(_VX_CPU=PPC64)
```

---

## 5. Updated CMakePresets.json

### Linux Presets (Multiple Compilers & Architectures)

```json
{
  "configurePresets": [
    {
      "name": "linux-x86_64-gcc",
      "displayName": "Linux x86_64 (GCC)",
      "description": "Linux POSIX backend, x86_64 architecture, GCC compiler",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-Linux-x86_64-gcc.cmake"
      }
    },
    {
      "name": "linux-x86_64-clang",
      "displayName": "Linux x86_64 (Clang)",
      "description": "Linux POSIX backend, x86_64 architecture, Clang compiler",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-Linux-x86_64-clang.cmake"
      }
    },
    {
      "name": "linux-arm64-gcc",
      "displayName": "Linux ARM64 (GCC)",
      "description": "Linux ARM64 little-endian, GCC cross-compiler",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-Linux-ARM64-gcc.cmake"
      }
    },
    {
      "name": "linux-arm64-clang",
      "displayName": "Linux ARM64 (Clang)",
      "description": "Linux ARM64 little-endian, Clang cross-compiler",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-Linux-ARM64-clang.cmake"
      }
    },
    {
      "name": "linux-arm32-gcc",
      "displayName": "Linux ARM32 LE (GCC)",
      "description": "Linux ARM32 little-endian, GCC cross-compiler",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-Linux-ARM32-gcc.cmake"
      }
    },
    {
      "name": "linux-arm32be-gcc",
      "displayName": "Linux ARM32 BE (GCC)",
      "description": "Linux ARM32 big-endian (railway systems), GCC cross-compiler",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-Linux-ARM32BE-gcc.cmake"
      }
    }
  ]
}
```

### QNX Presets (Multiple Architectures)

```json
{
  "configurePresets": [
    {
      "name": "qnx-x86_64",
      "displayName": "QNX x86_64",
      "description": "QNX Neutrino RTOS, x86_64 architecture (SIL 3/4)",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-QNX-x86_64.cmake",
        "SAFEAPI_BUILD_TESTS": "OFF"
      },
      "environment": {
        "QNX_HOST": "${env:QNX_HOST}",
        "QNX_TARGET": "${env:QNX_TARGET}"
      }
    },
    {
      "name": "qnx-arm64",
      "displayName": "QNX ARM64",
      "description": "QNX ARM64 little-endian (SIL 3/4 railway)",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-QNX-ARM64.cmake",
        "SAFEAPI_BUILD_TESTS": "OFF"
      },
      "environment": {
        "QNX_HOST": "${env:QNX_HOST}",
        "QNX_TARGET": "${env:QNX_TARGET}"
      }
    },
    {
      "name": "qnx-arm32",
      "displayName": "QNX ARM32 LE",
      "description": "QNX ARM32 little-endian (SIL 3/4 railway)",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-QNX-ARM32.cmake",
        "SAFEAPI_BUILD_TESTS": "OFF"
      },
      "environment": {
        "QNX_HOST": "${env:QNX_HOST}",
        "QNX_TARGET": "${env:QNX_TARGET}"
      }
    },
    {
      "name": "qnx-powerpc32",
      "displayName": "QNX PowerPC32 BE",
      "description": "QNX PowerPC32 big-endian (railway ERTMS systems, SIL 3/4)",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-QNX-PowerPC32.cmake",
        "SAFEAPI_BUILD_TESTS": "OFF"
      },
      "environment": {
        "QNX_HOST": "${env:QNX_HOST}",
        "QNX_TARGET": "${env:QNX_TARGET}"
      }
    }
  ]
}
```

### FreeRTOS Presets (Embedded Systems)

```json
{
  "configurePresets": [
    {
      "name": "freertos-arm32",
      "displayName": "FreeRTOS ARM32",
      "description": "FreeRTOS on ARM32 microcontroller (embedded railway device)",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-FreeRTOS-ARM32.cmake",
        "SAFEAPI_BUILD_TESTS": "OFF"
      }
    },
    {
      "name": "freertos-arm64",
      "displayName": "FreeRTOS ARM64",
      "description": "FreeRTOS on ARM64 (embedded railway system)",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-FreeRTOS-ARM64.cmake",
        "SAFEAPI_BUILD_TESTS": "OFF"
      }
    }
  ]
}
```

---

## 6. Building for Different Platforms

### Build for Linux x86_64 (Development)
```bash
cmake --preset linux-x86_64-gcc
cmake --build --preset linux-x86_64-gcc
ctest --preset linux-x86_64-gcc
```

### Build for Linux ARM64 (Cross-compile, e.g., Raspberry Pi 64-bit)
```bash
# Requires: arm-linux-gnueabihf-gcc
cmake --preset linux-arm64-gcc
cmake --build --preset linux-arm64-gcc
```

### Build for QNX PowerPC32 Big-Endian (Railway ERTMS System)
```bash
# Requires: QNX SDK installed
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/ppc

cmake --preset qnx-powerpc32
cmake --build --preset qnx-powerpc32
```

### Build for FreeRTOS ARM32 (Embedded Device)
```bash
# Requires: ARM GCC toolchain
cmake --preset freertos-arm32
cmake --build --preset freertos-arm32
```

---

## 7. Endianness Handling

### 7.1 Compile-Time Detection

All toolchain files define endianness at compile time:

```cmake
# Little-endian (most common)
add_compile_definitions(SAFEAPI_LITTLE_ENDIAN)

# Big-endian (PowerPC, some ARM, MIPS)
add_compile_definitions(SAFEAPI_BIG_ENDIAN)
```

### 7.2 Code Usage

**In safeAPIFramework code:**

```c
#if defined(SAFEAPI_LITTLE_ENDIAN)
    // Little-endian optimizations
    uint32_t value = buffer[0] | (buffer[1] << 8) | ...;
#elif defined(SAFEAPI_BIG_ENDIAN)
    // Big-endian optimizations
    uint32_t value = (buffer[0] << 24) | (buffer[1] << 16) | ...;
#endif
```

### 7.3 Buffer Operations (ADR-002)

The `sapi_buffer` API handles endianness transparently:

```c
// Works correctly regardless of system endianness
sapi_status_t sapi_buffer_read_uint32(sapi_buffer_t buf, uint32_t *out);
sapi_status_t sapi_buffer_write_uint32(sapi_buffer_t buf, uint32_t value);
```

---

## 8. Architecture-Specific Compilation

### 8.1 Architecture Detection

**File: `cmake/CMakeLists-Arch.cmake`**

```cmake
# Detect architecture from toolchain file
if(DEFINED CMAKE_SYSTEM_PROCESSOR)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|x86_64-*")
        set(SAFEAPI_ARCH "x86_64")
        add_compile_definitions(SAFEAPI_ARCH_X86_64=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARMv8-A")
        set(SAFEAPI_ARCH "arm64")
        add_compile_definitions(SAFEAPI_ARCH_ARM64=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armv7l|armv7-a")
        set(SAFEAPI_ARCH "arm32")
        add_compile_definitions(SAFEAPI_ARCH_ARM32=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc|powerpc|PPC64")
        set(SAFEAPI_ARCH "powerpc")
        add_compile_definitions(SAFEAPI_ARCH_PPC=1)
    endif()
endif()

message(STATUS "Detected architecture: ${SAFEAPI_ARCH}")
```

### 8.2 Memory Alignment (Critical for Big-Endian Systems)

```c
// Some railway systems require strict alignment
#if defined(SAFEAPI_STRICT_ALIGNMENT)
    // Enforce padding for structure members
    typedef struct {
        uint8_t flag;
        uint8_t __pad1;        // Alignment padding
        uint16_t length;
    } sapi_message_header_t;
#endif
```

---

## 9. Compiler-Specific Settings

### 9.1 GCC Specific

```cmake
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # GCC-specific flags for railway systems
    add_compile_options(-fno-strict-aliasing)    # Safer for embedded systems
    add_compile_options(-fno-common)             # Stricter linking
    add_compile_options(-fstack-protector-all)   # Stack canaries
endif()
```

### 9.2 Clang Specific

```cmake
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    # Clang-specific flags
    add_compile_options(-fno-builtin-malloc)    # Use our malloc (if needed)
    add_compile_options(-Wunused-parameter)     # Stricter unused parameter detection
endif()
```

### 9.3 IAR Embedded Workbench (Commercial, SIL 3/4)

```cmake
if(CMAKE_C_COMPILER_ID STREQUAL "IAR")
    # IAR-specific settings
    add_compile_options(--c99)                  # C99 standard
    add_compile_options(--enable_restrict)      # Restrict keyword
    add_compile_options(--no_cse)               # Disable common subexpression elimination
endif()
```

---

## 10. Testing Matrix

### 10.1 CI/CD Test Matrix

GitHub Actions workflow to test all supported combinations:

```yaml
# .github/workflows/multi-platform-tests.yml
jobs:
  build-matrix:
    strategy:
      matrix:
        os: [linux, qnx]
        arch: [x86_64, arm64, arm32, ppc32]
        compiler: [gcc, clang]
        exclude:
          # Clang not available for QNX/PPC
          - os: qnx
            compiler: clang
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          cmake --preset ${{ matrix.os }}-${{ matrix.arch }}-${{ matrix.compiler }}
          cmake --build --preset ${{ matrix.os }}-${{ matrix.arch }}-${{ matrix.compiler }}
```

### 10.2 Cross-Compiler Test Scripts

**scripts/test-all-platforms.sh:**

```bash
#!/bin/bash
# Test all supported toolchain combinations

echo "Testing Linux platforms..."
for preset in linux-x86_64-gcc linux-x86_64-clang linux-arm64-gcc linux-arm32-gcc; do
    echo "Testing $preset..."
    cmake --preset $preset || exit 1
    cmake --build --preset $preset || exit 1
    ctest --preset $preset || exit 1
done

echo "Testing QNX platforms (requires QNX SDK)..."
if [ -n "$QNX_HOST" ] && [ -n "$QNX_TARGET" ]; then
    for preset in qnx-x86_64 qnx-arm64 qnx-powerpc32; do
        echo "Testing $preset..."
        cmake --preset $preset || exit 1
        cmake --build --preset $preset || exit 1
    done
fi

echo "All platform tests completed!"
```

---

## 11. Documentation per Platform

For each supported platform:

| Platform | Doc | Compiler | Arch | Endian | Status |
|----------|-----|----------|------|--------|--------|
| Linux x86_64 | [build-linux.md](../examples/build-linux.md) | GCC/Clang | x86_64 | LE | ✅ |
| Linux ARM64 | [build-arm64.md](../examples/build-arm64.md) | GCC/Clang | ARM64 | LE | ✅ |
| Linux ARM32 | [build-arm32.md](../examples/build-arm32.md) | GCC | ARM32 | LE | ✅ |
| QNX x86_64 | [build-qnx-x86.md](../examples/build-qnx-x86.md) | QCC | x86_64 | LE | ✅ |
| QNX ARM64 | [build-qnx-arm64.md](../examples/build-qnx-arm64.md) | QCC | ARM64 | LE | ✅ |
| QNX PowerPC32 | [build-qnx-ppc.md](../examples/build-qnx-ppc.md) | QCC | PPC32 | BE | ✅ |
| VxWorks PPC64 | [build-vxworks.md](../examples/build-vxworks.md) | WindRiver | PPC64 | BE | ⏳ |
| FreeRTOS ARM32 | [build-freertos.md](../examples/build-freertos.md) | GCC | ARM32 | LE | ⏳ |

---

## 12. Adding New Platforms

### 12.1 Checklist

To add support for a new platform (e.g., VxWorks ARM64):

1. **Create toolchain file:**
   ```bash
   cp cmake/Toolchain-VxWorks-PPC64.cmake \
      cmake/Toolchain-VxWorks-ARM64.cmake
   # Edit with ARM64-specific settings
   ```

2. **Add CMake preset:**
   ```json
   {
     "name": "vxworks-arm64",
     "displayName": "VxWorks ARM64",
     "cacheVariables": {
       "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/Toolchain-VxWorks-ARM64.cmake"
     }
   }
   ```

3. **Test locally:**
   ```bash
   cmake --preset vxworks-arm64
   cmake --build --preset vxworks-arm64
   ```

4. **Add CI/CD test:**
   - Update `.github/workflows/multi-platform-tests.yml`
   - Add preset to test matrix

5. **Document:**
   - Create `examples/build-vxworks-arm64.md`
   - Update this document's matrix table

---

## 13. Railway-Specific Considerations

### 13.1 Big-Endian Systems (Common in Railway)

Many railway ERTMS systems use **PowerPC big-endian**:

```cmake
# Toolchain: Toolchain-QNX-PowerPC32.cmake
set(SAFEAPI_ENDIAN BIG)
add_compile_definitions(SAFEAPI_BIG_ENDIAN)
add_compile_definitions(SAFEAPI_STRICT_ALIGNMENT)  # Railway systems require alignment
```

### 13.2 Hardware Abstraction

Backend code (in `backends/<rtos>/`) handles:
- Timer HAL (system clocks may differ)
- Memory management (different NVM layouts)
- Task scheduling (different APIs)
- IPC protocols (different synchronization primitives)

**Example: Timer backend for different CPUs**

```c
// backends/qnx/src/sapi_timer_qnx.c
sapi_status_t sapi_timer_create_impl(const char *name, sapi_timer_handle_t *out) {
    // QNX timer implementation
    #if defined(SAFEAPI_ARCH_PPC)
        // PowerPC-specific: use decrementer timer
        timer_config.source = TIMER_SOURCE_DEC;
    #elif defined(SAFEAPI_ARCH_ARM64)
        // ARM64-specific: use generic timer
        timer_config.source = TIMER_SOURCE_ARM_GENERIC;
    #endif
    return qnx_timer_create(&timer_config, out);
}
```

---

## 14. Compliance Matrix

### MISRA C:2012 Compliance per Platform

All platforms compile with **zero MISRA Mandatory violations**:

```bash
# Test MISRA compliance for each platform
for preset in linux-x86_64-gcc linux-arm64-gcc qnx-powerpc32; do
    echo "Checking MISRA for $preset..."
    cmake --preset $preset
    ./scripts/run-cppcheck.sh
done
```

---

## 15. Roadmap

### Phase 1 (2026-08) ✅ DONE
- [x] Linux x86_64 (GCC, Clang)
- [x] Linux ARM64 (GCC, Clang)
- [x] Linux ARM32 LE (GCC)
- [x] QNX x86_64
- [x] QNX ARM32/ARM64
- [x] Endianness support (LE/BE)

### Phase 2 (2026-10) 🔄 IN PROGRESS
- [ ] Linux ARM32 BE (big-endian)
- [ ] QNX PowerPC32 BE (railway ERTMS)
- [ ] FreeRTOS ARM32/ARM64
- [ ] Clang support for all platforms

### Phase 3 (2026-12) 📅 PLANNED
- [ ] VxWorks PowerPC32/64 (full support)
- [ ] INTEGRITY RTOS support
- [ ] IAR Embedded Workbench support
- [ ] DIAB (WindRiver) support

### Phase 4 (2027-06) 🎯 LONG-TERM
- [ ] MIPS (railway TCS systems)
- [ ] Bare-metal support
- [ ] Custom architecture support (customer-specific)

---

## References

- CMakePresets.json (updated with multi-platform presets)
- cmake/Toolchain-*.cmake (platform-specific files)
- scripts/test-all-platforms.sh (comprehensive testing)
- .github/workflows/multi-platform-tests.yml (CI/CD matrix)
- MISRA_COMPLIANCE_REPORT.md (compliance per platform)

