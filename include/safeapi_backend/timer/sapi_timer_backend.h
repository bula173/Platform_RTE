/**
 * @file sapi_timer_backend.h
 * @brief OS-backend adaptation surface for the Timer service (ADR-005,
 *        ADR-021).
 *
 * This header is for platform/RTOS integrators implementing a
 * sapi_timer_backend_t and calling sapi_timer_register_backend() - it is
 * NOT part of the API a consuming application uses to create/start/stop
 * timers (that is safeapi/timer/sapi_timer.h). A real application should
 * never need to include this file; only the one piece of startup code
 * that wires a concrete backend into the framework should.
 *
 * @defgroup TIMER_BACKEND Timer Service - Backend Adaptation
 * @brief Backend vtable and registration for the Timer service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_TIMER_BACKEND_H
#define SAFEAPI_OS_TIMER_BACKEND_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/oal/timer/sapi_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the timer
 *        service for a specific OS/RTOS/BSP target (ADR-005). Any slot may
 *        be NULL if that operation is unsupported by the backend, in which
 *        case the corresponding sapi_timer_* call returns
 *        SAPI_STATUS_NOT_SUPPORTED.
 */
typedef struct sapi_timer_backend_s
{
    /** @brief Backend implementation of sapi_timer_create(). May be NULL. */
    sapi_status_t (*create)(sapi_timer_storage_t *storage,
                             const sapi_timer_config_t *config,
                             sapi_timer_handle_t *out_handle);
    /** @brief Backend implementation of sapi_timer_start(). May be NULL. */
    sapi_status_t (*start)(sapi_timer_handle_t handle);
    /** @brief Backend implementation of sapi_timer_stop(). May be NULL. */
    sapi_status_t (*stop)(sapi_timer_handle_t handle);
    /** @brief Backend implementation of sapi_timer_destroy(). May be NULL. */
    sapi_status_t (*destroy)(sapi_timer_handle_t handle);
    /** @brief Backend implementation of sapi_timer_now(). May be NULL. */
    sapi_status_t (*now)(sapi_timestamp_ms_t *out_now_ms);
} sapi_timer_backend_t;

/**
 * @brief Registers the backend implementation used by every sapi_timer_*
 *        call. This is how an integrator supplies their own timer
 *        implementation (ADR-005 section 2.1) - call once at startup,
 *        before any other sapi_timer_* function.
 * @param backend  Must not be NULL. Registering again replaces the
 *                 previously registered backend.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * @return SAPI_STATUS_INVALID_STATE if the application's setup phase is
 *         already locked (ADR-026).
 *
 * REQ-OAL-TIMER-015
 */
sapi_status_t sapi_timer_register_backend(const sapi_timer_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_TIMER_BACKEND_H */

/** @} */ /* TIMER_BACKEND */
