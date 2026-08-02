# Shared compiler warning / hardening flags for safety-related C sources.
# Kept centralized so every OAL service target applies the same rules.

function(safeapi_set_warnings target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wcast-qual
            -Wshadow
            -Wundef
            -Wswitch-enum
            -Wpointer-arith
        )
        if(SAFEAPI_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4)
        if(SAFEAPI_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()
