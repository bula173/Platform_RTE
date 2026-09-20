/**
 * @file sapi_osadapter_log.h
 * @brief OSAdapter interface for logging and diagnostics.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_LOG_H
#define SAFEAPI_OSADAPTER_LOG_H

#include "safeapi/oal/log/sapi_log.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter logging operations vtable.
 */
typedef struct sapi_osadapter_log_s
{
    sapi_status_t (*init)(void);
    void (*write)(sapi_log_level_t level, const char *tag, const char *message);
} sapi_osadapter_log_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_log_t sapi_log_backend_t;

/**
 * @brief Registers the OSAdapter log implementation.
 * @param adapter Pointer to log operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_log_register(const sapi_osadapter_log_t *adapter);

sapi_status_t sapi_log_register_backend(const sapi_log_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_LOG_H */
