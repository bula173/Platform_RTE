/**
 * @file sapi_protocol_adapter_tcp.c
 * @brief Raw TCP ProtocolAdapter Implementation using OSAdapter Sockets.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 * Strictly no dynamic allocation. All OS interactions go through
 * sapi_os_socket_ops_t.
 */

#include "safeapi/oal/protocol/sapi_protocol_adapter.h"
#include "safeapi/oal/memory/sapi_mem_util.h"

typedef struct sapi_protocol_tcp_state_s
{
    sapi_os_socket_handle_t     sock;
    const sapi_os_socket_ops_t *os_sockets;
    sapi_protocol_config_t      config;
    bool                        is_connected;
} sapi_protocol_tcp_state_t;

static sapi_status_t tcp_adapter_open(void *storage, size_t storage_size,
                                      const sapi_protocol_config_t *config,
                                      const sapi_os_socket_ops_t *os_sockets,
                                      sapi_protocol_handle_t *out_handle)
{
    sapi_protocol_tcp_state_t *state = NULL;
    const sapi_os_socket_ops_t *ops = os_sockets;
    sapi_status_t status;
    sapi_os_socket_handle_t sock = SAPI_OS_SOCKET_INVALID_HANDLE;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (storage_size < sizeof(sapi_protocol_tcp_state_t))
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    if (ops == NULL)
    {
        ops = sapi_osadapter_get_socket_ops();
    }
    if ((ops == NULL) || (ops->open == NULL) || (ops->close == NULL))
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }

    state = (sapi_protocol_tcp_state_t *)storage;
    sapi_mem_set(state, 0, sizeof(*state));
    state->sock = SAPI_OS_SOCKET_INVALID_HANDLE;
    state->os_sockets = ops;
    state->config = *config;

    status = ops->open(SAPI_OS_SOCKET_TYPE_TCP, &sock);
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    state->sock = sock;

    if (ops->set_nonblocking != NULL)
    {
        (void)ops->set_nonblocking(sock, true);
    }

    if (config->role == SAPI_PROTOCOL_ROLE_LISTEN)
    {
        if (ops->bind == NULL)
        {
            (void)ops->close(sock);
            state->sock = SAPI_OS_SOCKET_INVALID_HANDLE;
            return SAPI_STATUS_NOT_SUPPORTED;
        }
        status = ops->bind(sock, config->host[0] != '\0' ? config->host : NULL, config->port);
        if (status != SAPI_STATUS_OK)
        {
            (void)ops->close(sock);
            state->sock = SAPI_OS_SOCKET_INVALID_HANDLE;
            return status;
        }
    }
    else if (config->role == SAPI_PROTOCOL_ROLE_CONNECT)
    {
        if (ops->connect == NULL)
        {
            (void)ops->close(sock);
            state->sock = SAPI_OS_SOCKET_INVALID_HANDLE;
            return SAPI_STATUS_NOT_SUPPORTED;
        }
        status = ops->connect(sock, config->host, config->port);
        if (status == SAPI_STATUS_OK)
        {
            state->is_connected = true;
        }
    }

    *out_handle = (sapi_protocol_handle_t)(void *)state;
    return SAPI_STATUS_OK;
}

static sapi_status_t tcp_adapter_send(sapi_protocol_handle_t handle, const void *data,
                                      size_t size, sapi_duration_ms_t timeout_ms)
{
    sapi_protocol_tcp_state_t *state = (sapi_protocol_tcp_state_t *)(void *)handle;
    const sapi_os_socket_ops_t *ops;
    sapi_status_t status;
    uint32_t revents = 0U;
    size_t sent = 0U;

    if ((state == NULL) || (data == NULL) || (size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (state->sock == SAPI_OS_SOCKET_INVALID_HANDLE)
    {
        return SAPI_STATUS_INVALID_STATE;
    }

    ops = state->os_sockets;
    if ((ops == NULL) || (ops->send == NULL))
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }

    if (ops->poll != NULL)
    {
        status = ops->poll(state->sock, SAPI_OS_SOCKET_EVENT_WRITE, timeout_ms, &revents);
        if (status != SAPI_STATUS_OK)
        {
            return status;
        }
        if ((revents & SAPI_OS_SOCKET_EVENT_WRITE) == 0U)
        {
            return SAPI_STATUS_TIMEOUT;
        }
    }

    return ops->send(state->sock, data, size, &sent);
}

static sapi_status_t tcp_adapter_receive(sapi_protocol_handle_t handle, void *out_buf,
                                         size_t buffer_size, sapi_duration_ms_t timeout_ms)
{
    sapi_protocol_tcp_state_t *state = (sapi_protocol_tcp_state_t *)(void *)handle;
    const sapi_os_socket_ops_t *ops;
    sapi_status_t status;
    uint32_t revents = 0U;
    size_t recvd = 0U;

    if ((state == NULL) || (out_buf == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (state->sock == SAPI_OS_SOCKET_INVALID_HANDLE)
    {
        return SAPI_STATUS_INVALID_STATE;
    }

    ops = state->os_sockets;
    if ((ops == NULL) || (ops->recv == NULL))
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }

    if (ops->poll != NULL)
    {
        status = ops->poll(state->sock, SAPI_OS_SOCKET_EVENT_READ, timeout_ms, &revents);
        if (status != SAPI_STATUS_OK)
        {
            return status;
        }
        if ((revents & SAPI_OS_SOCKET_EVENT_READ) == 0U)
        {
            return SAPI_STATUS_TIMEOUT;
        }
    }

    return ops->recv(state->sock, out_buf, buffer_size, &recvd);
}

static sapi_status_t tcp_adapter_close(sapi_protocol_handle_t handle)
{
    sapi_protocol_tcp_state_t *state = (sapi_protocol_tcp_state_t *)(void *)handle;
    if ((state == NULL) || (state->sock == SAPI_OS_SOCKET_INVALID_HANDLE))
    {
        return SAPI_STATUS_OK;
    }
    if ((state->os_sockets != NULL) && (state->os_sockets->close != NULL))
    {
        (void)state->os_sockets->close(state->sock);
    }
    state->sock = SAPI_OS_SOCKET_INVALID_HANDLE;
    state->is_connected = false;
    return SAPI_STATUS_OK;
}

static const sapi_protocol_adapter_ops_t s_tcp_ops = {
    tcp_adapter_open,
    tcp_adapter_send,
    tcp_adapter_receive,
    tcp_adapter_close
};

const sapi_protocol_adapter_ops_t *sapi_protocol_adapter_get_tcp(void)
{
    return &s_tcp_ops;
}
