/**
 * @file rte_osadapter_log.h
 * @brief OSAdapter interface for logging and diagnostics.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_LOG_H
#define SAFEAPI_OSADAPTER_LOG_H

#include "safeapi/oal/log/rte_log.h"
#include "safeapi/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter logging operations vtable.
 */
typedef struct rte_osadapter_log_s
{
    rte_status_t (*init)(void);
    void (*write)(rte_log_level_t level, const char *tag, const char *message);
} rte_osadapter_log_t;

/* Backward compatibility typedef */
typedef rte_osadapter_log_t rte_log_backend_t;

/**
 * @brief Registers the OSAdapter log implementation.
 * @param adapter Pointer to log operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_log_register(const rte_osadapter_log_t *adapter);

rte_status_t rte_log_register_backend(const rte_log_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_LOG_H */
