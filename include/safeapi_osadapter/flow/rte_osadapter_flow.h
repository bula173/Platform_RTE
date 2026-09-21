/**
 * @file rte_osadapter_flow.h
 * @brief OSAdapter interface for OCORA PI-API Flow communication.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_FLOW_H
#define SAFEAPI_OSADAPTER_FLOW_H

#include <stddef.h>
#include "safeapi/oal/flow/rte_flow.h"
#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter Flow operations vtable.
 */
typedef struct rte_osadapter_flow_s
{
    rte_status_t (*open)(rte_flow_storage_t *storage,
                           const rte_flow_config_t *config,
                           rte_flow_handle_t *out_handle);
    rte_status_t (*send)(rte_flow_handle_t handle, const void *data, size_t data_size,
                           rte_flow_channel_t channel, rte_duration_ms_t timeout_ms);
    rte_status_t (*receive)(rte_flow_handle_t handle, void *out_data, size_t buffer_size,
                              rte_flow_channel_t *out_channel, rte_duration_ms_t timeout_ms);
    rte_status_t (*close)(rte_flow_handle_t handle);
    rte_status_t (*getattr)(rte_flow_handle_t handle, rte_flow_attr_t *out_attr);
    rte_status_t (*setattr)(rte_flow_handle_t handle, const rte_flow_attr_t *new_attr,
                              rte_flow_attr_t *out_old_attr);
} rte_osadapter_flow_t;

/* Backward compatibility typedef */
typedef rte_osadapter_flow_t rte_flow_backend_t;

/**
 * @brief Registers the OSAdapter Flow implementation.
 * @param adapter Pointer to flow operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_flow_register(const rte_osadapter_flow_t *adapter);

rte_status_t rte_flow_register_backend(const rte_flow_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_FLOW_H */
