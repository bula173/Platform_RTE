/**
 * @file sapi_osadapter_clocksync.h
 * @brief OSAdapter interface for clock synchronization.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_CLOCKSYNC_H
#define SAFEAPI_OSADAPTER_CLOCKSYNC_H

#include <stdint.h>
#include "safeapi/oal/clocksync/sapi_clocksync.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter clock synchronization operations vtable.
 */
typedef struct sapi_osadapter_clocksync_s
{
    sapi_status_t (*get_offset_ms)(int64_t *out_offset_ms);
    sapi_status_t (*get_quality)(sapi_clocksync_quality_t *out_quality);
} sapi_osadapter_clocksync_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_clocksync_t sapi_clocksync_backend_t;

/**
 * @brief Registers the OSAdapter clock synchronization implementation.
 * @param adapter Pointer to clocksync operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_clocksync_register(const sapi_osadapter_clocksync_t *adapter);

sapi_status_t sapi_clocksync_register_backend(const sapi_clocksync_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_CLOCKSYNC_H */
