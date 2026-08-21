/* Tests for sapi_checkpoint (ADR-017, rewired onto sapi_voter_t by ADR-025).
 *
 * Simulates two peer channels with an in-memory mailbox per channel
 * (no real transport needed to exercise the rendezvous/verification
 * logic): send() writes the local arrival marker into the peer's mailbox
 * slot, and recv() either returns a pre-seeded "peer reply" or
 * SAPI_STATUS_TIMEOUT if none was seeded, standing in for "the peer
 * never confirmed in time."
 *
 * sapi_channel_checkpoint() is documented as calling sapi_safestate_enter()
 * at SAPI_SAFESTATE_LEVEL_SAFE on an insufficient-confirmation timeout
 * (REQ-CHECKPOINT-003); as elsewhere in this repo, a setjmp/longjmp
 * diverting handler is used to verify that path without hanging the test
 * on the production infinite loop.
 */
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "safeapi/checkpoint/sapi_checkpoint.h"
#include "safeapi/checksum/sapi_checksum.h"
#include "safeapi/safestate/sapi_safestate.h"
#include "safeapi/watchdog/sapi_watchdog.h"

#define TEST_CHANNEL_COUNT 2U

static int g_channel_index[TEST_CHANNEL_COUNT];
static sapi_vital_message_t g_mailbox[TEST_CHANNEL_COUNT];
static bool g_reply_ready[TEST_CHANNEL_COUNT];
static bool g_send_should_fail[TEST_CHANNEL_COUNT];

static jmp_buf g_jmp;
static sapi_safestate_level_t g_captured_level;
static sapi_safestate_reason_t g_captured_reason;
static int g_handler_calls;

static void diverting_handler(sapi_safestate_level_t level, sapi_safestate_reason_t reason, const char *file,
                               int32_t line, const char *message)
{
    (void)file;
    (void)line;
    (void)message;
    g_captured_level = level;
    g_captured_reason = reason;
    g_handler_calls++;
    longjmp(g_jmp, 1);
}

static int channel_index_of(void *channel_handle)
{
    return *(const int *)channel_handle;
}

static sapi_status_t mock_send(void *channel_handle, const void *data, size_t data_size)
{
    int idx = channel_index_of(channel_handle);

    if (data_size != sizeof(sapi_vital_message_t))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (g_send_should_fail[idx])
    {
        return SAPI_STATUS_HARDWARE_FAULT;
    }
    /* Sent messages aren't inspected by these tests directly - only
     * pre-seeded mailbox contents (below) stand in for peer replies. */
    (void)data;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_recv(void *channel_handle, void *data, size_t data_size, uint32_t timeout_ms)
{
    int idx = channel_index_of(channel_handle);

    (void)timeout_ms;
    if (data_size != sizeof(sapi_vital_message_t))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!g_reply_ready[idx])
    {
        return SAPI_STATUS_TIMEOUT;
    }
    (void)memcpy(data, &g_mailbox[idx], sizeof(sapi_vital_message_t));
    return SAPI_STATUS_OK;
}

static void seed_valid_reply(int idx, uint32_t checkpoint_id)
{
    uint8_t payload[4];

    payload[0] = (uint8_t)(checkpoint_id & 0xFFU);
    payload[1] = (uint8_t)((checkpoint_id >> 8) & 0xFFU);
    payload[2] = (uint8_t)((checkpoint_id >> 16) & 0xFFU);
    payload[3] = (uint8_t)((checkpoint_id >> 24) & 0xFFU);

    assert(sapi_checksum_vital_message_create(&g_mailbox[idx], 0xAAU, checkpoint_id, payload, sizeof(payload))
           == SAPI_STATUS_OK);
    g_reply_ready[idx] = true;
}

static void seed_wrong_checkpoint_reply(int idx, uint32_t wrong_checkpoint_id)
{
    seed_valid_reply(idx, wrong_checkpoint_id);
}

/* Passes sapi_checksum_vital_message_verify() (correct sequence number =
 * checkpoint_id, valid CRC) but with a short, 2-byte payload instead of
 * the usual 4 - covers reply_confirms_checkpoint()'s
 * payload_size_out == sizeof(payload_out) check's false arm, distinct
 * from a checksum/sequence failure (REQ-CHECKPOINT-002 covers wrong
 * checkpoint_id; this covers a malformed-but-correctly-addressed reply). */
static void seed_short_payload_reply(int idx, uint32_t checkpoint_id)
{
    uint8_t payload[2];

    payload[0] = (uint8_t)(checkpoint_id & 0xFFU);
    payload[1] = (uint8_t)((checkpoint_id >> 8) & 0xFFU);

    assert(sapi_checksum_vital_message_create(&g_mailbox[idx], 0xAAU, checkpoint_id, payload, sizeof(payload))
           == SAPI_STATUS_OK);
    g_reply_ready[idx] = true;
}

/* Passes sapi_checksum_vital_message_verify() with the *correct* sequence
 * number (checkpoint_id) and a full 4-byte payload, but the payload bytes
 * decode to a different id - covers reply_confirms_checkpoint()'s
 * decoded_id == checkpoint_id check's false arm. This is a different
 * failure mode than seed_wrong_checkpoint_reply(), which fails the
 * checksum/sequence check itself and never reaches the payload decode
 * comparison at all. */
static void seed_correct_sequence_wrong_payload_reply(int idx, uint32_t checkpoint_id, uint32_t encoded_id)
{
    uint8_t payload[4];

    payload[0] = (uint8_t)(encoded_id & 0xFFU);
    payload[1] = (uint8_t)((encoded_id >> 8) & 0xFFU);
    payload[2] = (uint8_t)((encoded_id >> 16) & 0xFFU);
    payload[3] = (uint8_t)((encoded_id >> 24) & 0xFFU);

    assert(sapi_checksum_vital_message_create(&g_mailbox[idx], 0xAAU, checkpoint_id, payload, sizeof(payload))
           == SAPI_STATUS_OK);
    g_reply_ready[idx] = true;
}

static void clear_replies(void)
{
    size_t i;

    for (i = 0U; i < TEST_CHANNEL_COUNT; i++)
    {
        g_reply_ready[i] = false;
        g_send_should_fail[i] = false;
    }
}

/* Builds a voter with TEST_CHANNEL_COUNT channels registered, backed by
 * the mock mailbox above - the fixture every test in this file uses. */
static void init_voter(sapi_channel_storage_t channels[TEST_CHANNEL_COUNT], sapi_voter_t *voter)
{
    sapi_voter_config_t voter_cfg;
    size_t i;

    memset(&voter_cfg, 0, sizeof(voter_cfg));
    voter_cfg.voting_strategy = SAPI_VOTING_2OO2;
    voter_cfg.channel_timeout_ms = 50U;
    assert(sapi_voter_init(voter, &voter_cfg) == SAPI_STATUS_OK);

    for (i = 0U; i < TEST_CHANNEL_COUNT; i++)
    {
        sapi_channel_config_t chan_cfg;

        memset(&chan_cfg, 0, sizeof(chan_cfg));
        chan_cfg.channel_handle = &g_channel_index[i];
        chan_cfg.send = mock_send;
        chan_cfg.recv = mock_recv;
        assert(sapi_channel_init(&channels[i], &chan_cfg) == SAPI_STATUS_OK);
        assert(sapi_voter_register_channel(voter, &channels[i]) == SAPI_STATUS_OK);
    }
}

static void test_invalid_params(void)
{
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    cfg.checkpoint_id = 1U;
    cfg.max_delay_ms = 10U;
    cfg.expected_node_count = 1U;
    cfg.watchdog = NULL;

    assert(sapi_channel_checkpoint(NULL, &cfg) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_checkpoint(&voter, NULL) == SAPI_STATUS_INVALID_PARAM);

    cfg.expected_node_count = 0U;
    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_INVALID_PARAM);

    cfg.expected_node_count = TEST_CHANNEL_COUNT + 1U;
    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_INVALID_PARAM);
}

static void test_voter_with_no_registered_channels(void)
{
    /* A voter with zero registered channels: expected_node_count (1) >
     * channel_count (0) is rejected before any I/O is attempted -
     * exercises sapi_voter_get_channel_count() returning 0. */
    sapi_voter_storage_t voter;
    sapi_voter_config_t voter_cfg;
    sapi_checkpoint_config_t cfg;

    memset(&voter_cfg, 0, sizeof(voter_cfg));
    voter_cfg.voting_strategy = SAPI_VOTING_NMR;
    voter_cfg.quorum_size = 1U;
    assert(sapi_voter_init(&voter, &voter_cfg) == SAPI_STATUS_OK);

    cfg.checkpoint_id = 1U;
    cfg.max_delay_ms = 10U;
    cfg.expected_node_count = 1U;
    cfg.watchdog = NULL;

    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_INVALID_PARAM);
}

static void test_all_channels_confirm_succeeds(void)
{
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 42U);
    seed_valid_reply(1, 42U);

    cfg.checkpoint_id = 42U;
    cfg.max_delay_ms = 100U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_OK);
    /* Reaching this point at all proves safe-state was not entered. */
    assert(g_handler_calls == 0);
}

static void test_insufficient_confirmations_diverts(void)
{
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 7U);
    /* Channel 1 never replies -> recv() returns TIMEOUT for it. */

    cfg.checkpoint_id = 7U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT;
    cfg.watchdog = NULL;

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler) == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_channel_checkpoint(&voter, &cfg);
        /* Must never reach here: an insufficient-confirmation checkpoint
         * always diverts via longjmp. */
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == SAPI_SAFESTATE_LEVEL_SAFE);
        assert(g_captured_reason == SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT);
    }
}

static void test_wrong_checkpoint_id_does_not_count(void)
{
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 9U);
    seed_wrong_checkpoint_reply(1, 999U); /* replies, but to the wrong checkpoint */

    cfg.checkpoint_id = 9U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT; /* requires both */
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_channel_checkpoint(&voter, &cfg);
        assert(0);
    }
    else
    {
        /* Only channel 0's reply should have counted; channel 1's
         * wrong-checkpoint reply must be rejected (REQ-CHECKPOINT-002),
         * leaving confirmed_count (1) below expected_node_count (2). */
        assert(g_handler_calls == 1);
        assert(g_captured_reason == SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT);
    }
}

static void test_partial_quorum_succeeds_when_sufficient(void)
{
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 3U);
    /* Channel 1 does not reply, but only 1 confirmation is required. */

    cfg.checkpoint_id = 3U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = 1U;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_OK);
    assert(g_handler_calls == 0);
}

static void test_short_payload_reply_does_not_count(void)
{
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 11U);
    seed_short_payload_reply(1, 11U);

    cfg.checkpoint_id = 11U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_channel_checkpoint(&voter, &cfg);
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_reason == SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT);
    }
}

static void test_correct_sequence_wrong_payload_does_not_count(void)
{
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 13U);
    seed_correct_sequence_wrong_payload_reply(1, 13U, 999U);

    cfg.checkpoint_id = 13U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_channel_checkpoint(&voter, &cfg);
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_reason == SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT);
    }
}

static void test_send_failure_skips_channel(void)
{
    /* Covers the send status == SAPI_STATUS_OK check's false arm: when
     * send() itself fails for a channel, that channel must never be
     * polled for a reply (and can never count toward quorum). */
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(1, 17U);
    g_send_should_fail[0] = true;

    cfg.checkpoint_id = 17U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = 1U;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_OK);
    assert(g_handler_calls == 0);
}

static void test_zero_budget_hits_expired_remaining(void)
{
    /* max_delay_ms == 0 makes remaining_budget_ms()'s elapsed_ms >= max_delay_ms
     * branch true on every iteration (elapsed_ms is unsigned, so it is
     * always >= 0) - covers the "budget already exhausted" arm that a
     * non-zero budget with fast in-process mocks never reaches. The mock
     * recv() ignores timeout_ms, so the already-seeded reply still
     * counts; only expected_node_count (1) needs to be met. */
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 55U);

    cfg.checkpoint_id = 55U;
    cfg.max_delay_ms = 0U;
    cfg.expected_node_count = 1U;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_OK);
    assert(g_handler_calls == 0);
}

static void test_watchdog_kicked_on_success(void)
{
    /* Covers the config->watchdog != NULL branch on a successful
     * rendezvous: sapi_watchdog_kick() must be invoked. */
    sapi_channel_storage_t channels[TEST_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_checkpoint_config_t cfg;
    sapi_watchdog_config_t wd_config;
    sapi_watchdog_t wd;
    sapi_watchdog_status_t wd_status;

    memset(&wd_config, 0, sizeof(wd_config));
    wd_config.type = SAPI_WATCHDOG_CHECKPOINT;
    wd_config.name = "test_checkpoint_wd";
    wd_config.timeout_ms = 10000U;
    wd_config.action = SAPI_WATCHDOG_ACTION_LOG;
    wd_config.custom_action = NULL;
    wd_config.context = NULL;
    assert(sapi_watchdog_manager_initialize() == SAPI_STATUS_OK);
    assert(sapi_watchdog_create(&wd, &wd_config) == SAPI_STATUS_OK);
    assert(sapi_watchdog_start(wd) == SAPI_STATUS_OK);

    init_voter(channels, &voter);
    clear_replies();
    seed_valid_reply(0, 21U);
    seed_valid_reply(1, 21U);

    cfg.checkpoint_id = 21U;
    cfg.max_delay_ms = 100U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT;
    cfg.watchdog = wd;

    g_handler_calls = 0;
    assert(sapi_channel_checkpoint(&voter, &cfg) == SAPI_STATUS_OK);
    assert(g_handler_calls == 0);

    assert(sapi_watchdog_get_status(wd, &wd_status) == SAPI_STATUS_OK);
    assert(wd_status.kicks == 1U);
}

int main(void)
{
    size_t i;

    assert(sapi_checksum_crc64_init(SAPI_CRC64_ERTMS) == SAPI_STATUS_OK);

    for (i = 0U; i < TEST_CHANNEL_COUNT; i++)
    {
        g_channel_index[i] = (int)i;
    }

    test_invalid_params();
    test_voter_with_no_registered_channels();
    test_all_channels_confirm_succeeds();
    test_insufficient_confirmations_diverts();
    test_wrong_checkpoint_id_does_not_count();
    test_partial_quorum_succeeds_when_sufficient();
    test_short_payload_reply_does_not_count();
    test_correct_sequence_wrong_payload_does_not_count();
    test_send_failure_skips_channel();
    test_zero_budget_hits_expired_remaining();
    test_watchdog_kicked_on_success();
    return 0;
}
