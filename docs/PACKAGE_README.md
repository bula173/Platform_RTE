# safeAPIFramework as a Reusable Package

safeAPIFramework is now packaged and distributable as a CMake package with support for both **direct dependency management** and **reusable CMake modules**.

## Quick Start

### For Projects That Link safeAPIFramework

```bash
# Install safeAPIFramework
cd safeAPIFramework
cmake --preset debug
cmake --build --preset debug
cmake --install build --prefix ~/.local/safeapi

# In your project CMakeLists.txt
find_package(safeAPIFramework REQUIRED)
target_include_directories(myapp ${safeAPIFramework_INCLUDE_DIR})
target_link_libraries(myapp safeapi_timer safeapi_log)
```

### For Projects That Reuse CMake Modules Only

Even without linking safeAPIFramework, you can reuse its CMake helpers:

```bash
# Install just the CMake modules (already done via cmake --install)
# Then in your CMakeLists.txt:

find_package(safeAPIFramework REQUIRED)  # Finds and loads modules

include(SafeAPIHelpers)           # Helper functions for cppcheck, warnings, etc.
include(CompilerWarnings)         # Strict warning flags

# Now use the functions
sapi_enable_cppcheck_misra(TARGET myapp)
sapi_apply_strict_warnings(TARGET myapp)
```

---

## What's Installed

```
~/.local/safeapi/
├── include/safeapi/              # All public headers
│   ├── types/
│   ├── status/
│   ├── buffer/
│   ├── cast/
│   ├── safestate/
│   ├── string/
│   ├── timer/
│   ├── nvm/
│   ├── memory/
│   ├── task/
│   ├── ipc/
│   ├── log/
│   └── reboot/
├── lib/
│   ├── libsafeapi_*.a            # 12 static libraries (types is header-only)
│   └── cmake/safeAPIFramework/
│       ├── safeAPIFrameworkConfig.cmake         # find_package support
│       ├── safeAPIFrameworkConfigVersion.cmake  # Version checking
│       ├── CompilerWarnings.cmake               # Warning flags
│       └── SafeAPIHelpers.cmake                 # Helper functions
```

---

## Finding the Package

If installed to a non-standard location, tell CMake where to find it:

```bash
export CMAKE_PREFIX_PATH=~/.local/safeapi:$CMAKE_PREFIX_PATH
cmake --preset debug
```

Or pass it inline:
```bash
cmake --preset debug -DCMAKE_PREFIX_PATH=~/.local/safeapi
```

---

## Available Helper Functions (SafeAPIHelpers.cmake)

After `find_package()` and `include(SafeAPIHelpers)`:

### sapi_enable_cppcheck_misra(TARGET <name> [SUPPRESS_RULE_15_5])
Enables MISRA C:2012 checking via cppcheck for a target.

```cmake
sapi_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)
```

### sapi_apply_strict_warnings(TARGET <name>)
Applies safeAPIFramework's strict warning flags (-Wall -Wextra -Wpedantic -Werror).

```cmake
sapi_apply_strict_warnings(TARGET myapp)
```

### sapi_verify_conventions()
Validates that the project follows safeAPIFramework conventions (C99, no malloc, etc.).

```cmake
sapi_verify_conventions()
```

### sapi_generate_presets_template(OUTPUT_FILE)
Generates a CMakePresets.json template for your project.

```cmake
sapi_generate_presets_template("${CMAKE_SOURCE_DIR}/CMakePresets.json")
```

---

## Example: Downstream Project

See [docs/INTEGRATION.md](INTEGRATION.md) for a complete working example with source code.

**Key integration points:**

1. **Find the package:**
   ```cmake
   find_package(safeAPIFramework 0.1.0 REQUIRED)
   ```

2. **Use paths:**
   ```cmake
   target_include_directories(myapp PRIVATE ${safeAPIFramework_INCLUDE_DIR})
   ```

3. **Link libraries (if using safeAPIFramework services):**
   ```cmake
   target_link_libraries(myapp safeapi_timer safeapi_log)
   ```

4. **Reuse modules:**
   ```cmake
   include(SafeAPIHelpers)
   sapi_apply_strict_warnings(TARGET myapp)
   sapi_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)
   ```

---

## Two Integration Patterns

### Pattern A: Submodule / Embedded
```cmake
add_subdirectory(deps/safeAPIFramework)
# Targets available as safeapi_* directly
target_link_libraries(myapp safeapi_timer)
```

### Pattern B: Installed Package
```cmake
find_package(safeAPIFramework REQUIRED)
# Use variables + manual linking
target_include_directories(myapp ${safeAPIFramework_INCLUDE_DIR})
target_link_libraries(myapp ${safeAPIFramework_LIB_DIR}/libsafeapi_timer.a)
```

Or use a link wrapper (recommended in downstream CMakeLists):
```cmake
# Create your own namespace targets wrapping safeAPIFramework
add_library(mylibs::sapi_timer STATIC IMPORTED)
set_target_properties(mylibs::sapi_timer PROPERTIES
    IMPORTED_LOCATION "${safeAPIFramework_LIB_DIR}/libsafeapi_timer.a"
    INTERFACE_INCLUDE_DIRECTORIES "${safeAPIFramework_INCLUDE_DIR}"
)
target_link_libraries(myapp mylibs::sapi_timer)
```

---

## Distribution Options

### 1. System Package Manager
```bash
# Install to system prefix (requires sudo)
sudo cmake --install build --prefix /usr/local
# Now all projects can use: find_package(safeAPIFramework)
```

### 2. User/Project Directory
```bash
# Install to user home
cmake --install build --prefix ~/.local/safeapi
# Set CMAKE_PREFIX_PATH before building downstream projects
```

### 3. Monorepo / Git Submodule
```bash
# In your main project
git submodule add https://... deps/safeAPIFramework
# In your CMakeLists.txt
add_subdirectory(deps/safeAPIFramework)
```

### 4. Package Manager (vcpkg, Conan, etc.)
The CMakeLists.txt is compatible with standard package managers. Contributed recipes welcome.

---

## Version Checking

safeAPIFramework provides version information:

```cmake
find_package(safeAPIFramework 0.1.0 REQUIRED)
message("Using safeAPIFramework ${safeAPIFramework_FOUND}")
```

Version is defined in CMakeLists.txt:
```cmake
project(safeAPIFramework VERSION 0.1.0)
```

---

## Questions?

See [docs/INTEGRATION.md](INTEGRATION.md) for detailed integration examples, or [CLAUDE.md](../CLAUDE.md) for coding standards and safety practices.
