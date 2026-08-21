/**
 * @file sapi_mutex_backend.h
 * @brief OS-backend adaptation surface for the Mutex service (ADR-033,
 *        ADR-021).
 *
 * This header is for platform/RTOS integrators implementing a
 * sapi_mutex_backend_t and calling sapi_mutex_register_backend() - it is
 * NOT part of the API a consuming application uses to create/lock/unlock
 * mutexes (that is safeapi/mutex/sapi_mutex.h). A real application should
 * never need to include this file; only the one piece of startup code
 * that wires a concrete backend into the framework should.
 *
 * @defgroup MUTEX_BACKEND Mutex Service - Backend Adaptation
 * @brief Backend vtable and registration for the Mutex service (ADR-033)
 * @{
 */
#ifndef SAFEAPI_OS_MUTEX_BACKEND_H
#define SAFEAPI_OS_MUTEX_BACKEND_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/mutex/sapi_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the mutex
 *        service for a specific OS/RTOS/BSP target (ADR-033). Any slot may
 *        be NULL if that operation is unsupported by the backend, in which
 *        case the corresponding sapi_mutex_* call returns
 *        SAPI_STATUS_NOT_SUPPORTED.
 */
typedef struct sapi_mutex_backend_s
{
    /** @brief Backend implementation of sapi_mutex_create(). May be NULL. */
    sapi_status_t (*create)(sapi_mutex_storage_t *storage, sapi_mutex_handle_t *out_handle);
    /** @brief Backend implementation of sapi_mutex_lock(). May be NULL. */
    sapi_status_t (*lock)(sapi_mutex_handle_t handle);
    /** @brief Backend implementation of sapi_mutex_unlock(). May be NULL. */
    sapi_status_t (*unlock)(sapi_mutex_handle_t handle);
    /** @brief Backend implementation of sapi_mutex_destroy(). May be NULL. */
    sapi_status_t (*destroy)(sapi_mutex_handle_t handle);
} sapi_mutex_backend_t;

/**
 * @brief Registers the backend implementation used by every sapi_mutex_*
 *        call. This is how an integrator supplies their own mutex
 *        implementation - call once at startup, before any other
 *        sapi_mutex_* function.
 * @param backend  Must not be NULL. Registering again replaces the
 *                 previously registered backend.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * @return SAPI_STATUS_INVALID_STATE if the application's setup phase is
 *         already locked (ADR-026).
 *
 * REQ-OAL-MUTEX-015
 */
sapi_status_t sapi_mutex_register_backend(const sapi_mutex_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_MUTEX_BACKEND_H */

/** @} */ /* MUTEX_BACKEND */
