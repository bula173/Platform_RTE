/**
 * @file rte_osadapter_netlink.h
 * @brief OSAdapter interface for point-to-point network links.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_NETLINK_H
#define SAFEAPI_OSADAPTER_NETLINK_H

#include <stddef.h>
#include "safeapi/oal/netlink/rte_netlink.h"
#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter network link operations vtable.
 */
typedef struct rte_osadapter_netlink_s
{
    rte_status_t (*open)(rte_netlink_storage_t *storage,
                           const rte_netlink_config_t *config,
                           rte_netlink_handle_t *out_handle);
    rte_status_t (*send)(rte_netlink_handle_t handle, const void *message,
                           size_t message_size, rte_duration_ms_t timeout_ms);
    rte_status_t (*receive)(rte_netlink_handle_t handle, void *out_message,
                              size_t buffer_size, rte_duration_ms_t timeout_ms);
    rte_status_t (*close)(rte_netlink_handle_t handle);
} rte_osadapter_netlink_t;

/**
 * @brief Registers the OSAdapter network link implementation.
 * @param adapter Pointer to netlink operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_netlink_register(const rte_osadapter_netlink_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_NETLINK_H */
