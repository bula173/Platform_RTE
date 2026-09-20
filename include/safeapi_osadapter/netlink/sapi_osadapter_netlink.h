/**
 * @file sapi_osadapter_netlink.h
 * @brief OSAdapter interface for point-to-point network links.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_NETLINK_H
#define SAFEAPI_OSADAPTER_NETLINK_H

#include <stddef.h>
#include "safeapi/oal/netlink/sapi_netlink.h"
#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter network link operations vtable.
 */
typedef struct sapi_osadapter_netlink_s
{
    sapi_status_t (*open)(sapi_netlink_storage_t *storage,
                           const sapi_netlink_config_t *config,
                           sapi_netlink_handle_t *out_handle);
    sapi_status_t (*send)(sapi_netlink_handle_t handle, const void *message,
                           size_t message_size, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*receive)(sapi_netlink_handle_t handle, void *out_message,
                              size_t buffer_size, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*close)(sapi_netlink_handle_t handle);
} sapi_osadapter_netlink_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_netlink_t sapi_netlink_backend_t;

/**
 * @brief Registers the OSAdapter network link implementation.
 * @param adapter Pointer to netlink operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_netlink_register(const sapi_osadapter_netlink_t *adapter);

sapi_status_t sapi_netlink_register_backend(const sapi_netlink_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_NETLINK_H */
