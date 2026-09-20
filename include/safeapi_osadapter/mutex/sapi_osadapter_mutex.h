/**
 * @file sapi_osadapter_mutex.h
 * @brief OSAdapter interface for mutual exclusion.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_MUTEX_H
#define SAFEAPI_OSADAPTER_MUTEX_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/oal/mutex/sapi_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter mutex operations vtable.
 */
typedef struct sapi_osadapter_mutex_s
{
    sapi_status_t (*create)(sapi_mutex_storage_t *storage, sapi_mutex_handle_t *out_handle);
    sapi_status_t (*lock)(sapi_mutex_handle_t handle);
    sapi_status_t (*unlock)(sapi_mutex_handle_t handle);
    sapi_status_t (*destroy)(sapi_mutex_handle_t handle);
} sapi_osadapter_mutex_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_mutex_t sapi_mutex_backend_t;

/**
 * @brief Registers the OSAdapter mutex implementation.
 * @param adapter Pointer to mutex operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_mutex_register(const sapi_osadapter_mutex_t *adapter);

sapi_status_t sapi_mutex_register_backend(const sapi_mutex_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_MUTEX_H */
