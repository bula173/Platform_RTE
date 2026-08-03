/**
 * @file test_sapi_vital_channel.c
 * @brief Unit tests for vital channel abstraction (2oo2 / 2oo3 voting)
 */

#include <assert.h>
#include <string.h>

#include "safeapi/vital_channel/sapi_vital_channel.h"
#include "safeapi/status/sapi_status.h"

/* Mock IPC channels for testing */
static uint8_t mock_send_buffer[256];
static uint8_t mock_recv_buffer[256];
static sapi_status_t mock_send_status = SAPI_STATUS_OK;
static sapi_status_t mock_recv_status = SAPI_STATUS_OK;

/* Mock IPC send (for testing purposes) */
static sapi_status_t mock_ipc_send(sapi_ipc_request_reply_t *handle __attribute__((unused)),
                                    const void *data, size_t size)
{
    if (size > sizeof(mock_send_buffer)) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    if (mock_send_status != SAPI_STATUS_OK) {
        return mock_send_status;
    }

    memcpy(mock_send_buffer, data, size);
    return SAPI_STATUS_OK;
}

/* Mock IPC receive (for testing purposes) */
static sapi_status_t mock_ipc_receive(sapi_ipc_request_reply_t *handle __attribute__((unused)),
                                       void *data, size_t size, uint32_t timeout_ms __attribute__((unused)))
{
    if (size > sizeof(mock_recv_buffer)) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    if (mock_recv_status != SAPI_STATUS_OK) {
        return mock_recv_status;
    }

    memcpy(data, mock_recv_buffer, size);
    return SAPI_STATUS_OK;
}

static void test_vital_channel_2oo2_creation(void)
{
    sapi_vital_channel_storage_t storage;
    sapi_ipc_request_reply_t mock_channels[2] = {NULL, NULL};
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO2,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
    };

    /* Valid 2oo2 configuration */
    sapi_status_t rc = sapi_vital_channel_init(&storage, &config,
                                               (sapi_ipc_request_reply_t **)mock_channels, 2);
    assert(rc == SAPI_STATUS_OK);
    assert(storage.channel_count == 2);
    assert(storage.config.voting_strategy == SAPI_VOTING_2OO2);

    /* Invalid: wrong channel count for 2oo2 */
    rc = sapi_vital_channel_init(&storage, &config,
                                 (sapi_ipc_request_reply_t **)mock_channels, 3);
    assert(rc == SAPI_STATUS_INVALID_PARAM);

    /* Invalid: NULL storage */
    rc = sapi_vital_channel_init(NULL, &config,
                                 (sapi_ipc_request_reply_t **)mock_channels, 2);
    assert(rc == SAPI_STATUS_INVALID_PARAM);

    /* Invalid: NULL config */
    rc = sapi_vital_channel_init(&storage, NULL,
                                 (sapi_ipc_request_reply_t **)mock_channels, 2);
    assert(rc == SAPI_STATUS_INVALID_PARAM);
}

static void test_vital_channel_2oo3_creation(void)
{
    sapi_vital_channel_storage_t storage;
    sapi_ipc_request_reply_t mock_channels[3] = {NULL, NULL, NULL};
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO3,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
    };

    /* Valid 2oo3 configuration */
    sapi_status_t rc = sapi_vital_channel_init(&storage, &config,
                                               (sapi_ipc_request_reply_t **)mock_channels, 3);
    assert(rc == SAPI_STATUS_OK);
    assert(storage.channel_count == 3);
    assert(storage.config.voting_strategy == SAPI_VOTING_2OO3);

    /* Invalid: wrong channel count for 2oo3 */
    rc = sapi_vital_channel_init(&storage, &config,
                                 (sapi_ipc_request_reply_t **)mock_channels, 2);
    assert(rc == SAPI_STATUS_INVALID_PARAM);
}

static void test_vital_channel_health_tracking(void)
{
    sapi_vital_channel_storage_t storage;
    sapi_ipc_request_reply_t mock_channels[2] = {NULL, NULL};
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO2,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
    };

    sapi_status_t rc = sapi_vital_channel_init(&storage, &config,
                                               (sapi_ipc_request_reply_t **)mock_channels, 2);
    assert(rc == SAPI_STATUS_OK);

    /* Check initial health state */
    sapi_vital_channel_health_t health __attribute__((unused));
    rc = sapi_vital_channel_get_health((sapi_vital_channel_t *)&storage, 0, &health);
    assert(rc == SAPI_STATUS_OK);
    assert(health.send_count == 0);
    assert(health.receive_count == 0);
    assert(health.is_healthy == true);

    /* Check aggregated health */
    uint32_t healthy_count __attribute__((unused));
    uint32_t disagreements __attribute__((unused));
    rc = sapi_vital_channel_get_aggregated_health((sapi_vital_channel_t *)&storage,
                                                   &healthy_count, &disagreements);
    assert(rc == SAPI_STATUS_OK);
    assert(healthy_count == 2);  /* Both channels healthy initially */
    assert(disagreements == 0);  /* No disagreements yet */
}

static void test_vital_channel_destroy(void)
{
    sapi_vital_channel_storage_t storage;
    sapi_ipc_request_reply_t mock_channels[2] = {NULL, NULL};
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO2,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
    };

    sapi_status_t rc = sapi_vital_channel_init(&storage, &config,
                                               (sapi_ipc_request_reply_t **)mock_channels, 2);
    assert(rc == SAPI_STATUS_OK);

    /* Destroy is idempotent */
    rc = sapi_vital_channel_destroy((sapi_vital_channel_t *)&storage);
    assert(rc == SAPI_STATUS_OK);

    /* Destroy NULL is OK */
    rc = sapi_vital_channel_destroy(NULL);
    assert(rc == SAPI_STATUS_OK);
}

int main(void)
{
    test_vital_channel_2oo2_creation();
    test_vital_channel_2oo3_creation();
    test_vital_channel_health_tracking();
    test_vital_channel_destroy();

    return 0;
}
