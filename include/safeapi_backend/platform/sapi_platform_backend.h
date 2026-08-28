/**
 * @file sapi_platform_backend.h
 * @brief OS-backend adaptation surface for the Real-Time Platform
 *        Configuration service (ADR-035, ADR-005, ADR-021).
 *
 * For platform integrators implementing a sapi_platform_backend_t and
 * calling sapi_platform_register_backend() - NOT part of the consumer API
 * (safeapi/oal/platform/sapi_platform.h). A real application should never
 * include this file; only the startup code that wires a concrete backend
 * does.
 *
 * @defgroup PLATFORM_BACKEND Real-Time Platform Configuration - Backend Adaptation
 * @brief Backend vtable and registration for the platform service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OAL_PLATFORM_BACKEND_H
#define SAFEAPI_OAL_PLATFORM_BACKEND_H

#include "safeapi/oal/platform/sapi_platform.h"
#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the real-time
 *        platform bring-up for a specific target (ADR-005).
 */
typedef struct sapi_platform_backend_s
{
    /**
     * @brief Backend implementation of sapi_platform_realtime_init().
     *        May be NULL (the service then returns SAPI_STATUS_NOT_SUPPORTED).
     *
     * The framework has already range-checked rt_priority (0..99) before
     * this is called. The implementation is best-effort per
     * REQ-OAL-PLATFORM-001: it shall return SAPI_STATUS_OK even when it
     * can only partially apply the requested configuration.
     */
    sapi_status_t (*realtime_init)(uint32_t rt_priority);
} sapi_platform_backend_t;

/**
 * @brief Registers the backend implementation used by
 *        sapi_platform_realtime_init() (ADR-005 section 2.1). Call once at
 *        startup, before sapi_platform_realtime_init().
 * @param backend  Backend implementation. Must not be NULL. Registering
 *                 again replaces the previously registered backend.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL;
 *         SAPI_STATUS_INVALID_STATE if the application setup phase is
 *         already locked (ADR-026); SAPI_STATUS_OK otherwise.
 * REQ-OAL-PLATFORM-011
 */
sapi_status_t sapi_platform_register_backend(const sapi_platform_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OAL_PLATFORM_BACKEND_H */

/** @} */ /* PLATFORM_BACKEND */
