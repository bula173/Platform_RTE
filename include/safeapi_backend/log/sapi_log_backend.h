/**
 * @file sapi_log_backend.h
 * @brief OS-backend adaptation surface for the Logging/diagnostics service
 *        (ADR-005, ADR-021).
 *
 * For platform integrators implementing a sapi_log_backend_t and calling
 * sapi_log_register_backend() - NOT part of the consumer API
 * (safeapi/log/sapi_log.h). A real application should never include this
 * file; only the startup code that wires a concrete backend does.
 *
 * @defgroup LOG_BACKEND Logging and Diagnostics - Backend Adaptation
 * @brief Backend vtable and registration for the logging service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_LOG_BACKEND_H
#define SAFEAPI_OS_LOG_BACKEND_H

#include "safeapi/oal/log/sapi_log.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the logging
 *        sink (ADR-005). Unlike every other OAL service, an unregistered
 *        backend is not an error here: sapi_log_write() with no backend
 *        registered silently does nothing, consistent with
 *        REQ-OAL-LOG-001 (logging must never affect caller control flow).
 */
typedef struct sapi_log_backend_s
{
    /** @brief Backend implementation of sapi_log_init(). May be NULL. */
    sapi_status_t (*init)(void);
    /** @brief Backend implementation of sapi_log_write(). May be NULL. */
    void (*write)(sapi_log_level_t level, const char *tag, const char *message);
} sapi_log_backend_t;

/**
 * @brief Registers the backend implementation used by sapi_log_init()/
 *        sapi_log_write() (ADR-005 section 2.1/2.5). Call once at startup.
 * @param backend Vtable of backend function pointers. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-LOG-012
 */
sapi_status_t sapi_log_register_backend(const sapi_log_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_LOG_BACKEND_H */

/** @} */ /* LOG_BACKEND */
