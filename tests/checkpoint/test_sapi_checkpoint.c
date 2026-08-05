/* Tests for sapi_checkpoint (ADR-017).
 *
 * Simulates two peer channels with an in-memory mailbox per channel
 * (no real transport needed to exercise the rendezvous/verification
 * logic): backend_send() writes the local arrival marker into the
 * peer's mailbox slot, and backend_recv() either returns a pre-seeded
 * "peer reply" or SAPI_STATUS_TIMEOUT if none was seeded, standing in
 * for "the peer never confirmed in time."
 *
 * sapi_channel_checkpoint() is documented as calling sapi_safestate_enter()
 * at SAPI_SAFESTATE_LEVEL_SAFE on an insufficient-confirmation timeout
 * (REQ-CHECKPOINT-003); as in test_sapi_safestate.c and
 * test_sapi_channel.c, a setjmp/longjmp diverting handler is used to
 * verify that path without hanging the test on the production infinite
 * loop.
 */
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "safeapi/checkpoint/sapi_checkpoint.h"
#include "safeapi/checksum/sapi_checksum.h"
#include "safeapi/safestate/sapi_safestate.h"

#define TEST_CHANNEL_COUNT 2U

static void *g_channel_handles[TEST_CHANNEL_COUNT];
static int g_channel_index[TEST_CHANNEL_COUNT];
static sapi_vital_message_t g_mailbox[TEST_CHANNEL_COUNT];
static bool g_reply_ready[TEST_CHANNEL_COUNT];

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

static int channel_index_of(void *channel)
{
    return *(const int *)channel;
}

static sapi_status_t mock_backend_send(void *channel, const void *data, size_t data_size)
{
    int idx = channel_index_of(channel);

    if (data_size != sizeof(sapi_vital_message_t))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* Sent messages aren't inspected by these tests directly - only
     * pre-seeded mailbox contents (below) stand in for peer replies. */
    (void)idx;
    (void)data;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_backend_recv(void *channel, void *data, size_t data_size, uint32_t timeout_ms)
{
    int idx = channel_index_of(channel);

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

static void clear_replies(void)
{
    size_t i;

    for (i = 0U; i < TEST_CHANNEL_COUNT; i++)
    {
        g_reply_ready[i] = false;
    }
}

static void init_vital_channel(sapi_vital_channel_t *storage)
{
    sapi_vital_channel_config_t config;

    memset(&config, 0, sizeof(config));
    config.voting_strategy = SAPI_VOTING_2OO2;
    config.channel_count = TEST_CHANNEL_COUNT;
    config.channel_timeout_ms = 50U;
    config.backend_send = mock_backend_send;
    config.backend_recv = mock_backend_recv;

    assert(sapi_vital_channel_init(storage, &config, g_channel_handles, TEST_CHANNEL_COUNT) == SAPI_STATUS_OK);
}

static void test_invalid_params(void)
{
    sapi_vital_channel_t vc;
    sapi_checkpoint_config_t cfg;

    init_vital_channel(&vc);
    cfg.checkpoint_id = 1U;
    cfg.max_delay_ms = 10U;
    cfg.expected_node_count = 1U;
    cfg.watchdog = NULL;

    assert(sapi_channel_checkpoint(NULL, &cfg) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_checkpoint(&vc, NULL) == SAPI_STATUS_INVALID_PARAM);

    cfg.expected_node_count = 0U;
    assert(sapi_channel_checkpoint(&vc, &cfg) == SAPI_STATUS_INVALID_PARAM);

    cfg.expected_node_count = TEST_CHANNEL_COUNT + 1U;
    assert(sapi_channel_checkpoint(&vc, &cfg) == SAPI_STATUS_INVALID_PARAM);
}

static void test_all_channels_confirm_succeeds(void)
{
    sapi_vital_channel_t vc;
    sapi_checkpoint_config_t cfg;

    init_vital_channel(&vc);
    clear_replies();
    seed_valid_reply(0, 42U);
    seed_valid_reply(1, 42U);

    cfg.checkpoint_id = 42U;
    cfg.max_delay_ms = 100U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    assert(sapi_channel_checkpoint(&vc, &cfg) == SAPI_STATUS_OK);
    /* Reaching this point at all proves safe-state was not entered. */
    assert(g_handler_calls == 0);
}

static void test_insufficient_confirmations_diverts(void)
{
    sapi_vital_channel_t vc;
    sapi_checkpoint_config_t cfg;

    init_vital_channel(&vc);
    clear_replies();
    seed_valid_reply(0, 7U);
    /* Channel 1 never replies -> backend_recv returns TIMEOUT for it. */

    cfg.checkpoint_id = 7U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = TEST_CHANNEL_COUNT;
    cfg.watchdog = NULL;

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler) == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_channel_checkpoint(&vc, &cfg);
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
    sapi_vital_channel_t vc;
    sapi_checkpoint_config_t cfg;

    init_vital_channel(&vc);
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
        (void)sapi_channel_checkpoint(&vc, &cfg);
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
    sapi_vital_channel_t vc;
    sapi_checkpoint_config_t cfg;

    init_vital_channel(&vc);
    clear_replies();
    seed_valid_reply(0, 3U);
    /* Channel 1 does not reply, but only 1 confirmation is required. */

    cfg.checkpoint_id = 3U;
    cfg.max_delay_ms = 20U;
    cfg.expected_node_count = 1U;
    cfg.watchdog = NULL;

    g_handler_calls = 0;
    assert(sapi_channel_checkpoint(&vc, &cfg) == SAPI_STATUS_OK);
    assert(g_handler_calls == 0);
}

int main(void)
{
    size_t i;

    assert(sapi_checksum_crc64_init(SAPI_CRC64_ERTMS) == SAPI_STATUS_OK);

    for (i = 0U; i < TEST_CHANNEL_COUNT; i++)
    {
        g_channel_index[i] = (int)i;
        g_channel_handles[i] = &g_channel_index[i];
    }

    test_invalid_params();
    test_all_channels_confirm_succeeds();
    test_insufficient_confirmations_diverts();
    test_wrong_checkpoint_id_does_not_count();
    test_partial_quorum_succeeds_when_sufficient();
    return 0;
}
