/* Tests for rte_channel (ADR-025): one redundant link - validate,
 * then dispatch to its configured send/recv callbacks, tracking health as
 * a side effect. Same "validate then dispatch" pattern as
 * tests/timer/test_rte_timer.c. The N-way voting logic this module used
 * to contain has moved to rte_voter - see tests/voter/test_rte_voter.c. */
#include <assert.h>
#include <string.h>
#include "rte/redundancy/channel_link/rte_channel.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"

static int g_send_calls;
static int g_recv_calls;
static rte_status_t g_send_result = RTE_STATUS_OK;
static rte_status_t g_recv_result = RTE_STATUS_OK;
static uint8_t g_last_sent[16];
static size_t g_last_sent_size;

static rte_status_t mock_send(void *channel_handle, const void *data, size_t data_size)
{
    (void)channel_handle;
    g_send_calls++;
    if (g_send_result == RTE_STATUS_OK)
    {
        memcpy(g_last_sent, data, data_size);
        g_last_sent_size = data_size;
    }
    return g_send_result;
}

static rte_status_t mock_recv(void *channel_handle, void *data, size_t data_size, uint32_t timeout_ms)
{
    (void)channel_handle;
    (void)timeout_ms;
    g_recv_calls++;
    if (g_recv_result == RTE_STATUS_OK)
    {
        memset(data, 0x5A, data_size);
    }
    return g_recv_result;
}

static void reset_mock(void)
{
    g_send_calls = 0;
    g_recv_calls = 0;
    g_send_result = RTE_STATUS_OK;
    g_recv_result = RTE_STATUS_OK;
}

static void test_init_validation(void)
{
    rte_channel_storage_t storage;
    rte_channel_config_t config;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;

    assert(rte_channel_init(NULL, &config) == RTE_STATUS_INVALID_PARAM);
    assert(rte_channel_init(&storage, NULL) == RTE_STATUS_INVALID_PARAM);

    config.send = NULL;
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_INVALID_PARAM);
    config.send = mock_send;

    config.recv = NULL;
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_INVALID_PARAM);
    config.recv = mock_recv;

    assert(rte_channel_init(&storage, &config) == RTE_STATUS_OK);

    {
        rte_channel_health_t health;
        assert(rte_channel_get_health(&storage, &health) == RTE_STATUS_OK);
        assert(health.send_count == 0U);
        assert(health.receive_count == 0U);
        assert(health.is_healthy);
    }
}

static void test_send_dispatch(void)
{
    rte_channel_storage_t storage;
    rte_channel_config_t config;
    uint8_t payload[4] = {1, 2, 3, 4};
    rte_channel_health_t health;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_OK);

    assert(rte_channel_send(NULL, payload, sizeof(payload)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_channel_send(&storage, NULL, sizeof(payload)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_channel_send(&storage, payload, 0U) == RTE_STATUS_INVALID_PARAM);

    reset_mock();
    assert(rte_channel_send(&storage, payload, sizeof(payload)) == RTE_STATUS_OK);
    assert(g_send_calls == 1);
    assert(memcmp(g_last_sent, payload, sizeof(payload)) == 0);
    assert(g_last_sent_size == sizeof(payload));
    assert(rte_channel_get_health(&storage, &health) == RTE_STATUS_OK);
    assert(health.send_count == 1U);
    assert(health.send_error_count == 0U);

    reset_mock();
    g_send_result = RTE_STATUS_HARDWARE_FAULT;
    assert(rte_channel_send(&storage, payload, sizeof(payload)) == RTE_STATUS_HARDWARE_FAULT);
    assert(rte_channel_get_health(&storage, &health) == RTE_STATUS_OK);
    assert(health.send_error_count == 1U);
    assert(health.last_error == RTE_STATUS_HARDWARE_FAULT);
}

static void test_receive_dispatch(void)
{
    rte_channel_storage_t storage;
    rte_channel_config_t config;
    uint8_t buf[4] = {0};
    rte_channel_health_t health;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_OK);

    assert(rte_channel_receive(NULL, buf, sizeof(buf), 100U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_channel_receive(&storage, NULL, sizeof(buf), 100U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_channel_receive(&storage, buf, 0U, 100U) == RTE_STATUS_INVALID_PARAM);

    reset_mock();
    assert(rte_channel_receive(&storage, buf, sizeof(buf), 100U) == RTE_STATUS_OK);
    assert(g_recv_calls == 1);
    assert(buf[0] == 0x5AU);
    assert(rte_channel_get_health(&storage, &health) == RTE_STATUS_OK);
    assert(health.receive_count == 1U);

    reset_mock();
    g_recv_result = RTE_STATUS_TIMEOUT;
    assert(rte_channel_receive(&storage, buf, sizeof(buf), 100U) == RTE_STATUS_TIMEOUT);
    assert(rte_channel_get_health(&storage, &health) == RTE_STATUS_OK);
    assert(health.receive_error_count == 1U);
    assert(health.last_error == RTE_STATUS_TIMEOUT);
}

static void test_not_initialized(void)
{
    rte_channel_storage_t storage;
    uint8_t buf[4] = {0};

    memset(&storage, 0, sizeof(storage)); /* never rte_channel_init()-ed */

    assert(rte_channel_send(&storage, buf, sizeof(buf)) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_channel_receive(&storage, buf, sizeof(buf), 100U) == RTE_STATUS_NOT_INITIALIZED);
}

static void test_set_healthy_and_get_health_validation(void)
{
    rte_channel_storage_t storage;
    rte_channel_config_t config;
    rte_channel_health_t health;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_OK);

    assert(rte_channel_get_health(NULL, &health) == RTE_STATUS_INVALID_PARAM);
    assert(rte_channel_get_health(&storage, NULL) == RTE_STATUS_INVALID_PARAM);

    assert(rte_channel_set_healthy(NULL, false) == RTE_STATUS_INVALID_PARAM);
    assert(rte_channel_set_healthy(&storage, false) == RTE_STATUS_OK);
    assert(rte_channel_get_health(&storage, &health) == RTE_STATUS_OK);
    assert(!health.is_healthy);

    assert(rte_channel_set_healthy(&storage, true) == RTE_STATUS_OK);
    assert(rte_channel_get_health(&storage, &health) == RTE_STATUS_OK);
    assert(health.is_healthy);
}

/* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
 * locked, rte_channel_init() refuses even with a fully-valid config. */
static void test_lifecycle_lock(void)
{
    rte_channel_storage_t storage;
    rte_channel_config_t config;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;

    rte_lifecycle_lock();
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_INVALID_STATE);
    rte_lifecycle_unlock();
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_OK);
}

static void test_destroy(void)
{
    rte_channel_storage_t storage;
    rte_channel_config_t config;

    memset(&config, 0, sizeof(config));
    config.send = mock_send;
    config.recv = mock_recv;
    assert(rte_channel_init(&storage, &config) == RTE_STATUS_OK);

    assert(rte_channel_destroy(NULL) == RTE_STATUS_OK);
    assert(rte_channel_destroy(&storage) == RTE_STATUS_OK);

    {
        uint8_t buf[4] = {0};
        assert(rte_channel_send(&storage, buf, sizeof(buf)) == RTE_STATUS_NOT_INITIALIZED);
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
