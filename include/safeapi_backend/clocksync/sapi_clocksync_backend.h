/**
 * @file sapi_clocksync_backend.h
 * @brief OS-backend adaptation surface for Clock Synchronization
 *        (ADR-005, ADR-017, ADR-021).
 *
 * For platform integrators implementing a sapi_clocksync_backend_t (PTP,
 * GPS discipline, NTP, or a custom link) and calling
 * sapi_clocksync_register_backend() - NOT part of the consumer API
 * (safeapi/clocksync/sapi_clocksync.h). A real application should never
 * include this file; only the startup code that wires a concrete
 * backend does.
 *
 * @defgroup CLOCKSYNC_BACKEND Clock Synchronization - Backend Adaptation
 * @brief Backend vtable and registration for clock sync (ADR-005)
 * @{
 */
#ifndef SAPI_CLOCKSYNC_BACKEND_H
#define SAPI_CLOCKSYNC_BACKEND_H

#include <stdint.h>

#include "safeapi/oal/clocksync/sapi_clocksync.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of clock
 *        synchronization for a specific mechanism (PTP/GPS/NTP/custom).
 *        Any slot may be NULL if unsupported by the backend
 *        (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_clocksync_backend_s
{
    /** Reports this node's estimated clock offset, in milliseconds,
     *  relative to the backend's reference (positive: local clock is
     *  ahead). */
    sapi_status_t (*get_offset_ms)(int64_t *out_offset_ms);
    /** Reports the backend's current confidence in that offset. */
    sapi_status_t (*get_quality)(sapi_clocksync_quality_t *out_quality);
} sapi_clocksync_backend_t;

/**
 * @brief Registers the backend implementation used by every
 *        sapi_clocksync_* call (ADR-005-style single global backend).
 * @param backend  Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK
 *         otherwise. Registering again replaces the previous backend.
 * REQ-CLOCKSYNC-010
 */
sapi_status_t sapi_clocksync_register_backend(const sapi_clocksync_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAPI_CLOCKSYNC_BACKEND_H */

/** @} */ /* CLOCKSYNC_BACKEND */
