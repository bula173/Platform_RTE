/**
 * @file sapi_osadapter.h
 * @brief OS Abstraction Layer - Master OSAdapter Interface.
 *
 * Consolidates all OS adapter primitives provided by a platform-specific
 * OSAdapter (e.g. POSIX, QNX, FreeRTOS):
 * - Memory, Clocks/Timers, Mutexes, Tasks/Threads
 * - Logging, NVM, Platform, Reboot
 * - IPC, Netlink, Flow
 * - Raw OS Sockets (sapi_osadapter_socket.h)
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_H
#define SAFEAPI_OSADAPTER_H

#include "safeapi_osadapter/sapi_osadapter_socket.h"
#include "safeapi_osadapter/memory/sapi_osadapter_memory.h"
#include "safeapi_osadapter/clocksync/sapi_osadapter_clocksync.h"
#include "safeapi_osadapter/timer/sapi_osadapter_timer.h"
#include "safeapi_osadapter/mutex/sapi_osadapter_mutex.h"
#include "safeapi_osadapter/task/sapi_osadapter_task.h"
#include "safeapi_osadapter/log/sapi_osadapter_log.h"
#include "safeapi_osadapter/nvm/sapi_osadapter_nvm.h"
#include "safeapi_osadapter/platform/sapi_osadapter_platform.h"
#include "safeapi_osadapter/reboot/sapi_osadapter_reboot.h"
#include "safeapi_osadapter/ipc/sapi_osadapter_ipc.h"
#include "safeapi_osadapter/netlink/sapi_osadapter_netlink.h"
#include "safeapi_osadapter/flow/sapi_osadapter_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    const sapi_osadapter_ipc_t        *ipc;
    const sapi_osadapter_netlink_t    *netlink;
    const sapi_osadapter_flow_t       *flow;
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
