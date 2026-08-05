# POSIX/Linux OAL Backend (ADR-018)
# Real backend implementation for every OAL service, POSIX-only.

if(UNIX)
    add_library(safeapi_posix_backend STATIC
        sapi_posix_backend.c
        sapi_posix_backend_timer.c
        sapi_posix_backend_ipc.c
        sapi_posix_backend_task.c
        sapi_posix_backend_log.c
        sapi_posix_backend_nvm.c
        sapi_posix_backend_reboot.c
        sapi_posix_backend_memory.c
    )

    target_include_directories(safeapi_posix_backend
        PUBLIC
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(safeapi_posix_backend
        PUBLIC
            safeapi::status
            safeapi::types
            safeapi::timer
            safeapi::ipc
            safeapi::task
            safeapi::log
            safeapi::nvm
            safeapi::reboot
            safeapi::memory
    )

    find_package(Threads REQUIRED)
    target_link_libraries(safeapi_posix_backend PUBLIC Threads::Threads)

    safeapi_set_warnings(safeapi_posix_backend)

    add_library(safeapi::posix_backend ALIAS safeapi_posix_backend)
else()
    message(STATUS "posix_backend: skipped (non-UNIX target)")
endif()
