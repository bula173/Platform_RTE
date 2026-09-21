# Integrating RteFramework into Your Project

This guide shows how to use RteFramework as a dependency in your own C/embedded project, and how to reuse its CMake configurations and tools.

## Table of Contents

1. [Installation](#installation)
2. [Using as a CMake Dependency](#using-as-a-cmake-dependency)
3. [Reusing CMake Modules](#reusing-cmake-modules)
4. [Reusing Configurations](#reusing-configurations)
5. [Example Project](#example-project)

---

## Installation

### Option A: System-wide Install (recommended for packaged deployments)

```sh
# Build RteFramework
cd RteFramework
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Install to system (default: /usr/local)
sudo cmake --install build

# Or install to custom location
cmake --install build --prefix ~/opt/RteFramework
```

### Option B: Local/Embedded Install (recommended for development)

Include RteFramework as a subdirectory or Git submodule:

```sh
# In your project root
git submodule add https://github.com/user/RteFramework.git deps/RteFramework
```

Then in your `CMakeLists.txt`:
```cmake
add_subdirectory(deps/RteFramework)
```

---

## Using as a CMake Dependency

### Minimal Integration

In your **CMakeLists.txt**:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp)

# Find RteFramework
find_package(RteFramework 0.1.0 REQUIRED)

# Create your executable
add_executable(myapp main.c other.c)

# Link against the RteFramework libraries you need
target_link_libraries(myapp
    rte::core
    rte::oal
)

# Include headers
target_include_directories(myapp PRIVATE ${RteFramework_INCLUDE_DIR})
```

### Linking Specific Modules

RteFramework exports 4 libraries (ADR-023 - previously one per
feature; consolidated because no consumer ever linked a single feature
in isolation):

- `rte::core` — zero-OS-dependency primitives: status codes, fixed-width
  types, endianness-safe buffers, checked integer casting, safe-state
  transitions, bounded string operations
- `rte::oal` — OS Abstraction Layer services: timer, non-volatile
  memory, static memory reservation, task scheduling, inter-process
  communication, network links, logging, controlled reboot, watchdog
  (depends on `rte::core`)
- `rte::channels` — safety-comms/channel layer: CRC-64 checksums,
  voting channels, clock sync, checkpoint rendezvous, dual-transfer
  redundant links, the unified `rte_safechannel` factory (depends on
  `rte::core` and `rte::oal`)
- `rte::appmanager` — application lifecycle hooks and built-in
  checkpoint integration (depends on all three above)

Each library's `target_link_libraries()` is `PUBLIC`, so linking one
name pulls in everything it depends on transitively - e.g. linking just
`rte::channels` is enough to also get `core` and `oal` symbols.

Example: use only what you need
```cmake
target_link_libraries(myapp rte::oal)
```

---

## Reusing CMake Modules

RteFramework includes reusable CMake modules. After `find_package()`, you can use them:

### CompilerWarnings.cmake

Applies strict compiler warnings matching RteFramework standards:

```cmake
find_package(RteFramework REQUIRED)
include(CompilerWarnings)

add_executable(myapp main.c)
apply_compiler_warnings(myapp)  # Adds -Wall -Wextra -Wpedantic -Werror
```

### RTEHelpers.cmake

Provides helper functions:

```cmake
find_package(RteFramework REQUIRED)
include(RTEHelpers)

# Enable MISRA C:2012 checking via cppcheck
rte_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)

# Apply strict warnings
rte_apply_strict_warnings(TARGET myapp)

# Verify conventions
rte_verify_conventions()

# Generate CMakePresets.json template
rte_generate_presets_template("${CMAKE_SOURCE_DIR}/CMakePresets.json")
```

---

## Reusing Configurations

### Option A: CMake Presets Template

Generate a template `CMakePresets.json` for your project:

```cmake
find_package(RteFramework REQUIRED)
include(RTEHelpers)

rte_generate_presets_template("${CMAKE_SOURCE_DIR}/CMakePresets.json")
```

Then use presets:
```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

### Option B: Manual Preset Setup

Copy preset configurations from RteFramework's `CMakePresets.json` and adapt them to your project.

### Option C: Submodule Approach

If you embedded RteFramework as `deps/RteFramework/`, you can reference its presets in your own `CMakePresets.json`:

```json
{
  "configurePresets": [
    {
      "name": "debug",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    }
  ]
}
```

---

## Example Project

Here's a complete example that uses RteFramework:

### Project structure:
```
myproject/
├── CMakeLists.txt
├── CMakePresets.json
├── src/
│   ├── main.c
│   └── app.c
└── deps/
    └── RteFramework/  (git submodule)
```

### CMakeLists.txt:

```cmake
cmake_minimum_required(VERSION 3.16)
project(myproject
    VERSION 1.0.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include RteFramework as subdirectory
add_subdirectory(deps/RteFramework)

# Load RteFramework's CMake modules
include(RTEHelpers)
include(CompilerWarnings)

# Create executable
add_executable(myapp
    src/main.c
    src/app.c
)

# Link RteFramework components
target_link_libraries(myapp
    rte::core
    rte::oal
)

# Include RteFramework headers
target_include_directories(myapp PRIVATE deps/RteFramework/include)

# Apply strict warnings
rte_apply_strict_warnings(TARGET myapp)

# Enable MISRA analysis
rte_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)

# Verify conventions
rte_verify_conventions()
```

### CMakePresets.json (minimal):

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "debug",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {"CMAKE_BUILD_TYPE": "Debug"}
    }
  ]
}
```

### Build and test:

```sh
cmake --preset debug
cmake --build --preset debug
```

---

## Finding RteFramework

If you installed to a non-standard location:

```bash
# Export CMAKE_PREFIX_PATH so find_package() can locate it
export CMAKE_PREFIX_PATH=~/opt/RteFramework:$CMAKE_PREFIX_PATH

cmake --preset debug
```

Or pass it to CMake:
```bash
cmake --preset debug -DCMAKE_PREFIX_PATH=~/opt/RteFramework
```

---

## Troubleshooting

### `find_package(RteFramework) not found`

1. Check installation: `ls $(CMAKE_PREFIX_PATH)/lib/cmake/RteFramework`
2. Ensure `RteFrameworkConfig.cmake` is present
3. Check `CMAKE_PREFIX_PATH` environment variable
4. Try: `cmake --trace-expand | grep -i rte`

### Linking issues with specific components

Ensure you link the correct `rte::` target. Check available targets:
```cmake
find_package(RteFramework REQUIRED)
message(STATUS "RteFramework libraries: ${RteFramework_LIBRARIES}")
```

### MISRA violations in downstream project

Suppress specific rules via `.cppcheck-suppressions`:
```
# In your project's .cppcheck-suppressions file
misra-c2012-15.5  # If using guard clauses like RteFramework
```

---

## Next Steps

- Read [CLAUDE.md](../CLAUDE.md) for coding standards
- Review [docs/architecture/](../docs/architecture/) for design patterns
- Check [docs/MISRA_COMPLIANCE_REPORT.md](../docs/MISRA_COMPLIANCE_REPORT.md) for compliance details
