# Integrating safeAPIFramework into Your Project

This guide shows how to use safeAPIFramework as a dependency in your own C/embedded project, and how to reuse its CMake configurations and tools.

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
# Build safeAPIFramework
cd safeAPIFramework
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Install to system (default: /usr/local)
sudo cmake --install build

# Or install to custom location
cmake --install build --prefix ~/opt/safeAPIFramework
```

### Option B: Local/Embedded Install (recommended for development)

Include safeAPIFramework as a subdirectory or Git submodule:

```sh
# In your project root
git submodule add https://github.com/user/safeAPIFramework.git deps/safeAPIFramework
```

Then in your `CMakeLists.txt`:
```cmake
add_subdirectory(deps/safeAPIFramework)
```

---

## Using as a CMake Dependency

### Minimal Integration

In your **CMakeLists.txt**:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp)

# Find safeAPIFramework
find_package(safeAPIFramework 0.1.0 REQUIRED)

# Create your executable
add_executable(myapp main.c other.c)

# Link against safeAPIFramework components you need
target_link_libraries(myapp
    safeapi::timer
    safeapi::nvm
    safeapi::status
)

# Include headers
target_include_directories(myapp PRIVATE ${safeAPIFramework_INCLUDE_DIR})
```

### Linking Specific Modules

safeAPIFramework exports these components:
- `safeapi::status` — Common error codes
- `safeapi::types` — Fixed-width type definitions
- `safeapi::buffer` — Endianness-safe buffer operations
- `safeapi::cast` — Checked integer casting
- `safeapi::safestate` — Safe-state transitions
- `safeapi::string` — Bounded string operations
- `safeapi::timer` — Timer services
- `safeapi::nvm` — Non-volatile memory
- `safeapi::memory` — Memory management
- `safeapi::task` — Task scheduling
- `safeapi::ipc` — Inter-process communication
- `safeapi::log` — Logging
- `safeapi::reboot` — Controlled reboot

Example: use only what you need
```cmake
target_link_libraries(myapp safeapi::timer safeapi::log safeapi::status)
```

---

## Reusing CMake Modules

safeAPIFramework includes reusable CMake modules. After `find_package()`, you can use them:

### CompilerWarnings.cmake

Applies strict compiler warnings matching safeAPIFramework standards:

```cmake
find_package(safeAPIFramework REQUIRED)
include(CompilerWarnings)

add_executable(myapp main.c)
apply_compiler_warnings(myapp)  # Adds -Wall -Wextra -Wpedantic -Werror
```

### SafeAPIHelpers.cmake

Provides helper functions:

```cmake
find_package(safeAPIFramework REQUIRED)
include(SafeAPIHelpers)

# Enable MISRA C:2012 checking via cppcheck
sapi_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)

# Apply strict warnings
sapi_apply_strict_warnings(TARGET myapp)

# Verify conventions
sapi_verify_conventions()

# Generate CMakePresets.json template
sapi_generate_presets_template("${CMAKE_SOURCE_DIR}/CMakePresets.json")
```

---

## Reusing Configurations

### Option A: CMake Presets Template

Generate a template `CMakePresets.json` for your project:

```cmake
find_package(safeAPIFramework REQUIRED)
include(SafeAPIHelpers)

sapi_generate_presets_template("${CMAKE_SOURCE_DIR}/CMakePresets.json")
```

Then use presets:
```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

### Option B: Manual Preset Setup

Copy preset configurations from safeAPIFramework's `CMakePresets.json` and adapt them to your project.

### Option C: Submodule Approach

If you embedded safeAPIFramework as `deps/safeAPIFramework/`, you can reference its presets in your own `CMakePresets.json`:

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

Here's a complete example that uses safeAPIFramework:

### Project structure:
```
myproject/
├── CMakeLists.txt
├── CMakePresets.json
├── src/
│   ├── main.c
│   └── app.c
└── deps/
    └── safeAPIFramework/  (git submodule)
```

### CMakeLists.txt:

```cmake
cmake_minimum_required(VERSION 3.16)
project(myproject
    VERSION 1.0.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include safeAPIFramework as subdirectory
add_subdirectory(deps/safeAPIFramework)

# Load safeAPIFramework's CMake modules
include(SafeAPIHelpers)
include(CompilerWarnings)

# Create executable
add_executable(myapp
    src/main.c
    src/app.c
)

# Link safeAPIFramework components
target_link_libraries(myapp
    safeapi::timer
    safeapi::log
    safeapi::status
)

# Include safeAPIFramework headers
target_include_directories(myapp PRIVATE deps/safeAPIFramework/include)

# Apply strict warnings
sapi_apply_strict_warnings(TARGET myapp)

# Enable MISRA analysis
sapi_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)

# Verify conventions
sapi_verify_conventions()
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

## Finding safeAPIFramework

If you installed to a non-standard location:

```bash
# Export CMAKE_PREFIX_PATH so find_package() can locate it
export CMAKE_PREFIX_PATH=~/opt/safeAPIFramework:$CMAKE_PREFIX_PATH

cmake --preset debug
```

Or pass it to CMake:
```bash
cmake --preset debug -DCMAKE_PREFIX_PATH=~/opt/safeAPIFramework
```

---

## Troubleshooting

### `find_package(safeAPIFramework) not found`

1. Check installation: `ls $(CMAKE_PREFIX_PATH)/lib/cmake/safeAPIFramework`
2. Ensure `safeAPIFrameworkConfig.cmake` is present
3. Check `CMAKE_PREFIX_PATH` environment variable
4. Try: `cmake --trace-expand | grep -i safeapi`

### Linking issues with specific components

Ensure you link the correct `safeapi::` target. Check available targets:
```cmake
find_package(safeAPIFramework REQUIRED)
message(STATUS "safeAPIFramework libraries: ${safeAPIFramework_LIBRARIES}")
```

### MISRA violations in downstream project

Suppress specific rules via `.cppcheck-suppressions`:
```
# In your project's .cppcheck-suppressions file
misra-c2012-15.5  # If using guard clauses like safeAPIFramework
```

---

## Next Steps

- Read [CLAUDE.md](../CLAUDE.md) for coding standards
- Review [docs/architecture/](../docs/architecture/) for design patterns
- Check [docs/MISRA_COMPLIANCE_REPORT.md](../docs/MISRA_COMPLIANCE_REPORT.md) for compliance details
