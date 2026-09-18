/**
 * @file sapi_flow.c
 * @ingroup FLOW
 * @brief Flow service: validates parameters, then dispatches to the
 *        backend registered via sapi_flow_register_backend() (ADR-005).
 *        See sapi_netlink.c for the pattern this follows.
 */
#include "safeapi/oal/flow/sapi_flow.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi_backend/flow/sapi_flow_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_flow_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */
static bool oflags_select_exactly_one_role(uint32_t oflags);

/** Local functions */
/** @brief Exactly one of PUBLISHER/SUBSCRIBER/REQUESTER/RESPONDER must be
 *  set - a Flow has one, unambiguous role for its whole lifetime.
 *  @param oflags The sapi_flow_oflag_t bitmask to check.
 *  @return true if exactly one role bit is set; false otherwise. */
static bool oflags_select_exactly_one_role(uint32_t oflags)
{
    uint32_t role_bits = oflags & ((uint32_t)SAPI_FLOW_O_REQUESTER | (uint32_t)SAPI_FLOW_O_RESPONDER |
                                    (uint32_t)SAPI_FLOW_O_PUBLISHER | (uint32_t)SAPI_FLOW_O_SUBSCRIBER);

    return (role_bits != 0U) && ((role_bits & (role_bits - 1U)) == 0U);
}

/** Global functions */
sapi_status_t sapi_flow_register_backend(const sapi_flow_backend_t *backend)
{
    sapi_status_t lifecycle_status;

    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_flow_open(sapi_flow_storage_t *storage,
                              const sapi_flow_config_t *config,
                              sapi_flow_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->name == NULL) || (config->message_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!oflags_select_exactly_one_role(config->oflags))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->open == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->open(storage, config, out_handle);
}

sapi_status_t sapi_flow_send(sapi_flow_handle_t handle,
                              const void *data,
                              size_t data_size,
                              sapi_flow_channel_t channel,
                              sapi_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (data == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((channel != SAPI_FLOW_CHANNEL_USER) && (channel != SAPI_FLOW_CHANNEL_CTRL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->send == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->send(handle, data, data_size, channel, timeout_ms);
}

sapi_status_t sapi_flow_receive(sapi_flow_handle_t handle,
                                 void *out_data,
                                 size_t buffer_size,
                                 sapi_flow_channel_t *out_channel,
                                 sapi_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (out_data == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->receive == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->receive(handle, out_data, buffer_size, out_channel, timeout_ms);
}

sapi_status_t sapi_flow_close(sapi_flow_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->close == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->close(handle);
}

sapi_status_t sapi_flow_getattr(sapi_flow_handle_t handle, sapi_flow_attr_t *out_attr)
{
    if ((handle == NULL) || (out_attr == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->getattr == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->getattr(handle, out_attr);
}

sapi_status_t sapi_flow_setattr(sapi_flow_handle_t handle,
                                 const sapi_flow_attr_t *new_attr,
                                 sapi_flow_attr_t *out_old_attr)
{
    if ((handle == NULL) || (new_attr == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->setattr == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->setattr(handle, new_attr, out_old_attr);
}
