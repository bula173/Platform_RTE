/* Tests for the rte_ipc validate-then-dispatch API (ADR-005): see
 * tests/nvm/test_rte_nvm.c for the pattern this follows. */
#include <assert.h>
#include <string.h>
#include "safeapi/oal/ipc/rte_ipc.h"
#include "safeapi_osadapter/ipc/rte_osadapter_ipc.h"

static unsigned char g_rx_payload[4] = { 1U, 2U, 3U, 4U };

static rte_status_t mock_create(rte_ipc_storage_t *storage,
                                  const rte_ipc_config_t *config,
                                  rte_ipc_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    *out_handle = (rte_ipc_handle_t)(void *)1;
    return RTE_STATUS_OK;
}

static rte_status_t mock_send(rte_ipc_handle_t handle, const void *message,
                                size_t message_size, rte_duration_ms_t timeout_ms)
{
    (void)handle;
    (void)message;
    (void)message_size;
    (void)timeout_ms;
    return RTE_STATUS_OK;
}

static rte_status_t mock_receive(rte_ipc_handle_t handle, void *out_message,
                                   size_t buffer_size, rte_duration_ms_t timeout_ms)
{
    size_t copy_size = sizeof(g_rx_payload);
    (void)handle;
    (void)timeout_ms;
    if (buffer_size < copy_size)
    {
        copy_size = buffer_size;
    }
    memcpy(out_message, g_rx_payload, copy_size);
    return RTE_STATUS_OK;
}

static rte_status_t mock_destroy(rte_ipc_handle_t handle)
{
    (void)handle;
    return RTE_STATUS_OK;
}

/* Partial OSAdapter: no slots implemented -> exercises NOT_SUPPORTED paths. */
static const rte_osadapter_ipc_t g_partial_osadapter = {
    NULL, NULL, NULL, NULL
};

/* Full OSAdapter: every slot implemented -> exercises success paths. */
static const rte_osadapter_ipc_t g_full_osadapter = {
    mock_create, mock_send, mock_receive, mock_destroy
};

int main(void)
{
    rte_ipc_storage_t storage;
    rte_ipc_handle_t handle = NULL;
    rte_ipc_config_t config;
    unsigned char tx_buf[4] = { 9U, 9U, 9U, 9U };
    unsigned char rx_buf[4] = { 0U };

    config.name = "chan";
    config.message_size = 4U;
    config.queue_depth = 8U;

    /* --- rte_ipc_create: INVALID_PARAM paths --- */
    assert(rte_ipc_create(NULL, &config, &handle) == RTE_STATUS_INVALID_PARAM);
    assert(rte_ipc_create(&storage, NULL, &handle) == RTE_STATUS_INVALID_PARAM);
    assert(rte_ipc_create(&storage, &config, NULL) == RTE_STATUS_INVALID_PARAM);

    {
        rte_ipc_config_t bad_msg_size = config;
        bad_msg_size.message_size = 0U;
        assert(rte_ipc_create(&storage, &bad_msg_size, &handle) == RTE_STATUS_INVALID_PARAM);
    }
    {
        rte_ipc_config_t bad_depth = config;
        bad_depth.queue_depth = 0U;
        assert(rte_ipc_create(&storage, &bad_depth, &handle) == RTE_STATUS_INVALID_PARAM);
    }

    /* --- Not yet registered: NOT_INITIALIZED paths --- */
    assert(rte_ipc_create(&storage, &config, &handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_ipc_send((rte_ipc_handle_t)(void *)1, tx_buf, sizeof(tx_buf), 0U) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_ipc_receive((rte_ipc_handle_t)(void *)1, rx_buf, sizeof(rx_buf), 0U) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_ipc_destroy((rte_ipc_handle_t)(void *)1) == RTE_STATUS_NOT_INITIALIZED);

    /* --- rte_ipc_send: INVALID_PARAM paths --- */
    assert(rte_ipc_send(NULL, tx_buf, sizeof(tx_buf), 0U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_ipc_send((rte_ipc_handle_t)(void *)1, NULL, sizeof(tx_buf), 0U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_ipc_send((rte_ipc_handle_t)(void *)1, tx_buf, 0U, 0U) == RTE_STATUS_INVALID_PARAM);

    /* --- rte_ipc_receive: INVALID_PARAM paths --- */
    assert(rte_ipc_receive(NULL, rx_buf, sizeof(rx_buf), 0U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_ipc_receive((rte_ipc_handle_t)(void *)1, NULL, sizeof(rx_buf), 0U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_ipc_receive((rte_ipc_handle_t)(void *)1, rx_buf, 0U, 0U) == RTE_STATUS_INVALID_PARAM);

    /* --- rte_ipc_destroy: INVALID_PARAM path --- */
    assert(rte_ipc_destroy(NULL) == RTE_STATUS_INVALID_PARAM);

    /* --- register_osadapter --- */
    assert(rte_osadapter_ipc_register(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_osadapter_ipc_register(&g_partial_osadapter) == RTE_STATUS_OK);

    /* --- Partial OSAdapter registered: NOT_SUPPORTED paths --- */
    assert(rte_ipc_create(&storage, &config, &handle) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_ipc_send((rte_ipc_handle_t)(void *)1, tx_buf, sizeof(tx_buf), 0U) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_ipc_receive((rte_ipc_handle_t)(void *)1, rx_buf, sizeof(rx_buf), 0U) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_ipc_destroy((rte_ipc_handle_t)(void *)1) == RTE_STATUS_NOT_SUPPORTED);

    /* --- Full OSAdapter registered: success paths --- */
    assert(rte_osadapter_ipc_register(&g_full_osadapter) == RTE_STATUS_OK);

    handle = NULL;
    assert(rte_ipc_create(&storage, &config, &handle) == RTE_STATUS_OK);
    assert(handle != NULL);

    assert(rte_ipc_send(handle, tx_buf, sizeof(tx_buf), 100U) == RTE_STATUS_OK);

    memset(rx_buf, 0, sizeof(rx_buf));
    assert(rte_ipc_receive(handle, rx_buf, sizeof(rx_buf), 100U) == RTE_STATUS_OK);
    assert(memcmp(rx_buf, g_rx_payload, sizeof(rx_buf)) == 0);

    assert(rte_ipc_destroy(handle) == RTE_STATUS_OK);

    return 0;
}
