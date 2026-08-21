/* Tests for sapi_channel (ADR-025): one redundant link - validate,
 * then dispatch to its configured send/recv callbacks, tracking health as
 * a side effect. Same "validate then dispatch" pattern as
 * tests/timer/test_sapi_timer.c. The N-way voting logic this module used
 * to contain has moved to sapi_voter - see tests/voter/test_sapi_voter.c. */
#include <assert.h>
#include <string.h>
#include "safeapi/channel_link/sapi_channel.h"
#include "safeapi/lifecycle/sapi_lifecycle.h"

static int g_send_calls;
static int g_recv_calls;
static sapi_status_t g_send_result = SAPI_STATUS_OK;
static sapi_status_t g_recv_result = SAPI_STATUS_OK;
static uint8_t g_last_sent[16];
static size_t g_last_sent_size;

static sapi_status_t mock_send(void *channel_handle, const void *data, size_t data_size)
{
    (void)channel_handle;
    g_send_calls++;
    if (g_send_result == SAPI_STATUS_OK)
    {
        memcpy(g_last_sent, data, data_size);
        g_last_sent_size = data_size;
    }
    return g_send_result;
}

static sapi_status_t mock_recv(void *channel_handle, void *data, size_t data_size, uint32_t timeout_ms)
{
    (void)channel_handle;
    (void)timeout_ms;
    g_recv_calls++;
    if (g_recv_result == SAPI_STATUS_OK)
    {
        memset(data, 0x5A, data_size);
    }
    return g_recv_result;
}

static void reset_mock(void)
{
    g_send_calls = 0;
    g_recv_calls = 0;
    g_send_result = SAPI_STATUS_OK;
    g_recv_result = SAPI_STATUS_OK;
}

static void test_init_validation(void)
{
    sapi_channel_storage_t storage;
    sapi_channel_config_t config;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;

    assert(sapi_channel_init(NULL, &config) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_init(&storage, NULL) == SAPI_STATUS_INVALID_PARAM);

    config.send = NULL;
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_INVALID_PARAM);
    config.send = mock_send;

    config.recv = NULL;
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_INVALID_PARAM);
    config.recv = mock_recv;

    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_OK);

    {
        sapi_channel_health_t health;
        assert(sapi_channel_get_health(&storage, &health) == SAPI_STATUS_OK);
        assert(health.send_count == 0U);
        assert(health.receive_count == 0U);
        assert(health.is_healthy);
    }
}

static void test_send_dispatch(void)
{
    sapi_channel_storage_t storage;
    sapi_channel_config_t config;
    uint8_t payload[4] = {1, 2, 3, 4};
    sapi_channel_health_t health;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_OK);

    assert(sapi_channel_send(NULL, payload, sizeof(payload)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_send(&storage, NULL, sizeof(payload)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_send(&storage, payload, 0U) == SAPI_STATUS_INVALID_PARAM);

    reset_mock();
    assert(sapi_channel_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_OK);
    assert(g_send_calls == 1);
    assert(memcmp(g_last_sent, payload, sizeof(payload)) == 0);
    assert(g_last_sent_size == sizeof(payload));
    assert(sapi_channel_get_health(&storage, &health) == SAPI_STATUS_OK);
    assert(health.send_count == 1U);
    assert(health.send_error_count == 0U);

    reset_mock();
    g_send_result = SAPI_STATUS_HARDWARE_FAULT;
    assert(sapi_channel_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_HARDWARE_FAULT);
    assert(sapi_channel_get_health(&storage, &health) == SAPI_STATUS_OK);
    assert(health.send_error_count == 1U);
    assert(health.last_error == SAPI_STATUS_HARDWARE_FAULT);
}

static void test_receive_dispatch(void)
{
    sapi_channel_storage_t storage;
    sapi_channel_config_t config;
    uint8_t buf[4] = {0};
    sapi_channel_health_t health;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_OK);

    assert(sapi_channel_receive(NULL, buf, sizeof(buf), 100U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_receive(&storage, NULL, sizeof(buf), 100U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_receive(&storage, buf, 0U, 100U) == SAPI_STATUS_INVALID_PARAM);

    reset_mock();
    assert(sapi_channel_receive(&storage, buf, sizeof(buf), 100U) == SAPI_STATUS_OK);
    assert(g_recv_calls == 1);
    assert(buf[0] == 0x5AU);
    assert(sapi_channel_get_health(&storage, &health) == SAPI_STATUS_OK);
    assert(health.receive_count == 1U);

    reset_mock();
    g_recv_result = SAPI_STATUS_TIMEOUT;
    assert(sapi_channel_receive(&storage, buf, sizeof(buf), 100U) == SAPI_STATUS_TIMEOUT);
    assert(sapi_channel_get_health(&storage, &health) == SAPI_STATUS_OK);
    assert(health.receive_error_count == 1U);
    assert(health.last_error == SAPI_STATUS_TIMEOUT);
}

static void test_not_initialized(void)
{
    sapi_channel_storage_t storage;
    uint8_t buf[4] = {0};

    memset(&storage, 0, sizeof(storage)); /* never sapi_channel_init()-ed */

    assert(sapi_channel_send(&storage, buf, sizeof(buf)) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_channel_receive(&storage, buf, sizeof(buf), 100U) == SAPI_STATUS_NOT_INITIALIZED);
}

static void test_set_healthy_and_get_health_validation(void)
{
    sapi_channel_storage_t storage;
    sapi_channel_config_t config;
    sapi_channel_health_t health;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_OK);

    assert(sapi_channel_get_health(NULL, &health) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_get_health(&storage, NULL) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_channel_set_healthy(NULL, false) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_set_healthy(&storage, false) == SAPI_STATUS_OK);
    assert(sapi_channel_get_health(&storage, &health) == SAPI_STATUS_OK);
    assert(!health.is_healthy);

    assert(sapi_channel_set_healthy(&storage, true) == SAPI_STATUS_OK);
    assert(sapi_channel_get_health(&storage, &health) == SAPI_STATUS_OK);
    assert(health.is_healthy);
}

/* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
 * locked, sapi_channel_init() refuses even with a fully-valid config. */
static void test_lifecycle_lock(void)
{
    sapi_channel_storage_t storage;
    sapi_channel_config_t config;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;

    sapi_lifecycle_lock();
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_INVALID_STATE);
    sapi_lifecycle_unlock();
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_OK);
}

static void test_destroy(void)
{
    sapi_channel_storage_t storage;
    sapi_channel_config_t config;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(sapi_channel_init(&storage, &config) == SAPI_STATUS_OK);

    assert(sapi_channel_destroy(NULL) == SAPI_STATUS_OK);
    assert(sapi_channel_destroy(&storage) == SAPI_STATUS_OK);

    {
        uint8_t buf[4] = {0};
        assert(sapi_channel_send(&storage, buf, sizeof(buf)) == SAPI_STATUS_NOT_INITIALIZED);
    }
}

int main(void)
{
    test_init_validation();
    test_send_dispatch();
    test_receive_dispatch();
    test_not_initialized();
    test_set_healthy_and_get_health_validation();
    test_lifecycle_lock();
    test_destroy();
    return 0;
}
