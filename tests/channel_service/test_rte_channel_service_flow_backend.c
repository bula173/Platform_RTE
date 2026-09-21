/** @file test_rte_channel_service_flow_backend.c
 *  @brief Unit tests for rte_channel_service_flow_backend (mocks rte_flow_backend_t
 *         one layer down, same pattern tests/flow/test_rte_flow.c uses).
 */
#include <assert.h>
#include <string.h>

#include "safeapi/redundancy/channel_service/rte_channel_service_flow_backend.h"
#include "safeapi_backend/flow/rte_flow_backend.h"

static uint32_t g_last_open_oflags;
static size_t   g_last_open_message_size;
static char     g_last_open_name[64];
static int      g_open_calls;
static int      g_send_calls;
static int      g_receive_calls;
static int      g_close_calls;
static uint8_t  g_last_sent_byte;
static bool     g_open_should_timeout;

static rte_status_t mock_open(rte_flow_storage_t *storage, const rte_flow_config_t *config,
                                rte_flow_handle_t *out_handle)
{
    (void)storage;
    g_open_calls++;
    g_last_open_oflags = config->oflags;
    g_last_open_message_size = config->message_size;
    (void)strncpy(g_last_open_name, config->name, sizeof(g_last_open_name) - 1U);
    if (g_open_should_timeout)
    {
        return RTE_STATUS_TIMEOUT;
    }
    *out_handle = (rte_flow_handle_t)(void *)1;
    return RTE_STATUS_OK;
}

static rte_status_t mock_send(rte_flow_handle_t handle, const void *data, size_t data_size,
                                rte_flow_channel_t channel, rte_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)channel;
    (void)timeout_ms;
    assert(data_size == 1U);
    g_last_sent_byte = *(const uint8_t *)data;
    g_send_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_receive(rte_flow_handle_t handle, void *out_data, size_t buffer_size,
                                   rte_flow_channel_t *out_channel, rte_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)buffer_size;
    (void)timeout_ms;
    if (out_channel != NULL)
    {
        *out_channel = RTE_FLOW_CHANNEL_USER;
    }
    *(uint8_t *)out_data = 0x42U;
    g_receive_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_close(rte_flow_handle_t handle)
{
    (void)handle;
    g_close_calls++;
    return RTE_STATUS_OK;
}

static const rte_flow_backend_t g_mock_flow_backend = {
    mock_open, mock_send, mock_receive, mock_close, NULL, NULL
};

static rte_status_t resolver_connect(const char *channel_name, rte_netlink_config_t *out_config, void *context)
{
    (void)context;
    assert(strcmp(channel_name, "ab-peer") == 0);
    out_config->role = RTE_NETLINK_ROLE_CONNECT;
    out_config->host = "127.0.0.1";
    out_config->port = 15001U;
    out_config->message_size = 96U;
    out_config->connect_timeout_ms = 500U;
    return RTE_STATUS_OK;
}

static rte_status_t resolver_listen(const char *channel_name, rte_netlink_config_t *out_config, void *context)
{
    (void)context;
    (void)channel_name;
    out_config->role = RTE_NETLINK_ROLE_LISTEN;
    out_config->host = NULL;
    out_config->port = 15001U;
    out_config->message_size = 96U;
    out_config->connect_timeout_ms = 500U;
    return RTE_STATUS_OK;
}

static rte_status_t resolver_unknown(const char *channel_name, rte_netlink_config_t *out_config, void *context)
{
    (void)context;
    (void)channel_name;
    (void)out_config;
    return RTE_STATUS_INVALID_PARAM;
}

static void test_connect_role_maps_to_publisher(void)
{
    rte_channel_service_storage_t storage;
    const rte_channel_service_backend_t *backend = rte_channel_service_flow_backend();

    assert(rte_channel_service_flow_backend_register_resolver(resolver_connect, NULL) == RTE_STATUS_OK);
    g_open_calls = 0;
    assert(backend->setup(&storage, "ab-peer") == RTE_STATUS_OK);
    assert(g_open_calls == 0); /* lazy - setup() itself never opens, see below */
    {
        uint8_t byte = 0U;
        assert(backend->send(&storage, &byte, 1U, 100U) == RTE_STATUS_OK);
    }
    assert(g_open_calls == 1);
    assert(g_last_open_oflags == (uint32_t)RTE_FLOW_O_PUBLISHER);
    assert(g_last_open_message_size == 96U);
    assert(strcmp(g_last_open_name, "ab-peer") == 0);
    (void)backend->close(&storage);
}

static void test_listen_role_maps_to_subscriber(void)
{
    rte_channel_service_storage_t storage;
    const rte_channel_service_backend_t *backend = rte_channel_service_flow_backend();
    uint8_t byte = 0U;

    assert(rte_channel_service_flow_backend_register_resolver(resolver_listen, NULL) == RTE_STATUS_OK);
    g_open_calls = 0;
    assert(backend->setup(&storage, "ab-negotiate") == RTE_STATUS_OK);
    assert(g_open_calls == 0);
    assert(backend->send(&storage, &byte, 1U, 100U) == RTE_STATUS_OK);
    assert(g_open_calls == 1);
    assert(g_last_open_oflags == (uint32_t)RTE_FLOW_O_SUBSCRIBER);
    (void)backend->close(&storage);
}

/** The exact scenario this design fixes, found live against a real GP
 *  process (see root TODO.md's Phase 4 entry): a channel whose peer is
 *  never present must not fail setup() at all - only individual
 *  read()/send() calls see the failure, and setup()'s own bounded
 *  open_timeout_ms=0 (this file's own header doc) never blocks. */
static void test_absent_peer_fails_only_io_not_setup(void)
{
    rte_channel_service_storage_t storage;
    const rte_channel_service_backend_t *backend = rte_channel_service_flow_backend();
    uint8_t byte = 0U;

    assert(rte_channel_service_flow_backend_register_resolver(resolver_connect, NULL) == RTE_STATUS_OK);
    g_open_calls = 0;
    assert(backend->setup(&storage, "ab-peer") == RTE_STATUS_OK);
    assert(g_open_calls == 0);

    g_open_should_timeout = true;
    assert(backend->send(&storage, &byte, 1U, 100U) == RTE_STATUS_TIMEOUT);
    assert(backend->read(&storage, &byte, 1U, 100U) == RTE_STATUS_TIMEOUT);
    assert(g_open_calls == 2); /* retried on each call, never cached as a permanent failure */

    g_open_should_timeout = false;
    g_send_calls = 0;
    assert(backend->send(&storage, &byte, 1U, 100U) == RTE_STATUS_OK);
    assert(g_send_calls == 1);

    (void)backend->close(&storage);
}

static void test_send_and_receive_delegate_to_flow(void)
{
    rte_channel_service_storage_t storage;
    const rte_channel_service_backend_t *backend = rte_channel_service_flow_backend();
    uint8_t out_byte = 0U;
    uint8_t send_byte = 0x7AU;

    assert(rte_channel_service_flow_backend_register_resolver(resolver_connect, NULL) == RTE_STATUS_OK);
    assert(backend->setup(&storage, "ab-peer") == RTE_STATUS_OK);

    g_send_calls = 0;
    assert(backend->send(&storage, &send_byte, 1U, 100U) == RTE_STATUS_OK);
    assert(g_send_calls == 1);
    assert(g_last_sent_byte == 0x7AU);

    g_receive_calls = 0;
    assert(backend->read(&storage, &out_byte, 1U, 100U) == RTE_STATUS_OK);
    assert(g_receive_calls == 1);
    assert(out_byte == 0x42U);

    g_close_calls = 0;
    assert(backend->close(&storage) == RTE_STATUS_OK);
    assert(g_close_calls == 1);
}

static void test_read_before_setup_rejected(void)
{
    rte_channel_service_storage_t storage;
    const rte_channel_service_backend_t *backend = rte_channel_service_flow_backend();
    uint8_t out_byte = 0U;

    memset(&storage, 0, sizeof(storage));
    assert(backend->read(&storage, &out_byte, 1U, 100U) == RTE_STATUS_NOT_INITIALIZED);
    assert(backend->close(&storage) == RTE_STATUS_OK);
}

static void test_resolver_failure_propagates(void)
{
    rte_channel_service_storage_t storage;
    const rte_channel_service_backend_t *backend = rte_channel_service_flow_backend();

    assert(rte_channel_service_flow_backend_register_resolver(resolver_unknown, NULL) == RTE_STATUS_OK);
    assert(backend->setup(&storage, "does-not-exist") == RTE_STATUS_INVALID_PARAM);
}

static void test_null_params_rejected(void)
{
    rte_channel_service_storage_t storage;
    const rte_channel_service_backend_t *backend = rte_channel_service_flow_backend();

    assert(rte_channel_service_flow_backend_register_resolver(NULL, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(backend->setup(NULL, "ab-peer") == RTE_STATUS_INVALID_PARAM);
    assert(backend->setup(&storage, NULL) == RTE_STATUS_INVALID_PARAM);
}

int main(void)
{
    assert(rte_flow_register_backend(&g_mock_flow_backend) == RTE_STATUS_OK);
    test_connect_role_maps_to_publisher();
    test_listen_role_maps_to_subscriber();
    test_absent_peer_fails_only_io_not_setup();
    test_send_and_receive_delegate_to_flow();
    test_read_before_setup_rejected();
    test_resolver_failure_propagates();
    test_null_params_rejected();
    return 0;
}
