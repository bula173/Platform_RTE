/**
 * @file sapi_log.h
 * @brief OS Abstraction Layer - Logging/diagnostics service.
 *
 * NON-SAFETY-RELATED. Black-box/event-recorder style diagnostic logging.
 * This service must never sit on a safety execution path: it must not
 * block callers, must not be able to cause a safety function to miss a
 * deadline, and its failure shall never affect safety behavior. See
 * ADR-001, section 4 (item 6).
 *
 * REQ-OAL-LOG-001: log calls are best-effort and non-blocking; a full
 *                  backend buffer silently drops the newest entries rather
 *                  than blocking or erroring the caller's control flow.
 *
 * @defgroup LOG Logging and Diagnostics
 * @brief Non-safety-related black-box/event-recorder logging (ADR-001)
 * @{
 */
#ifndef SAFEAPI_OS_LOG_H
#define SAFEAPI_OS_LOG_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Log message severity, passed to sapi_log_write(). */
typedef enum sapi_log_level_e
{
    SAPI_LOG_LEVEL_DEBUG   = 0, /**< Verbose diagnostic detail. */
    SAPI_LOG_LEVEL_INFO    = 1, /**< Normal operational events. */
    SAPI_LOG_LEVEL_WARNING = 2, /**< Unexpected but recovered condition. */
    SAPI_LOG_LEVEL_ERROR   = 3  /**< Failure worth operator attention. */
} sapi_log_level_t;

/**
 * @brief Initializes the logging backend. Safe to call once at startup.
 * @return SAPI_STATUS_OK if no backend is registered yet (a no-op logger
 *         is a valid state) or if the registered backend's init succeeds;
 *         a backend-defined error status otherwise.
 * REQ-OAL-LOG-010
 */
sapi_status_t sapi_log_init(void);

/**
 * @brief Emits one log message. Non-blocking; never fails the caller's
 *        control flow even if the message is dropped.
 * @param level    Severity level.
 * @param tag      Short diagnostic source tag, backend-defined interpretation, may be NULL.
 * @param message  Human-readable message text, may be NULL.
 * REQ-OAL-LOG-011
 */
void sapi_log_write(sapi_log_level_t level, const char *tag, const char *message);

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

#endif /* SAFEAPI_OS_LOG_H */

/** @} */ /* LOG */
