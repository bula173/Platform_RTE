/**
 * @file sapi_osadapter_timer.h
 * @brief OSAdapter interface for monotonic timers and timestamps.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_TIMER_H
#define SAFEAPI_OSADAPTER_TIMER_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/oal/timer/sapi_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter timer operations vtable.
 */
typedef struct sapi_osadapter_timer_s
{
    sapi_status_t (*create)(sapi_timer_storage_t *storage,
                             const sapi_timer_config_t *config,
                             sapi_timer_handle_t *out_handle);
    sapi_status_t (*start)(sapi_timer_handle_t handle);
    sapi_status_t (*stop)(sapi_timer_handle_t handle);
    sapi_status_t (*destroy)(sapi_timer_handle_t handle);
    sapi_status_t (*now)(sapi_timestamp_ms_t *out_now_ms);
} sapi_osadapter_timer_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_timer_t sapi_timer_backend_t;

/**
 * @brief Registers the OSAdapter timer implementation.
 * @param adapter Pointer to timer operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_timer_register(const sapi_osadapter_timer_t *adapter);

sapi_status_t sapi_timer_register_backend(const sapi_timer_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_TIMER_H */
