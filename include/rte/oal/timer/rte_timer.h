/**
 * @file rte_timer.h
 * @brief OS Abstraction Layer - Timer service.
 *
 * Provides one-shot and periodic timers with millisecond resolution for
 * deadline supervision (e.g. ERTMS movement authority timeouts, cyclic
 * task watchdogs). See ADR-001.
 *
 * REQ-OAL-TIMER-001: no dynamic allocation; caller supplies storage.
 * REQ-OAL-TIMER-002: callback execution time is the caller's responsibility
 *                    to bound; the timer service itself must not block.
 *
 * @defgroup TIMER Timer Service
 * @brief One-shot and periodic timers with millisecond resolution (ADR-001)
 * @{
 */
#ifndef RTE_OS_TIMER_H
#define RTE_OS_TIMER_H

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque caller-owned storage for one timer instance. Size is part of the ABI. */
RTE_DECLARE_STORAGE(rte_timer_storage_t, 64U);

/** Opaque handle bound to a rte_timer_storage_t after rte_timer_create(). */
typedef struct rte_timer_impl_s *rte_timer_handle_t;

/** @brief Whether a timer fires once or repeatedly every period_ms. */
typedef enum rte_timer_mode_e
{
    RTE_TIMER_MODE_ONE_SHOT = 0, /**< Fires once after period_ms, then stops. */
    RTE_TIMER_MODE_PERIODIC = 1  /**< Fires every period_ms until stopped. */
} rte_timer_mode_t;

/**
 * @brief Signature for a timer expiry callback.
 * @param handle    The timer that expired.
 * @param user_ctx  Opaque context pointer supplied at creation time.
 *
 * REQ-OAL-TIMER-003: the callback executes in a bounded-time, non-blocking
 * context (exact context - task/ISR - is osadapter-defined and documented by
 * the OSAdapter implementation).
 */
typedef void (*rte_timer_callback_t)(rte_timer_handle_t handle, void *user_ctx);

/** @brief Configuration for rte_timer_create(). */
typedef struct rte_timer_config_s
{
    rte_timer_mode_t      mode;         /**< One-shot or periodic. */
    rte_duration_ms_t     period_ms;    /**< Period (or delay for one-shot). Must be > 0. */
    rte_timer_callback_t  callback;     /**< Must not be NULL. */
    void                  *user_ctx;     /**< Passed back to callback, may be NULL. */
} rte_timer_config_t;

/**
 * @brief Creates a timer bound to caller-owned storage. Does not start it.
 * @param storage     Caller-owned storage the timer's state is placed
 *                    into. Must not be NULL and must outlive the timer.
 * @param config      Timer configuration. Must not be NULL; config->callback
 *                    must not be NULL and config->period_ms must be > 0.
 * @param out_handle  Receives the created timer's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (rte_osadapter_timer_register()); RTE_STATUS_NOT_SUPPORTED if
 *         the registered OSAdapter does not implement create.
 * REQ-OAL-TIMER-010
 */
rte_status_t rte_timer_create(rte_timer_storage_t *storage,
                                 const rte_timer_config_t *config,
                                 rte_timer_handle_t *out_handle);

/**
 * @brief Starts (or restarts) a created timer.
 * @param handle  Timer to start. Must not be NULL.
 * @return RTE_STATUS_OK, RTE_STATUS_INVALID_PARAM, RTE_STATUS_NOT_INITIALIZED,
 *         or RTE_STATUS_NOT_SUPPORTED (see rte_timer_create()).
 * REQ-OAL-TIMER-011
 */
rte_status_t rte_timer_start(rte_timer_handle_t handle);

/**
 * @brief Stops a running timer; safe to call on an already-stopped timer.
 * @param handle  Timer to stop. Must not be NULL.
 * @return See rte_timer_start().
 * REQ-OAL-TIMER-012
 */
rte_status_t rte_timer_stop(rte_timer_handle_t handle);

/**
 * @brief Destroys a timer, releasing any OSAdapter resources bound to it.
 * @param handle  Timer to destroy. Must not be NULL. Invalid to use after this call.
 * @return See rte_timer_start().
 * REQ-OAL-TIMER-013
 */
rte_status_t rte_timer_destroy(rte_timer_handle_t handle);

/**
 * @brief Returns the current monotonic time base used by all timers.
 * @param out_now_ms  Receives the current time in milliseconds. Must not
 *                    be NULL; set to 0 if this call fails.
 * @return RTE_STATUS_OK, RTE_STATUS_INVALID_PARAM, RTE_STATUS_NOT_INITIALIZED,
 *         or RTE_STATUS_NOT_SUPPORTED (see rte_timer_create()).
 * REQ-OAL-TIMER-014
 */
rte_status_t rte_timer_now(rte_timestamp_ms_t *out_now_ms);

/*
 * The OSAdapter vtable (rte_osadapter_timer_t) and rte_osadapter_timer_register()
 * live in rte_osadapter/timer/rte_osadapter_timer.h, not here (ADR-021).
 * This header is the consumer-facing surface only - a real application
 * never needs to see the OSAdapter vtable shape; only the platform
 * integrator wiring a concrete OSAdapter does.
 */

#ifdef __cplusplus
}
#endif

#endif /* RTE_OS_TIMER_H */

/** @} */ /* TIMER */
