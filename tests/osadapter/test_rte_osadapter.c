/**
 * @file test_rte_osadapter.c
 * @brief Unit tests for OSAdapter and ProtocolAdapters (UDP, TCP).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rte_osadapter/rte_osadapter.h"
#include "rte/oal/protocol/rte_protocol_adapter.h"

static int s_mock_open_called = 0;
static int s_mock_bind_called = 0;
static int s_mock_connect_called = 0;
static int s_mock_send_called = 0;
static int s_mock_recv_called = 0;
static int s_mock_poll_called = 0;
static int s_mock_close_called = 0;

static rte_status_t mock_open(rte_os_socket_type_t type, rte_os_socket_handle_t *out_handle)
{
    (void)type;
    s_mock_open_called++;
    *out_handle = (rte_os_socket_handle_t)42;
    return RTE_STATUS_OK;
}

static rte_status_t mock_bind(rte_os_socket_handle_t handle, const char *host, uint16_t port)
{
    (void)handle;
    (void)host;
    (void)port;
    s_mock_bind_called++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_connect(rte_os_socket_handle_t handle, const char *host, uint16_t port)
{
    (void)handle;
    (void)host;
    (void)port;
    s_mock_connect_called++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_send(rte_os_socket_handle_t handle, const void *buf, size_t len, size_t *out_sent)
{
    (void)handle;
    (void)buf;
    s_mock_send_called++;
    if (out_sent != NULL) { *out_sent = len; }
    return RTE_STATUS_OK;
}

static rte_status_t mock_sendto(rte_os_socket_handle_t handle, const void *buf, size_t len,
                                 const char *host, uint16_t port, size_t *out_sent)
{
    (void)handle;
    (void)buf;
    (void)host;
    (void)port;
    s_mock_send_called++;
    if (out_sent != NULL) { *out_sent = len; }
    return RTE_STATUS_OK;
}

static rte_status_t mock_recv(rte_os_socket_handle_t handle, void *buf, size_t len, size_t *out_recv)
{
    (void)handle;
    s_mock_recv_called++;
    if (len > 0U)
    {
        ((char *)buf)[0] = 'X';
    }
    if (out_recv != NULL) { *out_recv = 1U; }
    return RTE_STATUS_OK;
}

static rte_status_t mock_recvfrom(rte_os_socket_handle_t handle, void *buf, size_t len,
                                   char *out_host, size_t host_len, uint16_t *out_port, size_t *out_recv)
{
    (void)handle;
    (void)out_host;
    (void)host_len;
    (void)out_port;
    s_mock_recv_called++;
    if (len > 0U)
    {
        ((char *)buf)[0] = 'Y';
    }
    if (out_recv != NULL) { *out_recv = 1U; }
    return RTE_STATUS_OK;
}

static rte_status_t mock_poll(rte_os_socket_handle_t handle, uint32_t events,
                               rte_duration_ms_t timeout_ms, uint32_t *out_revents)
{
    (void)handle;
    (void)timeout_ms;
    s_mock_poll_called++;
    *out_revents = events;
    return RTE_STATUS_OK;
}

static rte_status_t mock_set_nonblocking(rte_os_socket_handle_t handle, bool nonblocking)
{
    (void)handle;
    (void)nonblocking;
    return RTE_STATUS_OK;
}

static rte_status_t mock_close(rte_os_socket_handle_t handle)
{
    (void)handle;
    s_mock_close_called++;
    return RTE_STATUS_OK;
}

static const rte_os_socket_ops_t s_mock_ops = {
    mock_open,
    mock_bind,
    mock_connect,
    mock_send,
    mock_sendto,
    mock_recv,
    mock_recvfrom,
    mock_poll,
    mock_set_nonblocking,
    mock_close
};

static void test_osadapter_socket_registration(void)
{
    printf("test_osadapter_socket_registration...\n");
    assert(rte_osadapter_register_socket_ops(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_osadapter_register_socket_ops(&s_mock_ops) == RTE_STATUS_OK);
    assert(rte_osadapter_get_socket_ops() == &s_mock_ops);
}

static void test_protocol_adapter_udp(void)
{
    uint8_t storage[256];
    rte_protocol_config_t config;
    rte_protocol_handle_t handle = NULL;
    const rte_protocol_adapter_ops_t *udp_adapter = rte_protocol_adapter_get(RTE_PROTOCOL_TYPE_RAW_UDP);
    char buf[32];

    printf("test_protocol_adapter_udp...\n");
    assert(udp_adapter != NULL);

    memset(&config, 0, sizeof(config));
    config.protocol_type = RTE_PROTOCOL_TYPE_RAW_UDP;
    config.role = RTE_PROTOCOL_ROLE_CONNECT;
    snprintf(config.host, sizeof(config.host), "127.0.0.1");
    config.port = 15101;

    s_mock_open_called = 0;
    s_mock_connect_called = 0;
    s_mock_send_called = 0;
    s_mock_recv_called = 0;
    s_mock_close_called = 0;

    assert(udp_adapter->open(storage, sizeof(storage), &config, &s_mock_ops, &handle) == RTE_STATUS_OK);
    assert(handle != NULL);
    assert(s_mock_open_called == 1);
    assert(s_mock_connect_called == 1);

    assert(udp_adapter->send(handle, "hello", 5, 100) == RTE_STATUS_OK);
    assert(s_mock_send_called == 1);

    assert(udp_adapter->receive(handle, buf, sizeof(buf), 100) == RTE_STATUS_OK);
    assert(s_mock_recv_called == 1);
    assert(buf[0] == 'X');

    assert(udp_adapter->close(handle) == RTE_STATUS_OK);
    assert(s_mock_close_called == 1);
}

static void test_protocol_adapter_tcp(void)
{
    uint8_t storage[256];
    rte_protocol_config_t config;
    rte_protocol_handle_t handle = NULL;
    const rte_protocol_adapter_ops_t *tcp_adapter = rte_protocol_adapter_get(RTE_PROTOCOL_TYPE_RAW_TCP);
    char buf[32];

    printf("test_protocol_adapter_tcp...\n");
    assert(tcp_adapter != NULL);

    memset(&config, 0, sizeof(config));
    config.protocol_type = RTE_PROTOCOL_TYPE_RAW_TCP;
    config.role = RTE_PROTOCOL_ROLE_CONNECT;
    snprintf(config.host, sizeof(config.host), "127.0.0.1");
    config.port = 15102;

    s_mock_open_called = 0;
    s_mock_connect_called = 0;
    s_mock_send_called = 0;
    s_mock_recv_called = 0;
    s_mock_close_called = 0;

    assert(tcp_adapter->open(storage, sizeof(storage), &config, &s_mock_ops, &handle) == RTE_STATUS_OK);
    assert(handle != NULL);
    assert(s_mock_open_called == 1);
    assert(s_mock_connect_called == 1);

    assert(tcp_adapter->send(handle, "stream", 6, 100) == RTE_STATUS_OK);
    assert(s_mock_send_called == 1);

    assert(tcp_adapter->receive(handle, buf, sizeof(buf), 100) == RTE_STATUS_OK);
    assert(s_mock_recv_called == 1);
    assert(buf[0] == 'X');

    assert(tcp_adapter->close(handle) == RTE_STATUS_OK);
    assert(s_mock_close_called == 1);
}

int main(void)
{
    printf("Running test_rte_osadapter...\n");
    test_osadapter_socket_registration();
    test_protocol_adapter_udp();
    test_protocol_adapter_tcp();
    printf("All test_rte_osadapter tests passed!\n");
    return 0;
}
