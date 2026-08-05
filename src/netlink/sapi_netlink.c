/**
 * @file sapi_netlink.c
 * @brief Network link service: validates parameters, then dispatches to
 *        the backend registered via sapi_netlink_register_backend()
 *        (ADR-005). See sapi_ipc.c for the pattern this follows.
 */
#include "safeapi/netlink/sapi_netlink.h"

static const sapi_netlink_backend_t *s_backend = NULL;

sapi_status_t sapi_netlink_register_backend(const sapi_netlink_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_netlink_open(sapi_netlink_storage_t *storage,
                                 const sapi_netlink_config_t *config,
                                 sapi_netlink_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (config->message_size == 0U)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->role == SAPI_NETLINK_ROLE_CONNECT) && (config->host == NULL))
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

sapi_status_t sapi_netlink_send(sapi_netlink_handle_t handle,
                                 const void *message,
                                 size_t message_size,
                                 sapi_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (message == NULL) || (message_size == 0U))
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
    return s_backend->send(handle, message, message_size, timeout_ms);
}

sapi_status_t sapi_netlink_receive(sapi_netlink_handle_t handle,
                                    void *out_message,
                                    size_t buffer_size,
                                    sapi_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (out_message == NULL) || (buffer_size == 0U))
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
    return s_backend->receive(handle, out_message, buffer_size, timeout_ms);
}

sapi_status_t sapi_netlink_close(sapi_netlink_handle_t handle)
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
