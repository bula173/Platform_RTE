/**
 * @file rte_platform.h
 * @brief OS Abstraction Layer - Real-time platform configuration service
 *        (ADR-035).
 *
 * A one-shot startup hook for the platform-level execution characteristics
 * a cyclic safety application needs in place before it creates any
 * timer/task threads: locking process memory resident so later execution
 * is free of demand-paging jitter, and raising the calling thread to a
 * real-time scheduling policy/priority. What that means concretely is
 * entirely backend-defined (a PREEMPT_RT Linux backend uses
 * mlockall()/SCHED_FIFO; a bare-metal backend may do nothing at all) -
 * this header is only the backend-agnostic seam, so application startup
 * code never has to name a concrete backend to ask for it (ADR-005).
 *
 * REQ-OAL-PLATFORM-001: rte_platform_realtime_init() is best-effort - a
 *                       backend that cannot obtain some or all of the
 *                       requested capabilities (unprivileged host, no RT
 *                       scheduler, a bounded lockable-memory limit) shall
 *                       still return RTE_STATUS_OK, having applied what it
 *                       could; it never fails the caller's startup.
 *
 * @defgroup PLATFORM Real-Time Platform Configuration
 * @brief Backend-defined one-shot real-time bring-up (ADR-035)
 * @{
 */
#ifndef SAFEAPI_OAL_PLATFORM_H
#define SAFEAPI_OAL_PLATFORM_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Applies the registered backend's real-time platform
 *        configuration for the calling process/thread. Call once at
 *        process startup, after the backend is registered and before any
 *        rte_timer_* / rte_task_* thread is created.
 *
 * @param rt_priority  Desired real-time scheduling priority, 0..99. 0
 *                     requests memory-residency configuration only, with
 *                     no change to scheduling policy/priority. Values are
 *                     clamped by the backend to the target's supported
 *                     range.
 * @return RTE_STATUS_INVALID_PARAM if rt_priority > 99.
 *         RTE_STATUS_NOT_INITIALIZED if no backend is registered.
 *         RTE_STATUS_NOT_SUPPORTED if a backend is registered but does
 *         not implement this operation.
 *         RTE_STATUS_OK otherwise - including when the backend could only
 *         partially apply the requested configuration (REQ-OAL-PLATFORM-001).
 *
 * REQ-OAL-PLATFORM-010
 */
rte_status_t rte_platform_realtime_init(uint32_t rt_priority);

/*
 * The backend vtable (rte_platform_backend_t) and
 * rte_platform_register_backend() live in
 * safeapi_backend/platform/rte_platform_backend.h, not here (ADR-021).
 * This header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OAL_PLATFORM_H */

/** @} */ /* PLATFORM */
