/* Tests for sapi_voter (ADR-025): N-way voting over registered
 * sapi_channel_t links. */
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "safeapi/redundancy/voter/sapi_voter.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi/utils/safestate/sapi_safestate.h"

/* --- Mock channel backend, mirroring tests/vital_channel's pattern --- */

typedef struct
{
    uint8_t recv_buffer[8];
    sapi_status_t send_status;
    sapi_status_t recv_status;
    uint32_t send_calls;
    uint32_t recv_calls;
} mock_channel_t;

/* 9, not SAPI_VOTER_MAX_CHANNELS (8): test_register_channel() needs one
 * extra mock backend beyond the max to exercise the
 * SAPI_STATUS_RESOURCE_EXHAUSTED path at the 9th registration attempt
 * (channels[8]/g_mock[8]) - an 8-element array here was a real
 * out-of-bounds write, only caught once ASan instrumentation
 * (SAFEAPI_ENABLE_ASAN) was added to the build. */
static mock_channel_t g_mock[9];

static sapi_status_t mock_send(void *channel_handle, const void *data, size_t data_size)
{
    mock_channel_t *ch = (mock_channel_t *)channel_handle;
    (void)data;
    (void)data_size;
    ch->send_calls++;
    return ch->send_status;
}

static sapi_status_t mock_recv(void *channel_handle, void *data, size_t data_size, uint32_t timeout_ms)
{
    mock_channel_t *ch = (mock_channel_t *)channel_handle;
    (void)timeout_ms;
    ch->recv_calls++;
    if (ch->recv_status == SAPI_STATUS_OK)
    {
        memcpy(data, ch->recv_buffer, data_size);
    }
    return ch->recv_status;
}

static void reset_mocks(uint32_t count)
{
    uint32_t i;
    for (i = 0U; i < count; i++)
    {
        memset(&g_mock[i], 0, sizeof(g_mock[i]));
        g_mock[i].send_status = SAPI_STATUS_OK;
        g_mock[i].recv_status = SAPI_STATUS_OK;
    }
}

static void init_channel(sapi_channel_storage_t *storage, uint32_t idx)
{
    sapi_channel_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.channel_handle = &g_mock[idx];
    cfg.send = mock_send;
    cfg.recv = mock_recv;
    assert(sapi_channel_init(storage, &cfg) == SAPI_STATUS_OK);
}

/* --- Safe-state divert plumbing (same pattern used elsewhere in this repo) --- */
static jmp_buf g_jmp;
static sapi_safestate_level_t g_captured_level;
static int g_handler_calls;

static void diverting_handler(sapi_safestate_level_t level, sapi_safestate_reason_t reason,
                               const char *file, int32_t line, const char *message)
{
    (void)reason;
    (void)file;
    (void)line;
    (void)message;
    g_captured_level = level;
    g_handler_calls++;
    longjmp(g_jmp, 1);
}

static int g_disagreement_calls;
static sapi_voting_result_t g_last_disagreement_result;

static void disagreement_callback(void *context, sapi_voting_result_t result)
{
    (void)context;
    g_disagreement_calls++;
    g_last_disagreement_result = result;
}

static bool always_equal_compare(const void *a, const void *b, size_t size, void *ctx)
{
    (void)a;
    (void)b;
    (void)size;
    (void)ctx;
    return true;
}

static void test_init_validation(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;

    assert(sapi_voter_init(NULL, &cfg) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_init(&storage, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    cfg.voting_strategy = SAPI_VOTING_NMR;
    cfg.quorum_size = 0U;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_INVALID_PARAM);
    cfg.quorum_size = 2U;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    cfg.voting_strategy = (sapi_voting_strategy_t)99;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_INVALID_PARAM);
}

static void test_register_channel(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t channels[9];
    uint32_t i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_NMR;
    cfg.quorum_size = 1U;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(9);
    for (i = 0U; i < 9U; i++)
    {
        init_channel(&channels[i], i);
    }

    assert(sapi_voter_register_channel(NULL, &channels[0]) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_register_channel(&storage, NULL) == SAPI_STATUS_INVALID_PARAM);

    for (i = 0U; i < SAPI_VOTER_MAX_CHANNELS; i++)
    {
        assert(sapi_voter_register_channel(&storage, &channels[i]) == SAPI_STATUS_OK);
    }
    assert(sapi_voter_get_channel_count(&storage) == SAPI_VOTER_MAX_CHANNELS);
    /* 9th registration exceeds SAPI_VOTER_MAX_CHANNELS (8). */
    assert(sapi_voter_register_channel(&storage, &channels[8]) == SAPI_STATUS_RESOURCE_EXHAUSTED);

    {
        sapi_voter_storage_t not_init;
        memset(&not_init, 0, sizeof(not_init));
        assert(sapi_voter_register_channel(&not_init, &channels[0]) == SAPI_STATUS_NOT_INITIALIZED);
    }

    assert(sapi_voter_get_channel_count(NULL) == 0U);
    assert(sapi_voter_get_channel(NULL, 0U) == NULL);
    assert(sapi_voter_get_channel(&storage, SAPI_VOTER_MAX_CHANNELS) == NULL);
    assert(sapi_voter_get_channel(&storage, 0U) == &channels[0]);
}

static void test_send_2oo2(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[2];
    uint8_t payload[4] = {1, 2, 3, 4};

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);

    /* Wrong registered-channel count for the fixed 2OO2 strategy. */
    assert(sapi_voter_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    assert(sapi_voter_send(NULL, payload, sizeof(payload)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_send(&storage, NULL, sizeof(payload)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_send(&storage, payload, 0U) == SAPI_STATUS_INVALID_PARAM);
    {
        uint8_t oversized[SAPI_VOTER_MAX_MESSAGE_SIZE + 1U];
        assert(sapi_voter_send(&storage, oversized, sizeof(oversized)) == SAPI_STATUS_RESOURCE_EXHAUSTED);
    }

    assert(sapi_voter_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_OK);
    assert(g_mock[0].send_calls == 1U);
    assert(g_mock[1].send_calls == 1U);

    /* One channel marked unhealthy: skipped, still succeeds via the other. */
    assert(sapi_channel_set_healthy(&ch[1], false) == SAPI_STATUS_OK);
    assert(sapi_voter_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_OK);
    assert(g_mock[1].send_calls == 1U); /* unchanged - skipped */
    assert(sapi_channel_set_healthy(&ch[1], true) == SAPI_STATUS_OK);

    /* Both unhealthy: HARDWARE_FAULT, nothing sent. */
    assert(sapi_channel_set_healthy(&ch[0], false) == SAPI_STATUS_OK);
    assert(sapi_channel_set_healthy(&ch[1], false) == SAPI_STATUS_OK);
    assert(sapi_voter_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_HARDWARE_FAULT);
    assert(sapi_channel_set_healthy(&ch[0], true) == SAPI_STATUS_OK);
    assert(sapi_channel_set_healthy(&ch[1], true) == SAPI_STATUS_OK);

    /* A timeout on one channel surfaces as TIMEOUT overall. */
    g_mock[0].send_status = SAPI_STATUS_TIMEOUT;
    assert(sapi_voter_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_TIMEOUT);
    g_mock[0].send_status = SAPI_STATUS_OK;

    /* A non-timeout failure surfaces as HARDWARE_FAULT. */
    g_mock[0].send_status = SAPI_STATUS_HARDWARE_FAULT;
    assert(sapi_voter_send(&storage, payload, sizeof(payload)) == SAPI_STATUS_HARDWARE_FAULT);
}

static void test_receive_2oo2_agreement(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[2];
    uint8_t data[4];
    sapi_voting_result_t result;
    size_t bytes_received = 0U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_voter_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0x42, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x42, sizeof(g_mock[1].recv_buffer));

    assert(sapi_voter_receive(NULL, data, sizeof(data), &result, &bytes_received) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_receive(&storage, NULL, sizeof(data), &result, &bytes_received)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_voter_receive(&storage, data, 0U, &result, &bytes_received) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, &bytes_received) == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);
    assert(bytes_received == sizeof(data));
    assert(data[0] == 0x42U);

    uint32_t healthy = 0U;
    uint32_t disagreements = 0U;
    assert(sapi_voter_get_aggregated_health(&storage, &healthy, &disagreements) == SAPI_STATUS_OK);
    assert(healthy == 2U);
    assert(disagreements == 0U);
    assert(sapi_voter_get_aggregated_health(NULL, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_receive_majority_vote_fix(void)
{
    /* The bug this module fixes vs. the pre-ADR-025 vital_channel: with 3
     * channels where B and C agree but A disagrees, the real majority (B,
     * C) must win, not a "compare everyone against the first" scheme that
     * would report DISAGREED here. */
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[3];
    uint8_t data[4];
    sapi_voting_result_t result;
    size_t bytes_received = 0U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO3;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(3);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    init_channel(&ch[2], 2U);
    assert(sapi_voter_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[2]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer)); /* A: disagrees */
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer)); /* B: majority */
    memset(g_mock[2].recv_buffer, 0xBB, sizeof(g_mock[2].recv_buffer)); /* C: majority */

    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, &bytes_received) == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);
    assert(data[0] == 0xBBU); /* the majority value, not channel 0's */
}

static void test_receive_disagreement_triggers_safestate(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[2];
    uint8_t data[4];

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;
    cfg.log_disagreements = true;
    cfg.trigger_safestate_on_disagreement = true;
    cfg.on_disagreement = disagreement_callback;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_voter_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer));

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler) == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_voter_receive(&storage, data, sizeof(data), NULL, NULL);
        assert(0); /* must never return here */
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == SAPI_SAFESTATE_LEVEL_SAFE);
    }

    {
        uint32_t healthy = 0U;
        uint32_t disagreements = 0U;
        assert(sapi_voter_get_aggregated_health(&storage, &healthy, &disagreements) == SAPI_STATUS_OK);
        assert(disagreements == 1U);
    }
}

static void test_receive_disagreement_no_safestate_calls_callback(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[2];
    uint8_t data[4];
    sapi_voting_result_t result;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;
    cfg.trigger_safestate_on_disagreement = false;
    cfg.on_disagreement = disagreement_callback;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_voter_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer));

    g_disagreement_calls = 0;
    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, NULL) == SAPI_STATUS_HARDWARE_FAULT);
    assert(result == SAPI_VOTING_DISAGREED);
    assert(g_disagreement_calls == 1);
    assert(g_last_disagreement_result == SAPI_VOTING_DISAGREED);
}

static void test_receive_timeout_and_insufficient_quorum(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[3];
    uint8_t data[4];
    sapi_voting_result_t result;
    size_t bytes_received = 123U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO3;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(3);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    init_channel(&ch[2], 2U);
    assert(sapi_voter_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[2]) == SAPI_STATUS_OK);

    /* Pre-check: 2 of 3 unhealthy -> healthy_count (1) < required quorum (2). */
    assert(sapi_channel_set_healthy(&ch[0], false) == SAPI_STATUS_OK);
    assert(sapi_channel_set_healthy(&ch[1], false) == SAPI_STATUS_OK);
    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, &bytes_received)
           == SAPI_STATUS_HARDWARE_FAULT);
    assert(result == SAPI_VOTING_INSUFFICIENT_QUORUM);
    assert(bytes_received == 0U);
    assert(sapi_channel_set_healthy(&ch[0], true) == SAPI_STATUS_OK);
    assert(sapi_channel_set_healthy(&ch[1], true) == SAPI_STATUS_OK);

    /* All healthy channels time out -> successful == 0 -> TIMEOUT. */
    g_mock[0].recv_status = SAPI_STATUS_TIMEOUT;
    g_mock[1].recv_status = SAPI_STATUS_TIMEOUT;
    g_mock[2].recv_status = SAPI_STATUS_TIMEOUT;
    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, &bytes_received)
           == SAPI_STATUS_HARDWARE_FAULT);
    assert(result == SAPI_VOTING_TIMEOUT);

    /* Exactly one succeeds (non-timeout failures for the rest) -> successful
     * (1) < required quorum (2) -> INSUFFICIENT_QUORUM, not TIMEOUT. */
    g_mock[0].recv_status = SAPI_STATUS_OK;
    g_mock[1].recv_status = SAPI_STATUS_HARDWARE_FAULT;
    g_mock[2].recv_status = SAPI_STATUS_HARDWARE_FAULT;
    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, &bytes_received)
           == SAPI_STATUS_HARDWARE_FAULT);
    assert(result == SAPI_VOTING_INSUFFICIENT_QUORUM);
}

static void test_custom_compare_fn(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[2];
    uint8_t data[4];
    sapi_voting_result_t result;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;
    cfg.compare = always_equal_compare;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_voter_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    /* Genuinely different payloads, but the custom compare always reports
     * equal - proves the custom callback, not memcmp, decided this. */
    memset(g_mock[0].recv_buffer, 0x11, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x99, sizeof(g_mock[1].recv_buffer));

    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, NULL) == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);
}

static void test_nmr_voting(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t ch[5];
    uint8_t data[4];
    sapi_voting_result_t result;
    uint32_t i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_NMR;
    cfg.quorum_size = 3U;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(5);
    for (i = 0U; i < 5U; i++)
    {
        init_channel(&ch[i], i);
        assert(sapi_voter_register_channel(&storage, &ch[i]) == SAPI_STATUS_OK);
    }

    /* 3 agree (quorum met exactly), 2 disagree with each other and the
     * majority. */
    memset(g_mock[0].recv_buffer, 0x77, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x77, sizeof(g_mock[1].recv_buffer));
    memset(g_mock[2].recv_buffer, 0x77, sizeof(g_mock[2].recv_buffer));
    memset(g_mock[3].recv_buffer, 0x88, sizeof(g_mock[3].recv_buffer));
    memset(g_mock[4].recv_buffer, 0x99, sizeof(g_mock[4].recv_buffer));

    assert(sapi_voter_receive(&storage, data, sizeof(data), &result, NULL) == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);
    assert(data[0] == 0x77U);
}

/* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
 * locked, sapi_voter_init()/_register_channel() refuse even with
 * fully-valid arguments. */
static void test_lifecycle_lock(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t channel;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(1);
    init_channel(&channel, 0);

    sapi_lifecycle_lock();
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_INVALID_STATE);
    assert(sapi_voter_register_channel(&storage, &channel) == SAPI_STATUS_INVALID_STATE);
    sapi_lifecycle_unlock();
    assert(sapi_voter_register_channel(&storage, &channel) == SAPI_STATUS_OK);
}

static void test_destroy(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_2OO2;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    assert(sapi_voter_destroy(NULL) == SAPI_STATUS_OK);
    assert(sapi_voter_destroy(&storage) == SAPI_STATUS_OK);
}

static void test_get_channel_by_name(void)
{
    sapi_voter_storage_t storage;
    sapi_voter_config_t cfg;
    sapi_channel_storage_t channels[3];
    sapi_channel_config_t chan_cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = SAPI_VOTING_NMR;
    cfg.quorum_size = 1U;
    assert(sapi_voter_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks(3);

    memset(&chan_cfg, 0, sizeof(chan_cfg));
    chan_cfg.channel_handle = &g_mock[0];
    chan_cfg.send = mock_send;
    chan_cfg.recv = mock_recv;
    chan_cfg.name = "ChannelAtoB";
    assert(sapi_channel_init(&channels[0], &chan_cfg) == SAPI_STATUS_OK);

    chan_cfg.channel_handle = &g_mock[1];
    chan_cfg.name = "ChannelAtoC";
    assert(sapi_channel_init(&channels[1], &chan_cfg) == SAPI_STATUS_OK);

    /* Deliberately unnamed - must never match any name lookup. */
    chan_cfg.channel_handle = &g_mock[2];
    chan_cfg.name = NULL;
    assert(sapi_channel_init(&channels[2], &chan_cfg) == SAPI_STATUS_OK);

    assert(sapi_voter_register_channel(&storage, &channels[0]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &channels[1]) == SAPI_STATUS_OK);
    assert(sapi_voter_register_channel(&storage, &channels[2]) == SAPI_STATUS_OK);

    assert(sapi_channel_get_name(&channels[0]) != NULL);
    assert(strcmp(sapi_channel_get_name(&channels[0]), "ChannelAtoB") == 0);
    assert(sapi_channel_get_name(&channels[2]) == NULL);

    assert(sapi_voter_get_channel_by_name(&storage, "ChannelAtoB") == &channels[0]);
    assert(sapi_voter_get_channel_by_name(&storage, "ChannelAtoC") == &channels[1]);
    /* Unknown name, NULL voter/name, and the deliberately-unnamed channel
     * (searched by its own NULL name) must all miss. */
    assert(sapi_voter_get_channel_by_name(&storage, "ChannelAtoZ") == NULL);
    assert(sapi_voter_get_channel_by_name(NULL, "ChannelAtoB") == NULL);
    assert(sapi_voter_get_channel_by_name(&storage, NULL) == NULL);
}

int main(void)
{
    test_init_validation();
    test_register_channel();
    test_get_channel_by_name();
    test_send_2oo2();
    test_receive_2oo2_agreement();
    test_receive_majority_vote_fix();
    test_receive_disagreement_triggers_safestate();
    test_receive_disagreement_no_safestate_calls_callback();
    test_receive_timeout_and_insufficient_quorum();
    test_custom_compare_fn();
    test_nmr_voting();
    test_lifecycle_lock();
    test_destroy();
    return 0;
}
