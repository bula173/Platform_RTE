/* Tests for the sapi_netlink validate-then-dispatch API (ADR-005): see
 * tests/timer/test_sapi_timer.c for the pattern this follows. */
#include <assert.h>
#include "safeapi/netlink/sapi_netlink.h"

static int g_mock_open_calls = 0;

static sapi_status_t mock_open(sapi_netlink_storage_t *storage,
                                const sapi_netlink_config_t *config,
                                sapi_netlink_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    g_mock_open_calls++;
    *out_handle = (sapi_netlink_handle_t)(void *)1; /* arbitrary non-NULL sentinel */
    return SAPI_STATUS_OK;
}

static const sapi_netlink_backend_t g_mock_backend_full __attribute__((unused)) = {
    mock_open, NULL, NULL, NULL
};

static const sapi_netlink_backend_t g_mock_backend_no_open __attribute__((unused)) = {
    NULL, NULL, NULL, NULL
};

int main(void)
{
    sapi_netlink_storage_t storage __attribute__((unused));
    sapi_netlink_handle_t handle __attribute__((unused)) = NULL;
    sapi_netlink_config_t connect_config __attribute__((unused)) = {0};
    sapi_netlink_config_t listen_config __attribute__((unused)) = {0};

    /* Null-parameter rejection happens before any backend is consulted. */
    assert(sapi_netlink_open(NULL, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* CONNECT role requires a non-NULL host even before a backend exists. */
    connect_config.role = SAPI_NETLINK_ROLE_CONNECT;
    connect_config.host = NULL;
    connect_config.port = 9000U;
    connect_config.message_size = 4U;
    connect_config.connect_timeout_ms = 100U;
    assert(sapi_netlink_open(&storage, &connect_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    connect_config.host = "127.0.0.1";
    listen_config.role = SAPI_NETLINK_ROLE_LISTEN;
    listen_config.host = NULL; /* NULL is valid for LISTEN (bind to "any") */
    listen_config.port = 9000U;
    listen_config.message_size = 4U;
    listen_config.connect_timeout_ms = 100U;

    /* No backend registered yet: valid params, but nothing to dispatch to. */
    assert(sapi_netlink_open(&storage, &connect_config, &handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_netlink_open(&storage, &listen_config, &handle) == SAPI_STATUS_NOT_INITIALIZED);

    /* Registering NULL is rejected. */
    assert(sapi_netlink_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* A backend with a NULL open slot yields NOT_SUPPORTED. */
    assert(sapi_netlink_register_backend(&g_mock_backend_no_open) == SAPI_STATUS_OK);
    assert(sapi_netlink_open(&storage, &connect_config, &handle) == SAPI_STATUS_NOT_SUPPORTED);

    /* send/receive/close with no send/receive/close slot -> NOT_SUPPORTED
     * (using a sentinel handle here since open() didn't produce a real one
     * with this backend - the dispatch layer doesn't dereference it). */
    handle = (sapi_netlink_handle_t)(void *)1;
    assert(sapi_netlink_send(handle, "x", 1U, 10U) == SAPI_STATUS_NOT_SUPPORTED);

    /* A fully-populated backend is actually reached, with the same
     * already-validated arguments the caller passed in. */
    assert(sapi_netlink_register_backend(&g_mock_backend_full) == SAPI_STATUS_OK);
    assert(sapi_netlink_open(&storage, &connect_config, &handle) == SAPI_STATUS_OK);
    assert(g_mock_open_calls == 1);
    assert(handle != NULL);

    return 0;
}
