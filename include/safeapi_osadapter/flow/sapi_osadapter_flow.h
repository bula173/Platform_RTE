/**
 * @file sapi_osadapter_flow.h
 * @brief OSAdapter interface for OCORA PI-API Flow communication.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_FLOW_H
#define SAFEAPI_OSADAPTER_FLOW_H

#include <stddef.h>
#include "safeapi/oal/flow/sapi_flow.h"
#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter Flow operations vtable.
 */
typedef struct sapi_osadapter_flow_s
{
    sapi_status_t (*open)(sapi_flow_storage_t *storage,
                           const sapi_flow_config_t *config,
                           sapi_flow_handle_t *out_handle);
    sapi_status_t (*send)(sapi_flow_handle_t handle, const void *data, size_t data_size,
                           sapi_flow_channel_t channel, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*receive)(sapi_flow_handle_t handle, void *out_data, size_t buffer_size,
                              sapi_flow_channel_t *out_channel, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*close)(sapi_flow_handle_t handle);
    sapi_status_t (*getattr)(sapi_flow_handle_t handle, sapi_flow_attr_t *out_attr);
    sapi_status_t (*setattr)(sapi_flow_handle_t handle, const sapi_flow_attr_t *new_attr,
                              sapi_flow_attr_t *out_old_attr);
} sapi_osadapter_flow_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_flow_t sapi_flow_backend_t;

/**
 * @brief Registers the OSAdapter Flow implementation.
 * @param adapter Pointer to flow operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_flow_register(const sapi_osadapter_flow_t *adapter);

sapi_status_t sapi_flow_register_backend(const sapi_flow_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_FLOW_H */
