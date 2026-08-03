/**
 * @file test_sapi_vital_channel.c
 * @brief Unit tests for vital channel abstraction (2oo2 / 2oo3 voting)
 */

#include <assert.h>
#include <string.h>

#include "safeapi/vital_channel/sapi_vital_channel.h"
#include "safeapi/status/sapi_status.h"

/* Mock channel storage for testing */
typedef struct {
    uint8_t send_buffer[256];
    uint8_t recv_buffer[256];
    sapi_status_t send_status;
    sapi_status_t recv_status;
    uint32_t send_call_count;
    uint32_t recv_call_count;
} mock_channel_t;

/* Test fixture */
static mock_channel_t mock_channels[8];

/* Mock send callback (transport-agnostic backend) */
static sapi_status_t mock_backend_send(void *channel, const void *data, size_t size)
{
    mock_channel_t *ch = (mock_channel_t *)channel;

    if (ch == NULL || size > sizeof(ch->send_buffer)) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    if (ch->send_status != SAPI_STATUS_OK) {
        return ch->send_status;
    }

    memcpy(ch->send_buffer, data, size);
    ch->send_call_count++;
    return SAPI_STATUS_OK;
}

/* Mock receive callback (transport-agnostic backend) */
static sapi_status_t mock_backend_recv(void *channel, void *data, size_t size,
                                       uint32_t timeout_ms __attribute__((unused)))
{
    mock_channel_t *ch = (mock_channel_t *)channel;

    if (ch == NULL || size > sizeof(ch->recv_buffer)) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    if (ch->recv_status != SAPI_STATUS_OK) {
        return ch->recv_status;
    }

    memcpy(data, ch->recv_buffer, size);
    ch->recv_call_count++;
    return SAPI_STATUS_OK;
}

/* Helper to reset mock channels */
static void reset_mock_channels(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        memset(&mock_channels[i], 0, sizeof(mock_channels[i]));
        mock_channels[i].send_status = SAPI_STATUS_OK;
        mock_channels[i].recv_status = SAPI_STATUS_OK;
    }
}

static void test_vital_channel_2oo2_creation(void)
{
    sapi_vital_channel_storage_t storage;
    void *channels[2];
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO2,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
        .backend_send = mock_backend_send,
        .backend_recv = mock_backend_recv,
    };

    reset_mock_channels(2);
    channels[0] = &mock_channels[0];
    channels[1] = &mock_channels[1];

    /* Valid 2oo2 configuration */
    sapi_status_t rc = sapi_vital_channel_init(&storage, &config, channels, 2);
    assert(rc == SAPI_STATUS_OK);
    assert(storage.channel_count == 2);
    assert(storage.config.voting_strategy == SAPI_VOTING_2OO2);

    /* Invalid: wrong channel count for 2oo2 */
    rc = sapi_vital_channel_init(&storage, &config, channels, 3);
    assert(rc == SAPI_STATUS_INVALID_PARAM);

    /* Invalid: NULL storage */
    rc = sapi_vital_channel_init(NULL, &config, channels, 2);
    assert(rc == SAPI_STATUS_INVALID_PARAM);

    /* Invalid: NULL config */
    rc = sapi_vital_channel_init(&storage, NULL, channels, 2);
    assert(rc == SAPI_STATUS_INVALID_PARAM);

    /* Invalid: NULL backend callbacks */
    sapi_vital_channel_config_t bad_config = config;
    bad_config.backend_send = NULL;
    rc = sapi_vital_channel_init(&storage, &bad_config, channels, 2);
    assert(rc == SAPI_STATUS_INVALID_PARAM);
}

static void test_vital_channel_2oo3_creation(void)
{
    sapi_vital_channel_storage_t storage;
    void *channels[3];
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO3,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
        .backend_send = mock_backend_send,
        .backend_recv = mock_backend_recv,
    };

    reset_mock_channels(3);
    channels[0] = &mock_channels[0];
    channels[1] = &mock_channels[1];
    channels[2] = &mock_channels[2];

    /* Valid 2oo3 configuration */
    sapi_status_t rc = sapi_vital_channel_init(&storage, &config, channels, 3);
    assert(rc == SAPI_STATUS_OK);
    assert(storage.channel_count == 3);
    assert(storage.config.voting_strategy == SAPI_VOTING_2OO3);

    /* Invalid: wrong channel count for 2oo3 */
    rc = sapi_vital_channel_init(&storage, &config, channels, 2);
    assert(rc == SAPI_STATUS_INVALID_PARAM);
}

static void test_vital_channel_health_tracking(void)
{
    sapi_vital_channel_storage_t storage;
    void *channels[2];
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO2,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
        .backend_send = mock_backend_send,
        .backend_recv = mock_backend_recv,
    };

    reset_mock_channels(2);
    channels[0] = &mock_channels[0];
    channels[1] = &mock_channels[1];

    sapi_status_t rc = sapi_vital_channel_init(&storage, &config, channels, 2);
    assert(rc == SAPI_STATUS_OK);

    /* Check initial health state */
    sapi_vital_channel_health_t health;
    rc = sapi_vital_channel_get_health((sapi_vital_channel_t *)&storage, 0, &health);
    assert(rc == SAPI_STATUS_OK);
    assert(health.send_count == 0);
    assert(health.receive_count == 0);
    assert(health.is_healthy == true);

    /* Check aggregated health */
    uint32_t healthy_count;
    uint32_t disagreements;
    rc = sapi_vital_channel_get_aggregated_health((sapi_vital_channel_t *)&storage,
                                                   &healthy_count, &disagreements);
    assert(rc == SAPI_STATUS_OK);
    assert(healthy_count == 2);  /* Both channels healthy initially */
    assert(disagreements == 0);  /* No disagreements yet */
}

static void test_vital_channel_destroy(void)
{
    sapi_vital_channel_storage_t storage;
    void *channels[2];
    sapi_vital_channel_config_t config = {
        .voting_strategy = SAPI_VOTING_2OO2,
        .channel_timeout_ms = 1000,
        .log_disagreements = false,
        .on_disagreement = NULL,
        .context = NULL,
        .backend_send = mock_backend_send,
        .backend_recv = mock_backend_recv,
    };

    reset_mock_channels(2);
    channels[0] = &mock_channels[0];
    channels[1] = &mock_channels[1];

    sapi_status_t rc = sapi_vital_channel_init(&storage, &config, channels, 2);
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
