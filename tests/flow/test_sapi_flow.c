/* Tests for the sapi_flow validate-then-dispatch API (OCORA PI-API
 * compatibility, ADR-005 backend seam): see tests/netlink/test_sapi_netlink.c
 * for the pattern this follows. */
#include <assert.h>
#include "safeapi/oal/flow/sapi_flow.h"
#include "safeapi_backend/flow/sapi_flow_backend.h"

static int g_mock_open_calls = 0;
static int g_mock_send_calls = 0;
static int g_mock_receive_calls = 0;
static int g_mock_close_calls = 0;
static int g_mock_getattr_calls = 0;
static int g_mock_setattr_calls = 0;

static sapi_status_t mock_open(sapi_flow_storage_t *storage,
                                const sapi_flow_config_t *config,
                                sapi_flow_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    g_mock_open_calls++;
    *out_handle = (sapi_flow_handle_t)(void *)1; /* arbitrary non-NULL sentinel */
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_send(sapi_flow_handle_t handle, const void *data, size_t data_size,
                                sapi_flow_channel_t channel, sapi_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)data;
    (void)data_size;
    (void)channel;
    (void)timeout_ms;
    g_mock_send_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_receive(sapi_flow_handle_t handle, void *out_data, size_t buffer_size,
                                   sapi_flow_channel_t *out_channel, sapi_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)out_data;
    (void)buffer_size;
    (void)timeout_ms;
    if (out_channel != NULL)
    {
        *out_channel = SAPI_FLOW_CHANNEL_USER;
    }
    g_mock_receive_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_close(sapi_flow_handle_t handle)
{
    (void)handle;
    g_mock_close_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_getattr(sapi_flow_handle_t handle, sapi_flow_attr_t *out_attr)
{
    (void)handle;
    out_attr->message_size = 4U;
    out_attr->oflags = (uint32_t)SAPI_FLOW_O_PUBLISHER;
    out_attr->is_connected = true;
    g_mock_getattr_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_setattr(sapi_flow_handle_t handle, const sapi_flow_attr_t *new_attr,
                                   sapi_flow_attr_t *out_old_attr)
{
    (void)handle;
    (void)new_attr;
    if (out_old_attr != NULL)
    {
        out_old_attr->message_size = 4U;
        out_old_attr->oflags = (uint32_t)SAPI_FLOW_O_PUBLISHER;
        out_old_attr->is_connected = true;
    }
    g_mock_setattr_calls++;
    return SAPI_STATUS_OK;
}

static const sapi_flow_backend_t g_mock_backend_full __attribute__((unused)) = {
    mock_open, mock_send, mock_receive, mock_close, mock_getattr, mock_setattr
};

static const sapi_flow_backend_t g_mock_backend_no_open __attribute__((unused)) = {
    NULL, NULL, NULL, NULL, NULL, NULL
};

int main(void)
{
    sapi_flow_storage_t storage __attribute__((unused));
    sapi_flow_handle_t handle __attribute__((unused)) = NULL;
    sapi_flow_config_t pub_config __attribute__((unused)) = {0};

    /* Null-parameter rejection happens before any backend is consulted. */
    assert(sapi_flow_open(NULL, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* Invalid config (no name) rejected even with a valid oflags. */
    sapi_flow_config_t no_name_config __attribute__((unused)) = {0};
    no_name_config.name = NULL;
    no_name_config.oflags = (uint32_t)SAPI_FLOW_O_PUBLISHER;
    no_name_config.message_size = 4U;
    no_name_config.open_timeout_ms = 100U;
    assert(sapi_flow_open(&storage, &no_name_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    /* message_size == 0 rejected. */
    sapi_flow_config_t zero_size_config __attribute__((unused)) = {0};
    zero_size_config.name = "ab.peer";
    zero_size_config.oflags = (uint32_t)SAPI_FLOW_O_PUBLISHER;
    zero_size_config.message_size = 0U;
    zero_size_config.open_timeout_ms = 100U;
    assert(sapi_flow_open(&storage, &zero_size_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    /* oflags must select exactly one role - none set is rejected. */
    sapi_flow_config_t no_role_config __attribute__((unused)) = {0};
    no_role_config.name = "ab.peer";
    no_role_config.oflags = (uint32_t)SAPI_FLOW_O_NONBLOCK; /* not a role bit */
    no_role_config.message_size = 4U;
    no_role_config.open_timeout_ms = 100U;
    assert(sapi_flow_open(&storage, &no_role_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    /* oflags must select exactly one role - two roles set is rejected
     * (e.g. PUBLISHER and SUBSCRIBER both on is not a well-defined Flow). */
    sapi_flow_config_t two_roles_config __attribute__((unused)) = {0};
    two_roles_config.name = "ab.peer";
    two_roles_config.oflags = (uint32_t)SAPI_FLOW_O_PUBLISHER | (uint32_t)SAPI_FLOW_O_SUBSCRIBER;
    two_roles_config.message_size = 4U;
    two_roles_config.open_timeout_ms = 100U;
    assert(sapi_flow_open(&storage, &two_roles_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    pub_config.name = "ab.peer";
    pub_config.oflags = (uint32_t)SAPI_FLOW_O_PUBLISHER;
    pub_config.message_size = 4U;
    pub_config.open_timeout_ms = 100U;

    /* No backend registered yet: valid params, but nothing to dispatch to. */
    assert(sapi_flow_open(&storage, &pub_config, &handle) == SAPI_STATUS_NOT_INITIALIZED);

    /* Null-parameter rejection for send/receive/close/getattr/setattr
     * happens before any backend is consulted, independently of one another. */
    unsigned char buf[4] __attribute__((unused));
    sapi_flow_attr_t attr __attribute__((unused));
    assert(sapi_flow_send(NULL, "x", 1U, SAPI_FLOW_CHANNEL_USER, 10U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_send((sapi_flow_handle_t)(void *)1, NULL, 1U, SAPI_FLOW_CHANNEL_USER, 10U) ==
           SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_send((sapi_flow_handle_t)(void *)1, "x", 0U, SAPI_FLOW_CHANNEL_USER, 10U) ==
           SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_receive(NULL, buf, sizeof(buf), NULL, 10U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_receive((sapi_flow_handle_t)(void *)1, NULL, sizeof(buf), NULL, 10U) ==
           SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_receive((sapi_flow_handle_t)(void *)1, buf, 0U, NULL, 10U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_close(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_getattr(NULL, &attr) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_getattr((sapi_flow_handle_t)(void *)1, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_setattr(NULL, &attr, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_flow_setattr((sapi_flow_handle_t)(void *)1, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* No backend registered yet: valid params, but nothing to dispatch to,
     * for every remaining entry point. */
    sapi_flow_handle_t dummy_handle = (sapi_flow_handle_t)(void *)1;
    assert(sapi_flow_send(dummy_handle, "x", 1U, SAPI_FLOW_CHANNEL_USER, 10U) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_flow_receive(dummy_handle, buf, sizeof(buf), NULL, 10U) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_flow_close(dummy_handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_flow_getattr(dummy_handle, &attr) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_flow_setattr(dummy_handle, &attr, NULL) == SAPI_STATUS_NOT_INITIALIZED);

    /* Registering NULL is rejected. */
    assert(sapi_flow_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* A backend with a NULL open slot yields NOT_SUPPORTED. */
    assert(sapi_flow_register_backend(&g_mock_backend_no_open) == SAPI_STATUS_OK);
    assert(sapi_flow_open(&storage, &pub_config, &handle) == SAPI_STATUS_NOT_SUPPORTED);

    /* send/receive/close/getattr/setattr with no slot -> NOT_SUPPORTED
     * (using a sentinel handle here since open() didn't produce a real one
     * with this backend - the dispatch layer doesn't dereference it). */
    handle = (sapi_flow_handle_t)(void *)1;
    assert(sapi_flow_send(handle, "x", 1U, SAPI_FLOW_CHANNEL_USER, 10U) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_flow_receive(handle, buf, sizeof(buf), NULL, 10U) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_flow_close(handle) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_flow_getattr(handle, &attr) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_flow_setattr(handle, &attr, NULL) == SAPI_STATUS_NOT_SUPPORTED);

    /* A fully-populated backend is actually reached for every entry
     * point, with the same already-validated arguments the caller
     * passed in. */
    assert(sapi_flow_register_backend(&g_mock_backend_full) == SAPI_STATUS_OK);
    assert(sapi_flow_open(&storage, &pub_config, &handle) == SAPI_STATUS_OK);
    assert(g_mock_open_calls == 1);
    assert(handle != NULL);

    assert(sapi_flow_send(handle, "x", 1U, SAPI_FLOW_CHANNEL_USER, 10U) == SAPI_STATUS_OK);
    assert(g_mock_send_calls == 1);

    sapi_flow_channel_t rx_channel;
    assert(sapi_flow_receive(handle, buf, sizeof(buf), &rx_channel, 10U) == SAPI_STATUS_OK);
    assert(g_mock_receive_calls == 1);
    assert(rx_channel == SAPI_FLOW_CHANNEL_USER);

    assert(sapi_flow_getattr(handle, &attr) == SAPI_STATUS_OK);
    assert(g_mock_getattr_calls == 1);
    assert(attr.is_connected);

    assert(sapi_flow_setattr(handle, &attr, NULL) == SAPI_STATUS_OK);
    assert(g_mock_setattr_calls == 1);

    assert(sapi_flow_close(handle) == SAPI_STATUS_OK);
    assert(g_mock_close_calls == 1);

    return 0;
}
