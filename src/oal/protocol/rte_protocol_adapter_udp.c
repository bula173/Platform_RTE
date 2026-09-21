/**
 * @file rte_protocol_adapter_udp.c
 * @brief Raw UDP ProtocolAdapter Implementation using OSAdapter Sockets.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 * Strictly no dynamic allocation. All OS interactions go through
 * rte_os_socket_ops_t.
 */

#include "safeapi/oal/protocol/rte_protocol_adapter.h"
#include "safeapi/oal/memory/rte_mem_util.h"

typedef struct rte_protocol_udp_state_s
{
    rte_os_socket_handle_t     sock;
    const rte_os_socket_ops_t *os_sockets;
    rte_protocol_config_t      config;
    bool                        is_connected;
    bool                        peer_known;
    char                        peer_host[RTE_PROTOCOL_ENDPOINT_STR_MAX];
    uint16_t                    peer_port;
} rte_protocol_udp_state_t;

static rte_status_t udp_adapter_open(void *storage, size_t storage_size,
                                      const rte_protocol_config_t *config,
                                      const rte_os_socket_ops_t *os_sockets,
                                      rte_protocol_handle_t *out_handle)
{
    rte_protocol_udp_state_t *state = NULL;
    const rte_os_socket_ops_t *ops = os_sockets;
    rte_status_t status;
    rte_os_socket_handle_t sock = RTE_OS_SOCKET_INVALID_HANDLE;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (storage_size < sizeof(rte_protocol_udp_state_t))
    {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }

    if (ops == NULL)
    {
        ops = rte_osadapter_get_socket_ops();
    }
    if ((ops == NULL) || (ops->open == NULL) || (ops->close == NULL))
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }

    state = (rte_protocol_udp_state_t *)storage;
    rte_mem_set(state, 0, sizeof(*state));
    state->sock = RTE_OS_SOCKET_INVALID_HANDLE;
    state->os_sockets = ops;
    state->config = *config;

    status = ops->open(RTE_OS_SOCKET_TYPE_UDP, &sock);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    state->sock = sock;

    if (ops->set_nonblocking != NULL)
    {
        (void)ops->set_nonblocking(sock, true);
    }

    if (config->role == RTE_PROTOCOL_ROLE_LISTEN)
    {
        if (ops->bind == NULL)
        {
            (void)ops->close(sock);
            state->sock = RTE_OS_SOCKET_INVALID_HANDLE;
            return RTE_STATUS_NOT_SUPPORTED;
        }
        status = ops->bind(sock, config->host[0] != '\0' ? config->host : NULL, config->port);
        if (status != RTE_STATUS_OK)
        {
            (void)ops->close(sock);
            state->sock = RTE_OS_SOCKET_INVALID_HANDLE;
            return status;
        }
    }
    else if (config->role == RTE_PROTOCOL_ROLE_CONNECT)
    {
        if (ops->connect != NULL)
        {
            status = ops->connect(sock, config->host, config->port);
            if (status == RTE_STATUS_OK)
            {
                state->is_connected = true;
            }
        }
        /* Cache destination for sendto fallback */
        rte_mem_copy(state->peer_host, config->host, sizeof(state->peer_host));
        state->peer_port = config->port;
        state->peer_known = true;
    }
    else
    {
        /* Peer endpoint specified in config */
        rte_mem_copy(state->peer_host, config->host, sizeof(state->peer_host));
        state->peer_port = config->port;
        state->peer_known = (config->port > 0U);
    }

    *out_handle = (rte_protocol_handle_t)(void *)state;
    return RTE_STATUS_OK;
}

static rte_status_t udp_adapter_send(rte_protocol_handle_t handle, const void *data,
                                      size_t size, rte_duration_ms_t timeout_ms)
{
    rte_protocol_udp_state_t *state = (rte_protocol_udp_state_t *)(void *)handle;
    const rte_os_socket_ops_t *ops;
    rte_status_t status;
    uint32_t revents = 0U;
    size_t sent = 0U;

    if ((state == NULL) || (data == NULL) || (size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (state->sock == RTE_OS_SOCKET_INVALID_HANDLE)
    {
        return RTE_STATUS_INVALID_STATE;
    }

    ops = state->os_sockets;
    if (ops == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }

    if (ops->poll != NULL)
    {
        status = ops->poll(state->sock, RTE_OS_SOCKET_EVENT_WRITE, timeout_ms, &revents);
        if (status != RTE_STATUS_OK)
        {
            return status;
        }
        if ((revents & RTE_OS_SOCKET_EVENT_WRITE) == 0U)
        {
            return RTE_STATUS_TIMEOUT;
        }
    }

    if (state->is_connected && (ops->send != NULL))
    {
        return ops->send(state->sock, data, size, &sent);
    }
    else if (state->peer_known && (ops->sendto != NULL))
    {
        return ops->sendto(state->sock, data, size, state->peer_host, state->peer_port, &sent);
    }
    else
    {
        return RTE_STATUS_INVALID_STATE;
    }
}

static rte_status_t udp_adapter_receive(rte_protocol_handle_t handle, void *out_buf,
                                         size_t buffer_size, rte_duration_ms_t timeout_ms)
{
    rte_protocol_udp_state_t *state = (rte_protocol_udp_state_t *)(void *)handle;
    const rte_os_socket_ops_t *ops;
    rte_status_t status;
    uint32_t revents = 0U;
    size_t recvd = 0U;

    if ((state == NULL) || (out_buf == NULL) || (buffer_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (state->sock == RTE_OS_SOCKET_INVALID_HANDLE)
    {
        return RTE_STATUS_INVALID_STATE;
    }

    ops = state->os_sockets;
    if (ops == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }

    if (ops->poll != NULL)
    {
        status = ops->poll(state->sock, RTE_OS_SOCKET_EVENT_READ, timeout_ms, &revents);
        if (status != RTE_STATUS_OK)
        {
            return status;
        }
        if ((revents & RTE_OS_SOCKET_EVENT_READ) == 0U)
        {
            return RTE_STATUS_TIMEOUT;
        }
    }

    if (state->is_connected && (ops->recv != NULL))
    {
        return ops->recv(state->sock, out_buf, buffer_size, &recvd);
    }
    else if (ops->recvfrom != NULL)
    {
        status = ops->recvfrom(state->sock, out_buf, buffer_size,
                               state->peer_host, sizeof(state->peer_host),
                               &state->peer_port, &recvd);
        if (status == RTE_STATUS_OK)
        {
            state->peer_known = true;
        }
        return status;
    }
    else
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
}

static rte_status_t udp_adapter_close(rte_protocol_handle_t handle)
{
    rte_protocol_udp_state_t *state = (rte_protocol_udp_state_t *)(void *)handle;
    if ((state == NULL) || (state->sock == RTE_OS_SOCKET_INVALID_HANDLE))
    {
        return RTE_STATUS_OK;
    }
    if ((state->os_sockets != NULL) && (state->os_sockets->close != NULL))
    {
        (void)state->os_sockets->close(state->sock);
    }
    state->sock = RTE_OS_SOCKET_INVALID_HANDLE;
    state->is_connected = false;
    return RTE_STATUS_OK;
}

static const rte_protocol_adapter_ops_t s_udp_ops = {
    udp_adapter_open,
    udp_adapter_send,
    udp_adapter_receive,
    udp_adapter_close
};

const rte_protocol_adapter_ops_t *rte_protocol_adapter_get_udp(void)
{
    return &s_udp_ops;
}
