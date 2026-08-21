# SafeAPIHelpers.cmake
# Reusable CMake functions for projects using safeAPIFramework
#
# Usage in downstream CMakeLists.txt:
#   find_package(safeAPIFramework REQUIRED)
#   include(SafeAPIHelpers)
#   sapi_enable_cppcheck_misra(TARGET mytarget)
#   sapi_apply_strict_warnings(TARGET mytarget)

if(COMMAND sapi_enable_cppcheck_misra)
  return()  # Prevent multiple inclusions
endif()

# Enable cppcheck with MISRA C:2012 analysis for a target
# Usage: sapi_enable_cppcheck_misra(TARGET mytarget [SUPPRESS_RULE_15_5])
function(sapi_enable_cppcheck_misra)
  cmake_parse_arguments(ARGS "SUPPRESS_RULE_15_5" "TARGET" "" ${ARGN})

  if(NOT ARGS_TARGET)
    message(FATAL_ERROR "sapi_enable_cppcheck_misra: TARGET argument required")
  endif()

  find_program(CPPCHECK_EXECUTABLE cppcheck)
  if(NOT CPPCHECK_EXECUTABLE)
    message(STATUS "cppcheck not found - MISRA analysis disabled for ${ARGS_TARGET}")
    return()
  endif()

  set(CPPCHECK_FLAGS
    --addon=misra
    --std=c99
    --enable=all
    --inconclusive
  )

  if(ARGS_SUPPRESS_RULE_15_5)
    list(APPEND CPPCHECK_FLAGS --suppress=misra-single-exit)
  endif()

  set_target_properties(${ARGS_TARGET} PROPERTIES
    C_CPPCHECK "${CPPCHECK_EXECUTABLE};${CPPCHECK_FLAGS}"
  )
endfunction()

# Apply strict compiler warnings (matching safeAPIFramework standards)
# This includes the shared CompilerWarnings module
# Usage: sapi_apply_strict_warnings(TARGET mytarget)
function(sapi_apply_strict_warnings)
  cmake_parse_arguments(ARGS "" "TARGET" "" ${ARGN})

  if(NOT ARGS_TARGET)
    message(FATAL_ERROR "sapi_apply_strict_warnings: TARGET argument required")
  endif()

  include(CompilerWarnings OPTIONAL RESULT_VARIABLE _COMPILER_WARNINGS_FOUND)
  if(_COMPILER_WARNINGS_FOUND)
    target_compile_options(${ARGS_TARGET} PRIVATE
      $<$<C_COMPILER_ID:GNU>: -Wall -Wextra -Wpedantic -Werror>
      $<$<C_COMPILER_ID:Clang>: -Wall -Wextra -Wpedantic -Werror>
      $<$<C_COMPILER_ID:MSVC>: /W4 /WX>
    )
  else()
    message(WARNING "CompilerWarnings.cmake not found in CMAKE_MODULE_PATH")
  endif()
endfunction()

# Create a CMakePresets.json template for a downstream project
# Usage: sapi_generate_presets_template(OUTPUT_FILE)
function(sapi_generate_presets_template OUTPUT_FILE)
  set(PRESET_TEMPLATE
"{
  \"version\": 3,
  \"vendor\": {
    \"safeapi\": {
      \"description\": \"Presets based on safeAPIFramework standards\"
    }
  },
  \"configurePresets\": [
    {
      \"name\": \"debug\",
      \"description\": \"Debug build with safeAPIFramework\",
      \"generator\": \"Unix Makefiles\",
      \"binaryDir\": \"\${sourceDir}/build\",
      \"cacheVariables\": {
        \"CMAKE_BUILD_TYPE\": \"Debug\",
        \"CMAKE_EXPORT_COMPILE_COMMANDS\": \"ON\"
      }
    },
    {
      \"name\": \"release\",
      \"description\": \"Release build\",
      \"generator\": \"Unix Makefiles\",
      \"binaryDir\": \"\${sourceDir}/build\",
      \"cacheVariables\": {
        \"CMAKE_BUILD_TYPE\": \"Release\",
        \"CMAKE_EXPORT_COMPILE_COMMANDS\": \"ON\"
      }
    },
    {
      \"name\": \"asan\",
      \"description\": \"AddressSanitizer build\",
      \"inherits\": \"debug\",
      \"cacheVariables\": {
        \"CMAKE_C_FLAGS\": \"-fsanitize=address -fno-omit-frame-pointer\",
        \"CMAKE_CXX_FLAGS\": \"-fsanitize=address -fno-omit-frame-pointer\"
      }
    }
  ]
}")

  file(WRITE "${OUTPUT_FILE}" "${PRESET_TEMPLATE}")
  message(STATUS "CMakePresets.json template written to ${OUTPUT_FILE}")
endfunction()

# Enforce a feature option's build-time dependencies (ADR-024). FATAL_ERRORs
# with the exact -D flag to pass, rather than silently auto-enabling code the
# integrator didn't explicitly ask for - a disabled dependency here is a
# configuration mistake to surface, not a gap to paper over, since which
# modules get compiled also bounds this build's SIL verification scope.
# Usage: safeapi_require_feature(SAFEAPI_ENABLE_WATCHDOG DEPENDS SAFEAPI_ENABLE_LOG SAFEAPI_ENABLE_TIMER)
function(safeapi_require_feature FEATURE_VAR)
  cmake_parse_arguments(ARGS "" "" "DEPENDS" ${ARGN})

  if(NOT ${FEATURE_VAR})
    return()  # feature itself is off - its dependencies are irrelevant
  endif()

  foreach(_dep ${ARGS_DEPENDS})
    if(NOT ${_dep})
      message(FATAL_ERROR
        "${FEATURE_VAR}=ON requires ${_dep}=ON, but ${_dep} is OFF. "
        "Enable it with -D${_dep}=ON, or turn ${FEATURE_VAR} OFF.")
    endif()
  endforeach()
endfunction()

# Verify that project follows safeAPIFramework conventions
# Usage: sapi_verify_conventions()
function(sapi_verify_conventions)
  message(STATUS "Verifying safeAPIFramework conventions...")

  # Check that C standard is set to at least C99
  if(NOT CMAKE_C_STANDARD)
    message(WARNING "CMAKE_C_STANDARD not set; safeAPIFramework requires C99 or later")
  elseif(CMAKE_C_STANDARD LESS 99)
    message(WARNING "CMAKE_C_STANDARD is ${CMAKE_C_STANDARD}; safeAPIFramework requires C99 or later")
  endif()

  # Check that dynamic memory allocation is avoided
  # (This is enforced at review time, not by CMake)
  message(STATUS "✓ Ensure no malloc/free in production code (enforced at review)")

  message(STATUS "✓ Conventions check complete")
endfunction()
