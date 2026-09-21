/* Tests for the rte_netlink validate-then-dispatch API (ADR-005): see
 * tests/timer/test_rte_timer.c for the pattern this follows. */
#include <assert.h>
#include "rte/oal/netlink/rte_netlink.h"
#include "rte_osadapter/netlink/rte_osadapter_netlink.h"

static int g_mock_open_calls = 0;
static int g_mock_send_calls = 0;
static int g_mock_receive_calls = 0;
static int g_mock_close_calls = 0;

static rte_status_t mock_open(rte_netlink_storage_t *storage,
                                const rte_netlink_config_t *config,
                                rte_netlink_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    g_mock_open_calls++;
    *out_handle = (rte_netlink_handle_t)(void *)1; /* arbitrary non-NULL sentinel */
    return RTE_STATUS_OK;
}

static rte_status_t mock_send(rte_netlink_handle_t handle, const void *message,
                                size_t message_size, rte_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)message;
    (void)message_size;
    (void)timeout_ms;
    g_mock_send_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_receive(rte_netlink_handle_t handle, void *out_message,
                                   size_t buffer_size, rte_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)out_message;
    (void)buffer_size;
    (void)timeout_ms;
    g_mock_receive_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_close(rte_netlink_handle_t handle)
{
    (void)handle;
    g_mock_close_calls++;
    return RTE_STATUS_OK;
}

static const rte_osadapter_netlink_t g_mock_osadapter_full __attribute__((unused)) = {
    mock_open, mock_send, mock_receive, mock_close
};

static const rte_osadapter_netlink_t g_mock_osadapter_no_open __attribute__((unused)) = {
    NULL, NULL, NULL, NULL
};

int main(void)
{
    rte_netlink_storage_t storage __attribute__((unused));
    rte_netlink_handle_t handle __attribute__((unused)) = NULL;
    rte_netlink_config_t connect_config __attribute__((unused)) = {0};
    rte_netlink_config_t listen_config __attribute__((unused)) = {0};

    /* Null-parameter rejection happens before any OSAdapter is consulted. */
    assert(rte_netlink_open(NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);

    /* Invalid config (message_size == 0) rejected even with a valid role. */
    rte_netlink_config_t zero_size_config __attribute__((unused)) = {0};
    zero_size_config.role = RTE_NETLINK_ROLE_LISTEN;
    zero_size_config.host = NULL;
    zero_size_config.port = 9000U;
    zero_size_config.message_size = 0U;
    zero_size_config.connect_timeout_ms = 100U;
    assert(rte_netlink_open(&storage, &zero_size_config, &handle) == RTE_STATUS_INVALID_PARAM);

    /* CONNECT role requires a non-NULL host even before an OSAdapter exists. */
    connect_config.role = RTE_NETLINK_ROLE_CONNECT;
    connect_config.host = NULL;
    connect_config.port = 9000U;
    connect_config.message_size = 4U;
    connect_config.connect_timeout_ms = 100U;
    assert(rte_netlink_open(&storage, &connect_config, &handle) == RTE_STATUS_INVALID_PARAM);

    connect_config.host = "127.0.0.1";
    listen_config.role = RTE_NETLINK_ROLE_LISTEN;
    listen_config.host = NULL; /* NULL is valid for LISTEN (bind to "any") */
    listen_config.port = 9000U;
    listen_config.message_size = 4U;
    listen_config.connect_timeout_ms = 100U;

    /* No OSAdapter registered yet: valid params, but nothing to dispatch to. */
    assert(rte_netlink_open(&storage, &connect_config, &handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_netlink_open(&storage, &listen_config, &handle) == RTE_STATUS_NOT_INITIALIZED);

    /* Null-parameter rejection for send/receive/close happens before any
     * OSAdapter is consulted, independently of one another. */
    unsigned char buf[4] __attribute__((unused));
    assert(rte_netlink_send(NULL, "x", 1U, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_netlink_send((rte_netlink_handle_t)(void *)1, NULL, 1U, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_netlink_send((rte_netlink_handle_t)(void *)1, "x", 0U, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_netlink_receive(NULL, buf, sizeof(buf), 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_netlink_receive((rte_netlink_handle_t)(void *)1, NULL, sizeof(buf), 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_netlink_receive((rte_netlink_handle_t)(void *)1, buf, 0U, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_netlink_close(NULL) == RTE_STATUS_INVALID_PARAM);

    /* No OSAdapter registered yet: valid params, but nothing to dispatch to,
     * for every remaining entry point. */
    rte_netlink_handle_t dummy_handle = (rte_netlink_handle_t)(void *)1;
    assert(rte_netlink_send(dummy_handle, "x", 1U, 10U) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_netlink_receive(dummy_handle, buf, sizeof(buf), 10U) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_netlink_close(dummy_handle) == RTE_STATUS_NOT_INITIALIZED);

    /* Registering NULL is rejected. */
    assert(rte_osadapter_netlink_register(NULL) == RTE_STATUS_INVALID_PARAM);

    /* An OSAdapter with a NULL open slot yields NOT_SUPPORTED. */
    assert(rte_osadapter_netlink_register(&g_mock_osadapter_no_open) == RTE_STATUS_OK);
    assert(rte_netlink_open(&storage, &connect_config, &handle) == RTE_STATUS_NOT_SUPPORTED);

    /* send/receive/close with no send/receive/close slot -> NOT_SUPPORTED
     * (using a sentinel handle here since open() didn't produce a real one
     * with this OSAdapter - the dispatch layer doesn't dereference it). */
    handle = (rte_netlink_handle_t)(void *)1;
    assert(rte_netlink_send(handle, "x", 1U, 10U) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_netlink_receive(handle, buf, sizeof(buf), 10U) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_netlink_close(handle) == RTE_STATUS_NOT_SUPPORTED);

    /* A fully-populated OSAdapter is actually reached for every entry
     * point, with the same already-validated arguments the caller
     * passed in. */
    assert(rte_osadapter_netlink_register(&g_mock_osadapter_full) == RTE_STATUS_OK);
    assert(rte_netlink_open(&storage, &connect_config, &handle) == RTE_STATUS_OK);
    assert(g_mock_open_calls == 1);
    assert(handle != NULL);

    assert(rte_netlink_send(handle, "x", 1U, 10U) == RTE_STATUS_OK);
    assert(g_mock_send_calls == 1);

    assert(rte_netlink_receive(handle, buf, sizeof(buf), 10U) == RTE_STATUS_OK);
    assert(g_mock_receive_calls == 1);

    assert(rte_netlink_close(handle) == RTE_STATUS_OK);
    assert(g_mock_close_calls == 1);

    return 0;
}
