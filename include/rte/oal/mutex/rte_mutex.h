/**
 * @file rte_mutex.h
 * @brief OS Abstraction Layer - Mutual exclusion service.
 *
 * A portable, non-recursive blocking mutex for guarding a shared resource
 * accessed from more than one rte_task (e.g. a single netlink handle sent
 * on by both the application's own cyclic executive and a background rx
 * task's own reconnect/echo path). REQ-OAL-MUTEX-001 (ADR-033): an
 * application shall never call a platform threading primitive (pthread_*,
 * a raw RTOS semaphore API, etc.) directly to guard its own state - doing
 * so ties that application code to one specific OS/RTOS, defeating the
 * whole point of building on this framework's OAL in the first place (a
 * OSAdapter is meant to be swappable without touching application code).
 * Added specifically to close that gap: found live in safeAPIRBC2oo2,
 * which had been using pthread_mutex_t directly (see this ADR's own
 * "Context" section, docs/architecture/ADR-033-sapi-mutex.md).
 *
 * REQ-OAL-MUTEX-002: no dynamic allocation; caller supplies storage.
 * REQ-OAL-MUTEX-003: non-recursive - locking twice from the same task
 *                     without an intervening unlock is undefined behavior
 *                     (matches pthread's own PTHREAD_MUTEX_DEFAULT/_NORMAL
 *                     semantics on Linux/macOS, the only OSAdapter this
 *                     framework ships today) - a caller needing recursive
 *                     locking must track that itself.
 * REQ-OAL-MUTEX-004: every mutex uses priority inheritance (OSA rule R2);
 *                     an OSAdapter that cannot provide it fails the create
 *                     and never falls back to a plain mutex.
 *
 * @defgroup MUTEX Mutex Service
 * @brief Non-recursive blocking mutual exclusion (ADR-033)
 * @{
 */
#ifndef RTE_OS_MUTEX_H
#define RTE_OS_MUTEX_H

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque caller-owned storage for one mutex instance. Size is part of the ABI. */
RTE_DECLARE_STORAGE(rte_mutex_storage_t, 128U);

/** Opaque handle bound to a rte_mutex_storage_t after rte_mutex_create(). */
typedef struct rte_mutex_impl_s *rte_mutex_handle_t;

/**
 * @brief Creates (and initializes, unlocked) a priority-inheritance mutex
 *        bound to caller-owned storage (REQ-OAL-MUTEX-004).
 * @param storage     Caller-owned storage the mutex's state is placed
 *                    into. Must not be NULL and must outlive the mutex.
 * @param out_handle  Receives the created mutex's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (rte_osadapter_mutex_register()); RTE_STATUS_NOT_SUPPORTED if
 *         the registered OSAdapter does not implement create;
 *         RTE_STATUS_INVALID_STATE if the application's setup phase is
 *         already locked (ADR-026) - a mutex is a setup-only resource,
 *         same posture as rte_timer_create()/rte_task_create();
 *         RTE_STATUS_INTERNAL_ERROR if the OSAdapter cannot create a
 *         priority-inheritance mutex (OSA rule R2) - the refusal is reported
 *         as DEGRADED / RTE_SAFESTATE_REASON_OSA_RT_ATTRIBUTE and there is no
 *         fallback to a plain mutex.
 * REQ-OAL-MUTEX-010
 */
rte_status_t rte_mutex_create(rte_mutex_storage_t *storage, rte_mutex_handle_t *out_handle);

/**
 * @brief Blocks the calling task until it holds the mutex.
 * @param handle  Mutex to lock. Must not be NULL.
 * @return RTE_STATUS_OK, RTE_STATUS_INVALID_PARAM, RTE_STATUS_NOT_INITIALIZED,
 *         RTE_STATUS_NOT_SUPPORTED (see rte_mutex_create()), or
 *         RTE_STATUS_INTERNAL_ERROR if the OSAdapter itself reports a
 *         failure (e.g. a detected deadlock).
 * REQ-OAL-MUTEX-011
 */
rte_status_t rte_mutex_lock(rte_mutex_handle_t handle);

/**
 * @brief Waits at most timeout_ms for the mutex (OSA rule R2: every wait
 *        bounded).
 *
 * Not gated by the setup-phase lock: an operational call, like
 * rte_mutex_lock(). On RTE_STATUS_OK the calling task holds the mutex and
 * must release it with rte_mutex_unlock(); on any other status it does not
 * hold it.
 * @param handle      Mutex to lock. Must not be NULL.
 * @param timeout_ms  Maximum time to wait for the mutex, in milliseconds.
 *                    0 is one non-blocking try (same rule as
 *                    rte_ipc_send()/rte_ipc_receive()).
 * @return RTE_STATUS_OK with the lock held; RTE_STATUS_TIMEOUT if the lock
 *         was not acquired within timeout_ms (lock not held);
 *         RTE_STATUS_INVALID_PARAM for a NULL handle;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered;
 *         RTE_STATUS_NOT_SUPPORTED if the registered OSAdapter does not
 *         implement lock_timed; RTE_STATUS_INTERNAL_ERROR for any other
 *         OSAdapter failure.
 * REQ-OAL-MUTEX-014
 */
rte_status_t rte_mutex_lock_timed(rte_mutex_handle_t handle, rte_duration_ms_t timeout_ms);

/**
 * @brief Releases a mutex previously locked by the calling task.
 * @param handle  Mutex to unlock. Must not be NULL. Unlocking a mutex the
 *                calling task does not hold is undefined behavior (same
 *                as pthread_mutex_unlock()).
 * @return See rte_mutex_lock().
 * REQ-OAL-MUTEX-012
 */
rte_status_t rte_mutex_unlock(rte_mutex_handle_t handle);

/**
 * @brief Destroys a mutex, releasing any OSAdapter resources bound to it.
 *        Must not be called while any task holds or is waiting on the lock.
 * @param handle  Mutex to destroy. Must not be NULL. Invalid to use after this call.
 * @return See rte_mutex_lock().
 * REQ-OAL-MUTEX-013
 */
rte_status_t rte_mutex_destroy(rte_mutex_handle_t handle);

/*
 * The OSAdapter vtable (rte_osadapter_mutex_t) and rte_osadapter_mutex_register()
 * live in rte_osadapter/mutex/rte_osadapter_mutex.h, not here (ADR-021
 * consumer/osadapter split). This header is the consumer-facing surface
 * only - a real application never needs to see the OSAdapter vtable shape;
 * only the platform integrator wiring a concrete OSAdapter does.
 */

#ifdef __cplusplus
}
#endif

#endif /* RTE_OS_MUTEX_H */

/** @} */ /* MUTEX */
