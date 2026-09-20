/**
 * @file sapi_osadapter.h
 * @brief OS Abstraction Layer - Master OSAdapter Interface.
 *
 * Consolidates all OS adapter primitives provided by a platform-specific
 * OSAdapter (e.g. POSIX, QNX, FreeRTOS):
 * - Memory, Clocks/Timers, Mutexes, Tasks/Threads
 * - Logging, NVM, Platform, Reboot
 * - Raw OS Sockets (sapi_osadapter_socket.h)
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 *
 * @defgroup OSADAPTER OSAdapter Interface
 * @brief Master contract for OS adaptation in SafeAPI
 * @{
 */
#ifndef SAFEAPI_OSADAPTER_H
#define SAFEAPI_OSADAPTER_H

#include "safeapi_osadapter/sapi_osadapter_socket.h"

/* Include individual backend contracts and expose under OSAdapter naming */
#include "safeapi_backend/memory/sapi_memory_backend.h"
#include "safeapi_backend/clocksync/sapi_clocksync_backend.h"
#include "safeapi_backend/timer/sapi_timer_backend.h"
#include "safeapi_backend/mutex/sapi_mutex_backend.h"
#include "safeapi_backend/task/sapi_task_backend.h"
#include "safeapi_backend/log/sapi_log_backend.h"
#include "safeapi_backend/nvm/sapi_nvm_backend.h"
#include "safeapi_backend/platform/sapi_platform_backend.h"
#include "safeapi_backend/reboot/sapi_reboot_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief OSAdapter Memory interface. */
typedef sapi_mem_pool_backend_t sapi_osadapter_memory_t;

/** @brief OSAdapter Clock Synchronization interface. */
typedef sapi_clocksync_backend_t sapi_osadapter_clocksync_t;

/** @brief OSAdapter Timer interface. */
typedef sapi_timer_backend_t sapi_osadapter_timer_t;

/** @brief OSAdapter Mutex interface. */
typedef sapi_mutex_backend_t sapi_osadapter_mutex_t;

/** @brief OSAdapter Task/Thread interface. */
typedef sapi_task_backend_t sapi_osadapter_task_t;

/** @brief OSAdapter Logging interface. */
typedef sapi_log_backend_t sapi_osadapter_log_t;

/** @brief OSAdapter Non-Volatile Memory interface. */
typedef sapi_nvm_backend_t sapi_osadapter_nvm_t;

/** @brief OSAdapter Platform interface. */
typedef sapi_platform_backend_t sapi_osadapter_platform_t;

/** @brief OSAdapter Reboot interface. */
typedef sapi_reboot_backend_t sapi_osadapter_reboot_t;

/**
 * @brief Comprehensive bundle of all OSAdapter vtables for a platform.
 */
typedef struct sapi_osadapter_bundle_s
{
    const sapi_osadapter_memory_t     *memory;
    const sapi_osadapter_clocksync_t  *clocksync;
    const sapi_osadapter_timer_t      *timer;
    const sapi_osadapter_mutex_t      *mutex;
    const sapi_osadapter_task_t       *task;
    const sapi_osadapter_log_t        *log;
    const sapi_osadapter_nvm_t        *nvm;
    const sapi_osadapter_platform_t   *platform;
    const sapi_osadapter_reboot_t     *reboot;
    const sapi_os_socket_ops_t        *sockets;
} sapi_osadapter_bundle_t;

/**
 * @brief Registers all OSAdapter interfaces at once from an OSAdapter bundle.
 * @param bundle Pointer to bundle with valid vtables. Must not be NULL.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if bundle is NULL.
 */
sapi_status_t sapi_osadapter_register_all(const sapi_osadapter_bundle_t *bundle);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_H */

/** @} */
