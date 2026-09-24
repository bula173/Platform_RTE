/* Tests for rte_voter (ADR-025): N-way voting over registered
 * rte_channel_t links. */
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "rte/redundancy/voter/rte_voter.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte/utils/safestate/rte_safestate.h"

/* --- Mock channel OSAdapter, mirroring tests/vital_channel's pattern --- */

typedef struct
{
    uint8_t recv_buffer[8];
    rte_status_t send_status;
    rte_status_t recv_status;
    uint32_t send_calls;
    uint32_t recv_calls;
} mock_channel_t;

/* 9, not RTE_VOTER_MAX_CHANNELS (8): test_register_channel() needs one
 * extra mock OSAdapter beyond the max to exercise the
 * RTE_STATUS_RESOURCE_EXHAUSTED path at the 9th registration attempt
 * (channels[8]/g_mock[8]) - an 8-element array here was a real
 * out-of-bounds write, only caught once ASan instrumentation
 * (RTE_ENABLE_ASAN) was added to the build. */
static mock_channel_t g_mock[9];

static rte_status_t mock_send(void *channel_handle, const void *data, size_t data_size)
{
    mock_channel_t *ch = (mock_channel_t *)channel_handle;
    (void)data;
    (void)data_size;
    ch->send_calls++;
    return ch->send_status;
}

static rte_status_t mock_recv(void *channel_handle, void *data, size_t data_size, uint32_t timeout_ms)
{
    mock_channel_t *ch = (mock_channel_t *)channel_handle;
    (void)timeout_ms;
    ch->recv_calls++;
    if (ch->recv_status == RTE_STATUS_OK)
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
        g_mock[i].send_status = RTE_STATUS_OK;
        g_mock[i].recv_status = RTE_STATUS_OK;
    }
}

static void init_channel(rte_channel_storage_t *storage, uint32_t idx)
{
    rte_channel_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.channel_handle = &g_mock[idx];
    cfg.send = mock_send;
    cfg.recv = mock_recv;
    assert(rte_channel_init(storage, &cfg) == RTE_STATUS_OK);
}

/* --- Safe-state divert plumbing (same pattern used elsewhere in this repo) --- */
static jmp_buf g_jmp;
static rte_safestate_level_t g_captured_level;
static int g_handler_calls;

static void diverting_handler(rte_safestate_level_t level, rte_safestate_reason_t reason,
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
static rte_voting_result_t g_last_disagreement_result;

static void disagreement_callback(void *context, rte_voting_result_t result)
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
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;

    assert(rte_voter_init(NULL, &cfg) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_init(&storage, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    cfg.voting_strategy = RTE_VOTING_NMR;
    cfg.quorum_size = 0U;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_INVALID_PARAM);
    cfg.quorum_size = 2U;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    cfg.voting_strategy = (rte_voting_strategy_t)99;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_INVALID_PARAM);
}

static void test_register_channel(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t channels[9];
    uint32_t i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_NMR;
    cfg.quorum_size = 1U;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(9);
    for (i = 0U; i < 9U; i++)
    {
        init_channel(&channels[i], i);
    }

    assert(rte_voter_register_channel(NULL, &channels[0]) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_register_channel(&storage, NULL) == RTE_STATUS_INVALID_PARAM);

    for (i = 0U; i < RTE_VOTER_MAX_CHANNELS; i++)
    {
        assert(rte_voter_register_channel(&storage, &channels[i]) == RTE_STATUS_OK);
    }
    assert(rte_voter_get_channel_count(&storage) == RTE_VOTER_MAX_CHANNELS);
    /* 9th registration exceeds RTE_VOTER_MAX_CHANNELS (8). */
    assert(rte_voter_register_channel(&storage, &channels[8]) == RTE_STATUS_RESOURCE_EXHAUSTED);

    {
        rte_voter_storage_t not_init;
        memset(&not_init, 0, sizeof(not_init));
        assert(rte_voter_register_channel(&not_init, &channels[0]) == RTE_STATUS_NOT_INITIALIZED);
    }

    assert(rte_voter_get_channel_count(NULL) == 0U);
    assert(rte_voter_get_channel(NULL, 0U) == NULL);
    assert(rte_voter_get_channel(&storage, RTE_VOTER_MAX_CHANNELS) == NULL);
    assert(rte_voter_get_channel(&storage, 0U) == &channels[0]);
}

static void test_send_2oo2(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[2];
    uint8_t payload[4] = {1, 2, 3, 4};

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);

    /* Wrong registered-channel count for the fixed 2OO2 strategy. */
    assert(rte_voter_register_channel(&storage, &ch[0]) == RTE_STATUS_OK);
    assert(rte_voter_send(&storage, payload, sizeof(payload)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_register_channel(&storage, &ch[1]) == RTE_STATUS_OK);

    assert(rte_voter_send(NULL, payload, sizeof(payload)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_send(&storage, NULL, sizeof(payload)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_send(&storage, payload, 0U) == RTE_STATUS_INVALID_PARAM);
    {
        uint8_t oversized[RTE_VOTER_MAX_MESSAGE_SIZE + 1U];
        assert(rte_voter_send(&storage, oversized, sizeof(oversized)) == RTE_STATUS_RESOURCE_EXHAUSTED);
    }

    assert(rte_voter_send(&storage, payload, sizeof(payload)) == RTE_STATUS_OK);
    assert(g_mock[0].send_calls == 1U);
    assert(g_mock[1].send_calls == 1U);

    /* One channel marked unhealthy: skipped, still succeeds via the other. */
    assert(rte_channel_set_healthy(&ch[1], false) == RTE_STATUS_OK);
    assert(rte_voter_send(&storage, payload, sizeof(payload)) == RTE_STATUS_OK);
    assert(g_mock[1].send_calls == 1U); /* unchanged - skipped */
    assert(rte_channel_set_healthy(&ch[1], true) == RTE_STATUS_OK);

    /* Both unhealthy: HARDWARE_FAULT, nothing sent. */
    assert(rte_channel_set_healthy(&ch[0], false) == RTE_STATUS_OK);
    assert(rte_channel_set_healthy(&ch[1], false) == RTE_STATUS_OK);
    assert(rte_voter_send(&storage, payload, sizeof(payload)) == RTE_STATUS_HARDWARE_FAULT);
    assert(rte_channel_set_healthy(&ch[0], true) == RTE_STATUS_OK);
    assert(rte_channel_set_healthy(&ch[1], true) == RTE_STATUS_OK);

    /* A timeout on one channel surfaces as TIMEOUT overall. */
    g_mock[0].send_status = RTE_STATUS_TIMEOUT;
    assert(rte_voter_send(&storage, payload, sizeof(payload)) == RTE_STATUS_TIMEOUT);
    g_mock[0].send_status = RTE_STATUS_OK;

    /* A non-timeout failure surfaces as HARDWARE_FAULT. */
    g_mock[0].send_status = RTE_STATUS_HARDWARE_FAULT;
    assert(rte_voter_send(&storage, payload, sizeof(payload)) == RTE_STATUS_HARDWARE_FAULT);
}

static void test_receive_2oo2_agreement(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[2];
    uint8_t data[4];
    rte_voting_result_t result;
    size_t bytes_received = 0U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(rte_voter_register_channel(&storage, &ch[0]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[1]) == RTE_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0x42, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x42, sizeof(g_mock[1].recv_buffer));

    assert(rte_voter_receive(NULL, data, sizeof(data), &result, &bytes_received) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_receive(&storage, NULL, sizeof(data), &result, &bytes_received)
           == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_receive(&storage, data, 0U, &result, &bytes_received) == RTE_STATUS_INVALID_PARAM);

    assert(rte_voter_receive(&storage, data, sizeof(data), &result, &bytes_received) == RTE_STATUS_OK);
    assert(result == RTE_VOTING_AGREED);
    assert(bytes_received == sizeof(data));
    assert(data[0] == 0x42U);

    uint32_t healthy = 0U;
    uint32_t disagreements = 0U;
    assert(rte_voter_get_aggregated_health(&storage, &healthy, &disagreements) == RTE_STATUS_OK);
    assert(healthy == 2U);
    assert(disagreements == 0U);
    assert(rte_voter_get_aggregated_health(NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);
}

static void test_receive_majority_vote_fix(void)
{
    /* The bug this module fixes vs. the pre-ADR-025 vital_channel: with 3
     * channels where B and C agree but A disagrees, the real majority (B,
     * C) must win, not a "compare everyone against the first" scheme that
     * would report DISAGREED here. */
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[3];
    uint8_t data[4];
    rte_voting_result_t result;
    size_t bytes_received = 0U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO3;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(3);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    init_channel(&ch[2], 2U);
    assert(rte_voter_register_channel(&storage, &ch[0]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[1]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[2]) == RTE_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer)); /* A: disagrees */
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer)); /* B: majority */
    memset(g_mock[2].recv_buffer, 0xBB, sizeof(g_mock[2].recv_buffer)); /* C: majority */

    assert(rte_voter_receive(&storage, data, sizeof(data), &result, &bytes_received) == RTE_STATUS_OK);
    assert(result == RTE_VOTING_AGREED);
    assert(data[0] == 0xBBU); /* the majority value, not channel 0's */
}

static void test_receive_disagreement_triggers_safestate(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[2];
    uint8_t data[4];

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    cfg.log_disagreements = true;
    cfg.safestate_level = RTE_SAFESTATE_LEVEL_SAFE;
    cfg.safestate_reason = RTE_SAFESTATE_REASON_UNSPECIFIED;
    cfg.on_disagreement = disagreement_callback;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(rte_voter_register_channel(&storage, &ch[0]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[1]) == RTE_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer));

    assert(rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_SAFE, diverting_handler) == RTE_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)rte_voter_receive(&storage, data, sizeof(data), NULL, NULL);
        assert(0); /* must never return here */
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == RTE_SAFESTATE_LEVEL_SAFE);
    }

    {
        uint32_t healthy = 0U;
        uint32_t disagreements = 0U;
        assert(rte_voter_get_aggregated_health(&storage, &healthy, &disagreements) == RTE_STATUS_OK);
        assert(disagreements == 1U);
    }
}

/* Per the RCA/OCORA PI-API compatibility change (rte_voter.h's own
 * note), an application can no longer opt out of the safestate
 * transition on disagreement - the Platform always owns it. What it CAN
 * still configure is which level/reason, and it is guaranteed its own
 * on_disagreement callback runs first. This test exercises a
 * REBOOT-level configuration (as safeAPIRBC2oo2GP's own cross-compare
 * path uses via rte_cross_comparator, which shares this exact same
 * mechanism) and verifies the callback-then-transition ordering. */
static void test_receive_disagreement_triggers_reboot_level(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[2];
    uint8_t data[4];

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    cfg.safestate_level = RTE_SAFESTATE_LEVEL_REBOOT;
    cfg.safestate_reason = (rte_safestate_reason_t)(RTE_SAFESTATE_REASON_APPLICATION_BASE + 1U);
    cfg.on_disagreement = disagreement_callback;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(rte_voter_register_channel(&storage, &ch[0]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[1]) == RTE_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer));

    assert(rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_REBOOT, diverting_handler) == RTE_STATUS_OK);

    g_disagreement_calls = 0;
    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)rte_voter_receive(&storage, data, sizeof(data), NULL, NULL);
        assert(0); /* must never return here */
    }
    else
    {
        /* on_disagreement ran BEFORE the transition. */
        assert(g_disagreement_calls == 1);
        assert(g_last_disagreement_result == RTE_VOTING_DISAGREED);
        assert(g_handler_calls == 1);
        assert(g_captured_level == RTE_SAFESTATE_LEVEL_REBOOT);
    }
}

static void test_receive_timeout_and_insufficient_quorum(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[3];
    uint8_t data[4];
    rte_voting_result_t result;
    size_t bytes_received = 123U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO3;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(3);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    init_channel(&ch[2], 2U);
    assert(rte_voter_register_channel(&storage, &ch[0]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[1]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[2]) == RTE_STATUS_OK);

    /* Pre-check: 2 of 3 unhealthy -> healthy_count (1) < required quorum (2). */
    assert(rte_channel_set_healthy(&ch[0], false) == RTE_STATUS_OK);
    assert(rte_channel_set_healthy(&ch[1], false) == RTE_STATUS_OK);
    assert(rte_voter_receive(&storage, data, sizeof(data), &result, &bytes_received)
           == RTE_STATUS_HARDWARE_FAULT);
    assert(result == RTE_VOTING_INSUFFICIENT_QUORUM);
    assert(bytes_received == 0U);
    assert(rte_channel_set_healthy(&ch[0], true) == RTE_STATUS_OK);
    assert(rte_channel_set_healthy(&ch[1], true) == RTE_STATUS_OK);

    /* All healthy channels time out -> successful == 0 -> TIMEOUT. */
    g_mock[0].recv_status = RTE_STATUS_TIMEOUT;
    g_mock[1].recv_status = RTE_STATUS_TIMEOUT;
    g_mock[2].recv_status = RTE_STATUS_TIMEOUT;
    assert(rte_voter_receive(&storage, data, sizeof(data), &result, &bytes_received)
           == RTE_STATUS_HARDWARE_FAULT);
    assert(result == RTE_VOTING_TIMEOUT);

    /* Exactly one succeeds (non-timeout failures for the rest) -> successful
     * (1) < required quorum (2) -> INSUFFICIENT_QUORUM, not TIMEOUT. */
    g_mock[0].recv_status = RTE_STATUS_OK;
    g_mock[1].recv_status = RTE_STATUS_HARDWARE_FAULT;
    g_mock[2].recv_status = RTE_STATUS_HARDWARE_FAULT;
    assert(rte_voter_receive(&storage, data, sizeof(data), &result, &bytes_received)
           == RTE_STATUS_HARDWARE_FAULT);
    assert(result == RTE_VOTING_INSUFFICIENT_QUORUM);
}

static void test_custom_compare_fn(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[2];
    uint8_t data[4];
    rte_voting_result_t result;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    cfg.compare = always_equal_compare;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(2);
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(rte_voter_register_channel(&storage, &ch[0]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &ch[1]) == RTE_STATUS_OK);

    /* Genuinely different payloads, but the custom compare always reports
     * equal - proves the custom callback, not memcmp, decided this. */
    memset(g_mock[0].recv_buffer, 0x11, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x99, sizeof(g_mock[1].recv_buffer));

    assert(rte_voter_receive(&storage, data, sizeof(data), &result, NULL) == RTE_STATUS_OK);
    assert(result == RTE_VOTING_AGREED);
}

static void test_nmr_voting(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t ch[5];
    uint8_t data[4];
    rte_voting_result_t result;
    uint32_t i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_NMR;
    cfg.quorum_size = 3U;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(5);
    for (i = 0U; i < 5U; i++)
    {
        init_channel(&ch[i], i);
        assert(rte_voter_register_channel(&storage, &ch[i]) == RTE_STATUS_OK);
    }

    /* 3 agree (quorum met exactly), 2 disagree with each other and the
     * majority. */
    memset(g_mock[0].recv_buffer, 0x77, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x77, sizeof(g_mock[1].recv_buffer));
    memset(g_mock[2].recv_buffer, 0x77, sizeof(g_mock[2].recv_buffer));
    memset(g_mock[3].recv_buffer, 0x88, sizeof(g_mock[3].recv_buffer));
    memset(g_mock[4].recv_buffer, 0x99, sizeof(g_mock[4].recv_buffer));

    assert(rte_voter_receive(&storage, data, sizeof(data), &result, NULL) == RTE_STATUS_OK);
    assert(result == RTE_VOTING_AGREED);
    assert(data[0] == 0x77U);
}

/* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
 * locked, rte_voter_init()/_register_channel() refuse even with
 * fully-valid arguments. */
static void test_lifecycle_lock(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t channel;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(1);
    init_channel(&channel, 0);

    rte_lifecycle_lock();
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_INVALID_STATE);
    assert(rte_voter_register_channel(&storage, &channel) == RTE_STATUS_INVALID_STATE);
    rte_lifecycle_unlock();
    assert(rte_voter_register_channel(&storage, &channel) == RTE_STATUS_OK);
}

static void test_destroy(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    assert(rte_voter_destroy(NULL) == RTE_STATUS_OK);
    assert(rte_voter_destroy(&storage) == RTE_STATUS_OK);
}

static void test_get_channel_by_name(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    rte_channel_storage_t channels[3];
    rte_channel_config_t chan_cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_NMR;
    cfg.quorum_size = 1U;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    reset_mocks(3);

    memset(&chan_cfg, 0, sizeof(chan_cfg));
    chan_cfg.channel_handle = &g_mock[0];
    chan_cfg.send = mock_send;
    chan_cfg.recv = mock_recv;
    chan_cfg.name = "ChannelAtoB";
    assert(rte_channel_init(&channels[0], &chan_cfg) == RTE_STATUS_OK);

    chan_cfg.channel_handle = &g_mock[1];
    chan_cfg.name = "ChannelAtoC";
    assert(rte_channel_init(&channels[1], &chan_cfg) == RTE_STATUS_OK);

    /* Deliberately unnamed - must never match any name lookup. */
    chan_cfg.channel_handle = &g_mock[2];
    chan_cfg.name = NULL;
    assert(rte_channel_init(&channels[2], &chan_cfg) == RTE_STATUS_OK);

    assert(rte_voter_register_channel(&storage, &channels[0]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &channels[1]) == RTE_STATUS_OK);
    assert(rte_voter_register_channel(&storage, &channels[2]) == RTE_STATUS_OK);

    assert(rte_channel_get_name(&channels[0]) != NULL);
    assert(strcmp(rte_channel_get_name(&channels[0]), "ChannelAtoB") == 0);
    assert(rte_channel_get_name(&channels[2]) == NULL);

    assert(rte_voter_get_channel_by_name(&storage, "ChannelAtoB") == &channels[0]);
    assert(rte_voter_get_channel_by_name(&storage, "ChannelAtoC") == &channels[1]);
    /* Unknown name, NULL voter/name, and the deliberately-unnamed channel
     * (searched by its own NULL name) must all miss. */
    assert(rte_voter_get_channel_by_name(&storage, "ChannelAtoZ") == NULL);
    assert(rte_voter_get_channel_by_name(NULL, "ChannelAtoB") == NULL);
    assert(rte_voter_get_channel_by_name(&storage, NULL) == NULL);
}

/* --- rte_voter_vote_buffers() (ADR-039's N-way cross-compare half): no channel I/O at all -
 * these tests never register a channel, matching rte_cross_comparator_execute_buffers()'s own
 * "a voter with zero registered channels is a legitimate call shape" posture. */

static void test_vote_buffers_2oo2_agree(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    uint8_t a[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    uint8_t b[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    const void *bufs[2] = {a, b};
    uint8_t out[4] = {0U};
    rte_voting_result_t result;
    size_t out_size = 0U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    assert(rte_voter_vote_buffers(&storage, bufs, 2U, sizeof(a), &result, out, &out_size) == RTE_STATUS_OK);
    assert(result == RTE_VOTING_AGREED);
    assert(out_size == sizeof(a));
    assert(memcmp(out, a, sizeof(a)) == 0);
}

static void test_vote_buffers_nmr_majority(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    uint8_t v0[4];
    uint8_t v1[4];
    uint8_t v2[4];
    uint8_t v3[4];
    uint8_t v4[4];
    const void *bufs[5];
    uint8_t out[4] = {0U};
    rte_voting_result_t result;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_NMR;
    cfg.quorum_size = 3U;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    /* 3 agree (quorum met exactly), 2 disagree with each other and the majority - mirrors
     * test_nmr_voting()'s own registered-channel scenario, but via buffers. */
    memset(v0, 0x77U, sizeof(v0));
    memset(v1, 0x77U, sizeof(v1));
    memset(v2, 0x77U, sizeof(v2));
    memset(v3, 0x88U, sizeof(v3));
    memset(v4, 0x99U, sizeof(v4));
    bufs[0] = v0;
    bufs[1] = v1;
    bufs[2] = v2;
    bufs[3] = v3;
    bufs[4] = v4;

    assert(rte_voter_vote_buffers(&storage, bufs, 5U, sizeof(v0), &result, out, NULL) == RTE_STATUS_OK);
    assert(result == RTE_VOTING_AGREED);
    assert(out[0] == 0x77U);
}

static void test_vote_buffers_disagreement_triggers_safestate(void)
{
    rte_voter_storage_t storage;
    rte_voter_config_t cfg;
    uint8_t a[4];
    uint8_t b[4];
    const void *bufs[2];

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    cfg.log_disagreements = true;
    cfg.safestate_level = RTE_SAFESTATE_LEVEL_SAFE;
    cfg.safestate_reason = RTE_SAFESTATE_REASON_UNSPECIFIED;
    cfg.on_disagreement = disagreement_callback;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);

    memset(a, 0xAAU, sizeof(a));
    memset(b, 0xBBU, sizeof(b));
    bufs[0] = a;
    bufs[1] = b;

    /* diverting_handler is already registered for RTE_SAFESTATE_LEVEL_SAFE by
     * test_receive_disagreement_triggers_safestate() above, and stays registered - this test
     * (like every diverting test in this file) must set its OWN g_jmp immediately before the
     * call that may divert, never relying on a stale one (root ISSUES.md's own
     * test_rte_checkpoint finding: a longjmp to a dead stack frame is undefined behavior). */
    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)rte_voter_vote_buffers(&storage, bufs, 2U, sizeof(a), NULL, NULL, NULL);
        assert(0); /* must never return here */
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == RTE_SAFESTATE_LEVEL_SAFE);
    }

    {
        uint32_t disagreements = 0U;
        assert(rte_voter_get_aggregated_health(&storage, NULL, &disagreements) == RTE_STATUS_OK);
        assert(disagreements == 1U);
    }
}

static void test_vote_buffers_invalid_params(void)
{
    rte_voter_storage_t storage;
    rte_voter_storage_t not_init;
    rte_voter_config_t cfg;
    uint8_t a[4] = {0U};
    uint8_t b[4] = {0U};
    const void *bufs[2] = {a, b};
    const void *bufs_with_null[2] = {a, NULL};
    const void *too_many[(size_t)RTE_VOTER_MAX_CHANNELS + 1U];
    uint8_t filler[4] = {0U};
    uint32_t i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.voting_strategy = RTE_VOTING_2OO2;
    assert(rte_voter_init(&storage, &cfg) == RTE_STATUS_OK);
    memset(&not_init, 0, sizeof(not_init));

    assert(rte_voter_vote_buffers(NULL, bufs, 2U, sizeof(a), NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_vote_buffers(&storage, NULL, 2U, sizeof(a), NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_vote_buffers(&storage, bufs, 2U, 0U, NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_vote_buffers(&storage, bufs, 2U, RTE_VOTER_MAX_MESSAGE_SIZE + 1U, NULL, NULL, NULL) ==
          RTE_STATUS_RESOURCE_EXHAUSTED);
    assert(rte_voter_vote_buffers(&not_init, bufs, 2U, sizeof(a), NULL, NULL, NULL) == RTE_STATUS_NOT_INITIALIZED);
    /* Wrong count for 2OO2 (needs exactly 2). */
    assert(rte_voter_vote_buffers(&storage, bufs, 1U, sizeof(a), NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_voter_vote_buffers(&storage, bufs_with_null, 2U, sizeof(a), NULL, NULL, NULL) ==
          RTE_STATUS_INVALID_PARAM);

    for (i = 0U; i < ((uint32_t)RTE_VOTER_MAX_CHANNELS + 1U); i++)
    {
        too_many[i] = filler;
    }
    assert(rte_voter_vote_buffers(&storage, too_many, (uint32_t)RTE_VOTER_MAX_CHANNELS + 1U, sizeof(filler), NULL,
                                   NULL, NULL) == RTE_STATUS_INVALID_PARAM);
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
    test_receive_disagreement_triggers_reboot_level();
    test_receive_timeout_and_insufficient_quorum();
    test_custom_compare_fn();
    test_nmr_voting();
    test_vote_buffers_2oo2_agree();
    test_vote_buffers_nmr_majority();
    test_vote_buffers_disagreement_triggers_safestate();
    test_vote_buffers_invalid_params();
    test_lifecycle_lock();
    test_destroy();
    return 0;
}
