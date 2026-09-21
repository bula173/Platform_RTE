/**
 * @file rte_osadapter.h
 * @brief OS Abstraction Layer - Master OSAdapter Interface.
 *
 * Consolidates all OS adapter primitives provided by a platform-specific
 * OSAdapter (e.g. POSIX, QNX, FreeRTOS):
 * - Memory, Clocks/Timers, Mutexes, Tasks/Threads
 * - Logging, NVM, Platform, Reboot
 * - IPC, Netlink, Flow
 * - Raw OS Sockets (rte_osadapter_socket.h)
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_H
#define SAFEAPI_OSADAPTER_H

#include "safeapi_osadapter/rte_osadapter_socket.h"
#include "safeapi_osadapter/memory/rte_osadapter_memory.h"
#include "safeapi_osadapter/clocksync/rte_osadapter_clocksync.h"
#include "safeapi_osadapter/timer/rte_osadapter_timer.h"
#include "safeapi_osadapter/mutex/rte_osadapter_mutex.h"
#include "safeapi_osadapter/task/rte_osadapter_task.h"
#include "safeapi_osadapter/log/rte_osadapter_log.h"
#include "safeapi_osadapter/nvm/rte_osadapter_nvm.h"
#include "safeapi_osadapter/platform/rte_osadapter_platform.h"
#include "safeapi_osadapter/reboot/rte_osadapter_reboot.h"
#include "safeapi_osadapter/ipc/rte_osadapter_ipc.h"
#include "safeapi_osadapter/netlink/rte_osadapter_netlink.h"
#include "safeapi_osadapter/flow/rte_osadapter_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Comprehensive bundle of all OSAdapter vtables for a platform.
 */
typedef struct rte_osadapter_bundle_s
{
    const rte_osadapter_memory_t     *memory;
    const rte_osadapter_clocksync_t  *clocksync;
    const rte_osadapter_timer_t      *timer;
    const rte_osadapter_mutex_t      *mutex;
    const rte_osadapter_task_t       *task;
    const rte_osadapter_log_t        *log;
    const rte_osadapter_nvm_t        *nvm;
    const rte_osadapter_platform_t   *platform;
    const rte_osadapter_reboot_t     *reboot;
    const rte_osadapter_ipc_t        *ipc;
    const rte_osadapter_netlink_t    *netlink;
    const rte_osadapter_flow_t       *flow;
    const rte_os_socket_ops_t        *sockets;
} rte_osadapter_bundle_t;

/**
 * @brief Registers all OSAdapter interfaces at once from an OSAdapter bundle.
 * @param bundle Pointer to bundle with valid vtables. Must not be NULL.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if bundle is NULL.
 */
rte_status_t rte_osadapter_register_all(const rte_osadapter_bundle_t *bundle);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_H */
