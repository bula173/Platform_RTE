/* Tests for the rte_flow validate-then-dispatch API (OCORA PI-API
 * compatibility, ADR-005 OSAdapter seam): see tests/netlink/test_rte_netlink.c
 * for the pattern this follows. */
#include <assert.h>
#include "safeapi/oal/flow/rte_flow.h"
#include "safeapi_osadapter/flow/rte_osadapter_flow.h"

static int g_mock_open_calls = 0;
static int g_mock_send_calls = 0;
static int g_mock_receive_calls = 0;
static int g_mock_close_calls = 0;
static int g_mock_getattr_calls = 0;
static int g_mock_setattr_calls = 0;

static rte_status_t mock_open(rte_flow_storage_t *storage,
                                const rte_flow_config_t *config,
                                rte_flow_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    g_mock_open_calls++;
    *out_handle = (rte_flow_handle_t)(void *)1; /* arbitrary non-NULL sentinel */
    return RTE_STATUS_OK;
}

static rte_status_t mock_send(rte_flow_handle_t handle, const void *data, size_t data_size,
                                rte_flow_channel_t channel, rte_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)data;
    (void)data_size;
    (void)channel;
    (void)timeout_ms;
    g_mock_send_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_receive(rte_flow_handle_t handle, void *out_data, size_t buffer_size,
                                   rte_flow_channel_t *out_channel, rte_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)out_data;
    (void)buffer_size;
    (void)timeout_ms;
    if (out_channel != NULL)
    {
        *out_channel = RTE_FLOW_CHANNEL_USER;
    }
    g_mock_receive_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_close(rte_flow_handle_t handle)
{
    (void)handle;
    g_mock_close_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_getattr(rte_flow_handle_t handle, rte_flow_attr_t *out_attr)
{
    (void)handle;
    out_attr->message_size = 4U;
    out_attr->oflags = (uint32_t)RTE_FLOW_O_PUBLISHER;
    out_attr->is_connected = true;
    g_mock_getattr_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_setattr(rte_flow_handle_t handle, const rte_flow_attr_t *new_attr,
                                   rte_flow_attr_t *out_old_attr)
{
    (void)handle;
    (void)new_attr;
    if (out_old_attr != NULL)
    {
        out_old_attr->message_size = 4U;
        out_old_attr->oflags = (uint32_t)RTE_FLOW_O_PUBLISHER;
        out_old_attr->is_connected = true;
    }
    g_mock_setattr_calls++;
    return RTE_STATUS_OK;
}

static const rte_osadapter_flow_t g_mock_osadapter_full __attribute__((unused)) = {
    mock_open, mock_send, mock_receive, mock_close, mock_getattr, mock_setattr
};

static const rte_osadapter_flow_t g_mock_osadapter_no_open __attribute__((unused)) = {
    NULL, NULL, NULL, NULL, NULL, NULL
};

int main(void)
{
    rte_flow_storage_t storage __attribute__((unused));
    rte_flow_handle_t handle __attribute__((unused)) = NULL;
    rte_flow_config_t pub_config __attribute__((unused)) = {0};

    /* Null-parameter rejection happens before any OSAdapter is consulted. */
    assert(rte_flow_open(NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);

    /* Invalid config (no name) rejected even with a valid oflags. */
    rte_flow_config_t no_name_config __attribute__((unused)) = {0};
    no_name_config.name = NULL;
    no_name_config.oflags = (uint32_t)RTE_FLOW_O_PUBLISHER;
    no_name_config.message_size = 4U;
    no_name_config.open_timeout_ms = 100U;
    assert(rte_flow_open(&storage, &no_name_config, &handle) == RTE_STATUS_INVALID_PARAM);

    /* message_size == 0 rejected. */
    rte_flow_config_t zero_size_config __attribute__((unused)) = {0};
    zero_size_config.name = "ab.peer";
    zero_size_config.oflags = (uint32_t)RTE_FLOW_O_PUBLISHER;
    zero_size_config.message_size = 0U;
    zero_size_config.open_timeout_ms = 100U;
    assert(rte_flow_open(&storage, &zero_size_config, &handle) == RTE_STATUS_INVALID_PARAM);

    /* oflags must select exactly one role - none set is rejected. */
    rte_flow_config_t no_role_config __attribute__((unused)) = {0};
    no_role_config.name = "ab.peer";
    no_role_config.oflags = (uint32_t)RTE_FLOW_O_NONBLOCK; /* not a role bit */
    no_role_config.message_size = 4U;
    no_role_config.open_timeout_ms = 100U;
    assert(rte_flow_open(&storage, &no_role_config, &handle) == RTE_STATUS_INVALID_PARAM);

    /* oflags must select exactly one role - two roles set is rejected
     * (e.g. PUBLISHER and SUBSCRIBER both on is not a well-defined Flow). */
    rte_flow_config_t two_roles_config __attribute__((unused)) = {0};
    two_roles_config.name = "ab.peer";
    two_roles_config.oflags = (uint32_t)RTE_FLOW_O_PUBLISHER | (uint32_t)RTE_FLOW_O_SUBSCRIBER;
    two_roles_config.message_size = 4U;
    two_roles_config.open_timeout_ms = 100U;
    assert(rte_flow_open(&storage, &two_roles_config, &handle) == RTE_STATUS_INVALID_PARAM);

    pub_config.name = "ab.peer";
    pub_config.oflags = (uint32_t)RTE_FLOW_O_PUBLISHER;
    pub_config.message_size = 4U;
    pub_config.open_timeout_ms = 100U;

    /* No OSAdapter registered yet: valid params, but nothing to dispatch to. */
    assert(rte_flow_open(&storage, &pub_config, &handle) == RTE_STATUS_NOT_INITIALIZED);

    /* Null-parameter rejection for send/receive/close/getattr/setattr
     * happens before any OSAdapter is consulted, independently of one another. */
    unsigned char buf[4] __attribute__((unused));
    rte_flow_attr_t attr __attribute__((unused));
    assert(rte_flow_send(NULL, "x", 1U, RTE_FLOW_CHANNEL_USER, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_send((rte_flow_handle_t)(void *)1, NULL, 1U, RTE_FLOW_CHANNEL_USER, 10U) ==
           RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_send((rte_flow_handle_t)(void *)1, "x", 0U, RTE_FLOW_CHANNEL_USER, 10U) ==
           RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_receive(NULL, buf, sizeof(buf), NULL, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_receive((rte_flow_handle_t)(void *)1, NULL, sizeof(buf), NULL, 10U) ==
           RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_receive((rte_flow_handle_t)(void *)1, buf, 0U, NULL, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_close(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_getattr(NULL, &attr) == RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_getattr((rte_flow_handle_t)(void *)1, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_setattr(NULL, &attr, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_flow_setattr((rte_flow_handle_t)(void *)1, NULL, NULL) == RTE_STATUS_INVALID_PARAM);

    /* No OSAdapter registered yet: valid params, but nothing to dispatch to,
     * for every remaining entry point. */
    rte_flow_handle_t dummy_handle = (rte_flow_handle_t)(void *)1;
    assert(rte_flow_send(dummy_handle, "x", 1U, RTE_FLOW_CHANNEL_USER, 10U) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_flow_receive(dummy_handle, buf, sizeof(buf), NULL, 10U) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_flow_close(dummy_handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_flow_getattr(dummy_handle, &attr) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_flow_setattr(dummy_handle, &attr, NULL) == RTE_STATUS_NOT_INITIALIZED);

    /* Registering NULL is rejected. */
    assert(rte_osadapter_flow_register(NULL) == RTE_STATUS_INVALID_PARAM);

    /* An OSAdapter with a NULL open slot yields NOT_SUPPORTED. */
    assert(rte_osadapter_flow_register(&g_mock_osadapter_no_open) == RTE_STATUS_OK);
    assert(rte_flow_open(&storage, &pub_config, &handle) == RTE_STATUS_NOT_SUPPORTED);

    /* send/receive/close/getattr/setattr with no slot -> NOT_SUPPORTED
     * (using a sentinel handle here since open() didn't produce a real one
     * with this OSAdapter - the dispatch layer doesn't dereference it). */
    handle = (rte_flow_handle_t)(void *)1;
    assert(rte_flow_send(handle, "x", 1U, RTE_FLOW_CHANNEL_USER, 10U) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_flow_receive(handle, buf, sizeof(buf), NULL, 10U) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_flow_close(handle) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_flow_getattr(handle, &attr) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_flow_setattr(handle, &attr, NULL) == RTE_STATUS_NOT_SUPPORTED);

    /* A fully-populated OSAdapter is actually reached for every entry
     * point, with the same already-validated arguments the caller
     * passed in. */
    assert(rte_osadapter_flow_register(&g_mock_osadapter_full) == RTE_STATUS_OK);
    assert(rte_flow_open(&storage, &pub_config, &handle) == RTE_STATUS_OK);
    assert(g_mock_open_calls == 1);
    assert(handle != NULL);

    assert(rte_flow_send(handle, "x", 1U, RTE_FLOW_CHANNEL_USER, 10U) == RTE_STATUS_OK);
    assert(g_mock_send_calls == 1);

    rte_flow_channel_t rx_channel;
    assert(rte_flow_receive(handle, buf, sizeof(buf), &rx_channel, 10U) == RTE_STATUS_OK);
    assert(g_mock_receive_calls == 1);
    assert(rx_channel == RTE_FLOW_CHANNEL_USER);

    assert(rte_flow_getattr(handle, &attr) == RTE_STATUS_OK);
    assert(g_mock_getattr_calls == 1);
    assert(attr.is_connected);

    assert(rte_flow_setattr(handle, &attr, NULL) == RTE_STATUS_OK);
    assert(g_mock_setattr_calls == 1);

    assert(rte_flow_close(handle) == RTE_STATUS_OK);
    assert(g_mock_close_calls == 1);

    return 0;
}
