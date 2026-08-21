/* Tests for the sapi_ipc validate-then-dispatch API (ADR-005): see
 * tests/nvm/test_sapi_nvm.c for the pattern this follows. */
#include <assert.h>
#include <string.h>
#include "safeapi/ipc/sapi_ipc.h"
#include "safeapi_backend/ipc/sapi_ipc_backend.h"

static unsigned char g_rx_payload[4] = { 1U, 2U, 3U, 4U };

static sapi_status_t mock_create(sapi_ipc_storage_t *storage,
                                  const sapi_ipc_config_t *config,
                                  sapi_ipc_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    *out_handle = (sapi_ipc_handle_t)(void *)1;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_send(sapi_ipc_handle_t handle, const void *message,
                                size_t message_size, sapi_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)message;
    (void)message_size;
    (void)timeout_ms;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_receive(sapi_ipc_handle_t handle, void *out_message,
                                   size_t buffer_size, sapi_duration_ms_t timeout_ms)
{
    size_t copy_size = sizeof(g_rx_payload);
    (void)handle;
    (void)timeout_ms;
    if (buffer_size < copy_size)
    {
        copy_size = buffer_size;
    }
    memcpy(out_message, g_rx_payload, copy_size);
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_destroy(sapi_ipc_handle_t handle)
{
    (void)handle;
    return SAPI_STATUS_OK;
}

/* Partial backend: no slots implemented -> exercises NOT_SUPPORTED paths. */
static const sapi_ipc_backend_t g_partial_backend = {
    NULL, NULL, NULL, NULL
};

/* Full backend: every slot implemented -> exercises success paths. */
static const sapi_ipc_backend_t g_full_backend = {
    mock_create, mock_send, mock_receive, mock_destroy
};

int main(void)
{
    sapi_ipc_storage_t storage;
    sapi_ipc_handle_t handle = NULL;
    sapi_ipc_config_t config;
    unsigned char tx_buf[4] = { 9U, 9U, 9U, 9U };
    unsigned char rx_buf[4] = { 0U };

    config.name = "chan";
    config.message_size = 4U;
    config.queue_depth = 8U;

    /* --- sapi_ipc_create: INVALID_PARAM paths --- */
    assert(sapi_ipc_create(NULL, &config, &handle) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_ipc_create(&storage, NULL, &handle) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_ipc_create(&storage, &config, NULL) == SAPI_STATUS_INVALID_PARAM);

    {
        sapi_ipc_config_t bad_msg_size = config;
        bad_msg_size.message_size = 0U;
        assert(sapi_ipc_create(&storage, &bad_msg_size, &handle) == SAPI_STATUS_INVALID_PARAM);
    }
    {
        sapi_ipc_config_t bad_depth = config;
        bad_depth.queue_depth = 0U;
        assert(sapi_ipc_create(&storage, &bad_depth, &handle) == SAPI_STATUS_INVALID_PARAM);
    }

    /* --- Not yet registered: NOT_INITIALIZED paths --- */
    assert(sapi_ipc_create(&storage, &config, &handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_ipc_send((sapi_ipc_handle_t)(void *)1, tx_buf, sizeof(tx_buf), 0U) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_ipc_receive((sapi_ipc_handle_t)(void *)1, rx_buf, sizeof(rx_buf), 0U) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_ipc_destroy((sapi_ipc_handle_t)(void *)1) == SAPI_STATUS_NOT_INITIALIZED);

    /* --- sapi_ipc_send: INVALID_PARAM paths --- */
    assert(sapi_ipc_send(NULL, tx_buf, sizeof(tx_buf), 0U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_ipc_send((sapi_ipc_handle_t)(void *)1, NULL, sizeof(tx_buf), 0U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_ipc_send((sapi_ipc_handle_t)(void *)1, tx_buf, 0U, 0U) == SAPI_STATUS_INVALID_PARAM);

    /* --- sapi_ipc_receive: INVALID_PARAM paths --- */
    assert(sapi_ipc_receive(NULL, rx_buf, sizeof(rx_buf), 0U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_ipc_receive((sapi_ipc_handle_t)(void *)1, NULL, sizeof(rx_buf), 0U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_ipc_receive((sapi_ipc_handle_t)(void *)1, rx_buf, 0U, 0U) == SAPI_STATUS_INVALID_PARAM);

    /* --- sapi_ipc_destroy: INVALID_PARAM path --- */
    assert(sapi_ipc_destroy(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* --- register_backend --- */
    assert(sapi_ipc_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_ipc_register_backend(&g_partial_backend) == SAPI_STATUS_OK);

    /* --- Partial backend registered: NOT_SUPPORTED paths --- */
    assert(sapi_ipc_create(&storage, &config, &handle) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_ipc_send((sapi_ipc_handle_t)(void *)1, tx_buf, sizeof(tx_buf), 0U) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_ipc_receive((sapi_ipc_handle_t)(void *)1, rx_buf, sizeof(rx_buf), 0U) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_ipc_destroy((sapi_ipc_handle_t)(void *)1) == SAPI_STATUS_NOT_SUPPORTED);

    /* --- Full backend registered: success paths --- */
    assert(sapi_ipc_register_backend(&g_full_backend) == SAPI_STATUS_OK);

    handle = NULL;
    assert(sapi_ipc_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);

    assert(sapi_ipc_send(handle, tx_buf, sizeof(tx_buf), 100U) == SAPI_STATUS_OK);

    memset(rx_buf, 0, sizeof(rx_buf));
    assert(sapi_ipc_receive(handle, rx_buf, sizeof(rx_buf), 100U) == SAPI_STATUS_OK);
    assert(memcmp(rx_buf, g_rx_payload, sizeof(rx_buf)) == 0);

    assert(sapi_ipc_destroy(handle) == SAPI_STATUS_OK);

    return 0;
}
