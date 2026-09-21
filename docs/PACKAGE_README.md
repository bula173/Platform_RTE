# RteFramework as a Reusable Package

RteFramework is now packaged and distributable as a CMake package with support for both **direct dependency management** and **reusable CMake modules**.

## Quick Start

### For Projects That Link RteFramework

```bash
# Install RteFramework
cd RteFramework
cmake --preset debug
cmake --build --preset debug
cmake --install build --prefix ~/.local/rte

# In your project CMakeLists.txt
find_package(RteFramework REQUIRED)
target_include_directories(myapp ${RteFramework_INCLUDE_DIR})
target_link_libraries(myapp rte_timer rte_log)
```

### For Projects That Reuse CMake Modules Only

Even without linking RteFramework, you can reuse its CMake helpers:

```bash
# Install just the CMake modules (already done via cmake --install)
# Then in your CMakeLists.txt:

find_package(RteFramework REQUIRED)  # Finds and loads modules

include(RTEHelpers)           # Helper functions for cppcheck, warnings, etc.
include(CompilerWarnings)         # Strict warning flags

# Now use the functions
rte_enable_cppcheck_misra(TARGET myapp)
rte_apply_strict_warnings(TARGET myapp)
```

---

## What's Installed

```
~/.local/rte/
├── include/rte/              # All public headers
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
│   ├── librte_*.a            # 12 static libraries (types is header-only)
│   └── cmake/RteFramework/
│       ├── RteFrameworkConfig.cmake         # find_package support
│       ├── RteFrameworkConfigVersion.cmake  # Version checking
│       ├── CompilerWarnings.cmake               # Warning flags
│       └── RTEHelpers.cmake                 # Helper functions
```

---

## Finding the Package

If installed to a non-standard location, tell CMake where to find it:

```bash
export CMAKE_PREFIX_PATH=~/.local/rte:$CMAKE_PREFIX_PATH
cmake --preset debug
```

Or pass it inline:
```bash
cmake --preset debug -DCMAKE_PREFIX_PATH=~/.local/rte
```

---

## Available Helper Functions (RTEHelpers.cmake)

After `find_package()` and `include(RTEHelpers)`:

### rte_enable_cppcheck_misra(TARGET <name> [SUPPRESS_RULE_15_5])
Enables MISRA C:2012 checking via cppcheck for a target.

```cmake
rte_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)
```

### rte_apply_strict_warnings(TARGET <name>)
Applies RteFramework's strict warning flags (-Wall -Wextra -Wpedantic -Werror).

```cmake
rte_apply_strict_warnings(TARGET myapp)
```

### rte_verify_conventions()
Validates that the project follows RteFramework conventions (C99, no malloc, etc.).

```cmake
rte_verify_conventions()
```

### rte_generate_presets_template(OUTPUT_FILE)
Generates a CMakePresets.json template for your project.

```cmake
rte_generate_presets_template("${CMAKE_SOURCE_DIR}/CMakePresets.json")
```

---

## Example: Downstream Project

See [docs/INTEGRATION.md](INTEGRATION.md) for a complete working example with source code.

**Key integration points:**

1. **Find the package:**
   ```cmake
   find_package(RteFramework 0.1.0 REQUIRED)
   ```

2. **Use paths:**
   ```cmake
   target_include_directories(myapp PRIVATE ${RteFramework_INCLUDE_DIR})
   ```

3. **Link libraries (if using RteFramework services):**
   ```cmake
   target_link_libraries(myapp rte_timer rte_log)
   ```

4. **Reuse modules:**
   ```cmake
   include(RTEHelpers)
   rte_apply_strict_warnings(TARGET myapp)
   rte_enable_cppcheck_misra(TARGET myapp SUPPRESS_RULE_15_5)
   ```

---

## Two Integration Patterns

### Pattern A: Submodule / Embedded
```cmake
add_subdirectory(deps/RteFramework)
# Targets available as rte_* directly
target_link_libraries(myapp rte_timer)
```

### Pattern B: Installed Package
```cmake
find_package(RteFramework REQUIRED)
# Use variables + manual linking
target_include_directories(myapp ${RteFramework_INCLUDE_DIR})
target_link_libraries(myapp ${RteFramework_LIB_DIR}/librte_timer.a)
```

Or use a link wrapper (recommended in downstream CMakeLists):
```cmake
# Create your own namespace targets wrapping RteFramework
add_library(mylibs::rte_timer STATIC IMPORTED)
set_target_properties(mylibs::rte_timer PROPERTIES
    IMPORTED_LOCATION "${RteFramework_LIB_DIR}/librte_timer.a"
    INTERFACE_INCLUDE_DIRECTORIES "${RteFramework_INCLUDE_DIR}"
)
target_link_libraries(myapp mylibs::rte_timer)
```

---

## Distribution Options

### 1. System Package Manager
```bash
# Install to system prefix (requires sudo)
sudo cmake --install build --prefix /usr/local
# Now all projects can use: find_package(RteFramework)
```

### 2. User/Project Directory
```bash
# Install to user home
cmake --install build --prefix ~/.local/rte
# Set CMAKE_PREFIX_PATH before building downstream projects
```

### 3. Monorepo / Git Submodule
```bash
# In your main project
git submodule add https://... deps/RteFramework
# In your CMakeLists.txt
add_subdirectory(deps/RteFramework)
```

### 4. Package Manager (vcpkg, Conan, etc.)
The CMakeLists.txt is compatible with standard package managers. Contributed recipes welcome.

---

## Version Checking

RteFramework provides version information:

```cmake
find_package(RteFramework 0.1.0 REQUIRED)
message("Using RteFramework ${RteFramework_FOUND}")
```

Version is defined in CMakeLists.txt:
```cmake
project(RteFramework VERSION 0.1.0)
```

---

## Questions?

See [docs/INTEGRATION.md](INTEGRATION.md) for detailed integration examples, or [CLAUDE.md](../CLAUDE.md) for coding standards and safety practices.
