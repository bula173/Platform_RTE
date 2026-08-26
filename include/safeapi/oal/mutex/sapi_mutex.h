/**
 * @file sapi_mutex.h
 * @brief OS Abstraction Layer - Mutual exclusion service.
 *
 * A portable, non-recursive blocking mutex for guarding a shared resource
 * accessed from more than one sapi_task (e.g. a single netlink handle sent
 * on by both the application's own cyclic executive and a background rx
 * task's own reconnect/echo path). REQ-OAL-MUTEX-001 (ADR-033): an
 * application shall never call a platform threading primitive (pthread_*,
 * a raw RTOS semaphore API, etc.) directly to guard its own state - doing
 * so ties that application code to one specific OS/RTOS, defeating the
 * whole point of building on this framework's OAL in the first place (a
 * backend is meant to be swappable without touching application code).
 * Added specifically to close that gap: found live in safeAPIRBC2oo2,
 * which had been using pthread_mutex_t directly (see this ADR's own
 * "Context" section, docs/architecture/ADR-033-sapi-mutex.md).
 *
 * REQ-OAL-MUTEX-002: no dynamic allocation; caller supplies storage.
 * REQ-OAL-MUTEX-003: non-recursive - locking twice from the same task
 *                     without an intervening unlock is undefined behavior
 *                     (matches pthread's own PTHREAD_MUTEX_DEFAULT/_NORMAL
 *                     semantics on Linux/macOS, the only backend this
 *                     framework ships today) - a caller needing recursive
 *                     locking must track that itself.
 *
 * @defgroup MUTEX Mutex Service
 * @brief Non-recursive blocking mutual exclusion (ADR-033)
 * @{
 */
#ifndef SAFEAPI_OS_MUTEX_H
#define SAFEAPI_OS_MUTEX_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque caller-owned storage for one mutex instance. Size is part of the ABI. */
SAFEAPI_DECLARE_STORAGE(sapi_mutex_storage_t, 128U);

/** Opaque handle bound to a sapi_mutex_storage_t after sapi_mutex_create(). */
typedef struct sapi_mutex_impl_s *sapi_mutex_handle_t;

/**
 * @brief Creates (and initializes, unlocked) a mutex bound to caller-owned
 *        storage.
 * @param storage     Caller-owned storage the mutex's state is placed
 *                    into. Must not be NULL and must outlive the mutex.
 * @param out_handle  Receives the created mutex's handle. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM for a bad argument;
 *         SAPI_STATUS_NOT_INITIALIZED if no backend is registered
 *         (sapi_mutex_register_backend()); SAPI_STATUS_NOT_SUPPORTED if
 *         the registered backend does not implement create;
 *         SAPI_STATUS_INVALID_STATE if the application's setup phase is
 *         already locked (ADR-026) - a mutex is a setup-only resource,
 *         same posture as sapi_timer_create()/sapi_task_create().
 * REQ-OAL-MUTEX-010
 */
sapi_status_t sapi_mutex_create(sapi_mutex_storage_t *storage, sapi_mutex_handle_t *out_handle);

/**
 * @brief Blocks the calling task until it holds the mutex.
 * @param handle  Mutex to lock. Must not be NULL.
 * @return SAPI_STATUS_OK, SAPI_STATUS_INVALID_PARAM, SAPI_STATUS_NOT_INITIALIZED,
 *         SAPI_STATUS_NOT_SUPPORTED (see sapi_mutex_create()), or
 *         SAPI_STATUS_INTERNAL_ERROR if the backend itself reports a
 *         failure (e.g. a detected deadlock).
 * REQ-OAL-MUTEX-011
 */
sapi_status_t sapi_mutex_lock(sapi_mutex_handle_t handle);

/**
 * @brief Releases a mutex previously locked by the calling task.
 * @param handle  Mutex to unlock. Must not be NULL. Unlocking a mutex the
 *                calling task does not hold is undefined behavior (same
 *                as pthread_mutex_unlock()).
 * @return See sapi_mutex_lock().
 * REQ-OAL-MUTEX-012
 */
sapi_status_t sapi_mutex_unlock(sapi_mutex_handle_t handle);

/**
 * @brief Destroys a mutex, releasing any backend resources bound to it.
 *        Must not be called while any task holds or is waiting on the lock.
 * @param handle  Mutex to destroy. Must not be NULL. Invalid to use after this call.
 * @return See sapi_mutex_lock().
 * REQ-OAL-MUTEX-013
 */
sapi_status_t sapi_mutex_destroy(sapi_mutex_handle_t handle);

/*
 * The backend vtable (sapi_mutex_backend_t) and sapi_mutex_register_backend()
 * live in safeapi_backend/mutex/sapi_mutex_backend.h, not here (ADR-021
 * consumer/backend split). This header is the consumer-facing surface
 * only - a real application never needs to see the backend vtable shape;
 * only the platform integrator wiring a concrete backend does.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_MUTEX_H */

/** @} */ /* MUTEX */
