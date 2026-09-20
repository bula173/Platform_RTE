/**
 * @file sapi_osadapter_platform.h
 * @brief OSAdapter interface for real-time platform configuration.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_PLATFORM_H
#define SAFEAPI_OSADAPTER_PLATFORM_H

#include <stdint.h>
#include "safeapi/oal/platform/sapi_platform.h"
#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter platform operations vtable.
 */
typedef struct sapi_osadapter_platform_s
{
    sapi_status_t (*realtime_init)(uint32_t rt_priority);
} sapi_osadapter_platform_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_platform_t sapi_platform_backend_t;

/**
 * @brief Registers the OSAdapter platform implementation.
 * @param adapter Pointer to platform operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_platform_register(const sapi_osadapter_platform_t *adapter);

sapi_status_t sapi_platform_register_backend(const sapi_platform_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_PLATFORM_H */
