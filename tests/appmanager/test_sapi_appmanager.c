/* Tests for sapi_appmanager (previously untested): lifecycle ordering,
 * max_iterations/error_threshold bounds, sapi_appmanager_request_shutdown(),
 * and the POSIX-only sapi_appmanager_install_default_signal_handlers()
 * convenience (see that function's own doc in sapi_appmanager.h for why
 * it's a deliberate, scoped exception to this framework's usual
 * OS-agnosticism).
 *
 * Also covers ADR-019: the optional pre_execute/post_execute cycle hooks
 * and the optional built-in checkpoint integration. The checkpoint tests
 * reuse the same in-memory-mailbox mock vital channel and setjmp/longjmp
 * SAFE-diverting handler pattern already established in
 * tests/checkpoint/test_sapi_checkpoint.c, for the same reason: a
 * checkpoint timeout calls sapi_safestate_enter() at
 * SAPI_SAFESTATE_LEVEL_SAFE, which this framework's shipped
 * sapi_safestate.c halts permanently (REQ-COMMON-SAFESTATE-002) - the
 * diverting handler is the only way to observe that path without hanging
 * the test suite.
 */
#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "safeapi/app/appmanager/sapi_appmanager.h"
#include "safeapi/redundancy/checksum/sapi_checksum.h"
#include "safeapi/utils/safestate/sapi_safestate.h"
#include "safeapi/redundancy/channel_link/sapi_channel.h"
#include "safeapi/redundancy/voter/sapi_voter.h"

/* ---- shared call-order/count tracking for the fake application under test ---- */
static int g_init_calls;
static int g_execute_calls;
static int g_shutdown_calls;
static int g_last_seen_state_in_execute;

static void reset_counters(void)
{
    g_init_calls = 0;
    g_execute_calls = 0;
    g_shutdown_calls = 0;
    g_last_seen_state_in_execute = -1;
}

static sapi_status_t fake_init_ok(void *context)
{
    (void)context;
    g_init_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t fake_init_fail(void *context)
{
    (void)context;
    g_init_calls++;
    return SAPI_STATUS_INTERNAL_ERROR;
}

static sapi_status_t fake_execute_ok(void *context)
{
    (void)context;
    g_execute_calls++;
    g_last_seen_state_in_execute = (int)sapi_appmanager_get_state();
    return SAPI_STATUS_OK;
}

static sapi_status_t fake_execute_always_fails(void *context)
{
    (void)context;
    g_execute_calls++;
    return SAPI_STATUS_INTERNAL_ERROR;
}

static sapi_status_t fake_execute_requests_shutdown_after_3(void *context)
{
    (void)context;
    g_execute_calls++;
    if (g_execute_calls >= 3)
    {
        sapi_appmanager_request_shutdown();
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t fake_shutdown(void *context)
{
    (void)context;
    g_shutdown_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t fake_shutdown_fails(void *context)
{
    (void)context;
    g_shutdown_calls++;
    return SAPI_STATUS_INTERNAL_ERROR;
}

static const char *fake_get_name(void)
{
    return "test_app";
}

static const char *fake_get_version(void)
{
    return "0.0.0";
}

static void test_config_validation(void)
{
    sapi_appmanager_config_t config;
    static const sapi_appmanager_operations_t incomplete_ops = {
        .init = NULL,
        .execute = fake_execute_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };

    assert(sapi_appmanager_run(NULL) == EXIT_FAILURE);

    config.ops = NULL;
    config.context = NULL;
    config.max_iterations = 1U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;
    assert(sapi_appmanager_run(&config) == EXIT_FAILURE);

    config.ops = &incomplete_ops;
    assert(sapi_appmanager_run(&config) == EXIT_FAILURE);
}

static void test_init_failure_still_calls_shutdown(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_fail,
        .execute = fake_execute_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 5U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_FAILURE);
    assert(g_init_calls == 1);
    assert(g_execute_calls == 0); /* init failed - execute must never run */
    assert(g_shutdown_calls == 1); /* shutdown always runs, even on init failure */
}

static void test_bounded_run_calls_in_order_with_correct_counts(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .execute = fake_execute_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 4U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_init_calls == 1);
    assert(g_execute_calls == 4); /* stopped exactly at max_iterations */
    assert(g_shutdown_calls == 1);
    assert(g_last_seen_state_in_execute == (int)SAPI_APP_STATE_RUNNING);
    assert(sapi_appmanager_get_state() == SAPI_APP_STATE_SHUTDOWN);
}

/* A non-OK shutdown() status is logged (stderr) but never fails the run
 * overall - sapi_appmanager_run()'s own comment says "Continue anyway;
 * shutdown must complete". */
static void test_shutdown_failure_is_logged_but_not_fatal(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .execute = fake_execute_ok,
        .shutdown = fake_shutdown_fails,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 2U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_shutdown_calls == 1);
    assert(sapi_appmanager_get_state() == SAPI_APP_STATE_SHUTDOWN);
}

static void test_error_threshold_stops_run_early(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .execute = fake_execute_always_fails,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;
    sapi_appmanager_state_t stats;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 100U; /* would never reach this - threshold hits first */
    config.error_threshold = 3U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_FAILURE);
    assert(g_execute_calls == 3); /* stopped as soon as the 3rd error was counted */
    assert(g_shutdown_calls == 1);

    assert(sapi_appmanager_get_stats(&stats) == SAPI_STATUS_OK);
    assert(stats.error_count == 3U);
    assert(stats.last_error == SAPI_STATUS_INTERNAL_ERROR);

    assert(sapi_appmanager_get_stats(NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_request_shutdown_stops_infinite_run(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .execute = fake_execute_requests_shutdown_after_3,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 0U; /* infinite - only request_shutdown() can end this */
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_execute_calls == 3); /* the fake requested shutdown on its 3rd call */
    assert(g_shutdown_calls == 1);
}

/* ---- sapi_appmanager_install_default_signal_handlers() ---- */

static int g_signal_execute_calls;

static sapi_status_t fake_execute_raises_sigint_once(void *context)
{
    (void)context;
    g_signal_execute_calls++;
    if (g_signal_execute_calls == 1)
    {
        /* Stands in for an external `kill -INT <pid>` / Ctrl+C: raise()
         * delivers the signal to this same process synchronously, so by
         * the time it returns the handler has already run. */
        (void)raise(SIGINT);
    }
    return SAPI_STATUS_OK;
}

static void test_install_default_signal_handlers(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .execute = fake_execute_raises_sigint_once,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    assert(sapi_appmanager_install_default_signal_handlers() == SAPI_STATUS_OK);
    /* Idempotent: installing again must not error. */
    assert(sapi_appmanager_install_default_signal_handlers() == SAPI_STATUS_OK);

    reset_counters();
    g_signal_execute_calls = 0;
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 0U; /* infinite - only the SIGINT-triggered shutdown should end this */
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    /* SIGINT was raised during the 1st execute() call; the app manager
     * checks g_shutdown_requested at the top of its loop (see
     * sapi_appmanager.c), so the run ends after that 1st iteration
     * completes, not mid-iteration. */
    assert(g_signal_execute_calls == 1);
    assert(g_shutdown_calls == 1);

    /* Restore default disposition so later tests in this same process
     * (or a future SIGINT this process might legitimately receive, e.g.
     * from a test runner's own Ctrl+C) are not left pointed at a fake
     * application's now out-of-scope operations table. */
    (void)signal(SIGINT, SIG_DFL);
    (void)signal(SIGTERM, SIG_DFL);
}

/* ---- ADR-019: pre_execute/post_execute cycle hooks ---- */

#define TEST_HOOK_ORDER_MAX 32
static char g_hook_order[TEST_HOOK_ORDER_MAX + 1];
static int g_hook_order_len;
static int g_pre_calls;
static int g_post_calls;
static int g_on_checkpoint_result_calls;
static int g_on_checkpoint_result_committed_true_calls;

static void reset_hook_tracking(void)
{
    /* Zero the whole buffer, not just index 0: a shorter run's appended
     * characters would otherwise overwrite only the buffer's front and
     * leave a stale, later NUL terminator from a longer previous test's
     * string in place, making strcmp() read past the current run's real
     * content into that leftover tail. */
    memset(g_hook_order, 0, sizeof(g_hook_order));
    g_hook_order_len = 0;
    g_pre_calls = 0;
    g_post_calls = 0;
    g_on_checkpoint_result_calls = 0;
    g_on_checkpoint_result_committed_true_calls = 0;
}

/** @brief ops->on_checkpoint_result (ADR-034) tracking hook shared by the
 *         checkpoint-integration tests below. */
static sapi_status_t hook_on_checkpoint_result_tracks(void *context, bool committed)
{
    (void)context;
    g_on_checkpoint_result_calls++;
    if (committed)
    {
        g_on_checkpoint_result_committed_true_calls++;
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t hook_pre_ok(void *context)
{
    (void)context;
    g_pre_calls++;
    if (g_hook_order_len < TEST_HOOK_ORDER_MAX) { g_hook_order[g_hook_order_len++] = 'P'; }
    return SAPI_STATUS_OK;
}

static sapi_status_t hook_execute_ok(void *context)
{
    (void)context;
    g_execute_calls++;
    if (g_hook_order_len < TEST_HOOK_ORDER_MAX) { g_hook_order[g_hook_order_len++] = 'E'; }
    return SAPI_STATUS_OK;
}

static sapi_status_t hook_post_ok(void *context)
{
    (void)context;
    g_post_calls++;
    if (g_hook_order_len < TEST_HOOK_ORDER_MAX) { g_hook_order[g_hook_order_len++] = 'O'; }
    return SAPI_STATUS_OK;
}

static sapi_status_t hook_pre_fails(void *context)
{
    (void)context;
    g_pre_calls++;
    if (g_hook_order_len < TEST_HOOK_ORDER_MAX) { g_hook_order[g_hook_order_len++] = 'p'; }
    return SAPI_STATUS_INTERNAL_ERROR;
}

static sapi_status_t hook_execute_fails(void *context)
{
    (void)context;
    g_execute_calls++;
    if (g_hook_order_len < TEST_HOOK_ORDER_MAX) { g_hook_order[g_hook_order_len++] = 'e'; }
    return SAPI_STATUS_INTERNAL_ERROR;
}

static void test_pre_post_hooks_called_in_order(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .pre_execute = hook_pre_ok,
        .execute = hook_execute_ok,
        .post_execute = hook_post_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 3U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_pre_calls == 3);
    assert(g_execute_calls == 3);
    assert(g_post_calls == 3);
    assert(strcmp(g_hook_order, "PEOPEOPEO") == 0);
}

static void test_hooks_left_null_is_backward_compatible(void)
{
    /* Same operations table shape a pre-ADR-019 application still uses:
     * pre_execute/post_execute left at their designated-initializer
     * default of NULL. Must behave exactly like execute()-only always
     * has - stages are silently skipped, not treated as an error. */
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .execute = hook_execute_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 3U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_pre_calls == 0);
    assert(g_execute_calls == 3);
    assert(g_post_calls == 0);
    assert(strcmp(g_hook_order, "EEE") == 0);
}

static void test_pre_execute_failure_skips_execute_and_post(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .pre_execute = hook_pre_fails,
        .execute = hook_execute_ok,
        .post_execute = hook_post_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;
    sapi_appmanager_state_t stats;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 2U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_pre_calls == 2);
    assert(g_execute_calls == 0); /* never reached - pre_execute failed both cycles */
    assert(g_post_calls == 0);
    assert(strcmp(g_hook_order, "pp") == 0);

    assert(sapi_appmanager_get_stats(&stats) == SAPI_STATUS_OK);
    assert(stats.error_count == 2U);
    assert(stats.last_error == SAPI_STATUS_INTERNAL_ERROR);
}

static void test_execute_failure_skips_post_execute(void)
{
    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .pre_execute = hook_pre_ok,
        .execute = hook_execute_fails,
        .post_execute = hook_post_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 2U;
    config.error_threshold = 0U;
    config.checkpoint = NULL;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_pre_calls == 2);
    assert(g_execute_calls == 2);
    assert(g_post_calls == 0); /* execute() failed both cycles - post_execute never reached */
    assert(strcmp(g_hook_order, "PePe") == 0);
}

/* ---- ADR-019: built-in checkpoint integration ----
 *
 * Reuses the same in-memory-mailbox mock vital channel approach as
 * tests/checkpoint/test_sapi_checkpoint.c: backend_send()/backend_recv()
 * never touch a real transport, only a pre-seeded per-channel reply slot.
 */
#define TEST_CP_CHANNEL_COUNT 2U

static void *g_cp_channel_handles[TEST_CP_CHANNEL_COUNT];
static int g_cp_channel_index[TEST_CP_CHANNEL_COUNT];
static sapi_vital_message_t g_cp_mailbox[TEST_CP_CHANNEL_COUNT];
static bool g_cp_reply_ready[TEST_CP_CHANNEL_COUNT];

static jmp_buf g_cp_jmp;
static sapi_safestate_level_t g_cp_captured_level;
static sapi_safestate_reason_t g_cp_captured_reason;
static int g_cp_handler_calls;

static void cp_diverting_handler(sapi_safestate_level_t level, sapi_safestate_reason_t reason,
                                  const char *file, int32_t line, const char *message)
{
    (void)file;
    (void)line;
    (void)message;
    g_cp_captured_level = level;
    g_cp_captured_reason = reason;
    g_cp_handler_calls++;
    longjmp(g_cp_jmp, 1);
}

static int cp_channel_index_of(void *channel)
{
    return *(const int *)channel;
}

static sapi_status_t cp_mock_backend_send(void *channel, const void *data, size_t data_size)
{
    (void)cp_channel_index_of(channel);
    (void)data;
    if (data_size != sizeof(sapi_vital_message_t))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t cp_mock_backend_recv(void *channel, void *data, size_t data_size, uint32_t timeout_ms)
{
    int idx = cp_channel_index_of(channel);

    (void)timeout_ms;
    if (data_size != sizeof(sapi_vital_message_t))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!g_cp_reply_ready[idx])
    {
        return SAPI_STATUS_TIMEOUT;
    }
    (void)memcpy(data, &g_cp_mailbox[idx], sizeof(sapi_vital_message_t));
    return SAPI_STATUS_OK;
}

static void cp_seed_valid_reply(int idx, uint32_t checkpoint_id)
{
    uint8_t payload[4];

    payload[0] = (uint8_t)(checkpoint_id & 0xFFU);
    payload[1] = (uint8_t)((checkpoint_id >> 8) & 0xFFU);
    payload[2] = (uint8_t)((checkpoint_id >> 16) & 0xFFU);
    payload[3] = (uint8_t)((checkpoint_id >> 24) & 0xFFU);

    assert(sapi_checksum_vital_message_create(&g_cp_mailbox[idx], 0xAAU, checkpoint_id, payload, sizeof(payload))
           == SAPI_STATUS_OK);
    g_cp_reply_ready[idx] = true;
}

static void cp_clear_replies(void)
{
    size_t i;

    for (i = 0U; i < TEST_CP_CHANNEL_COUNT; i++)
    {
        g_cp_reply_ready[i] = false;
    }
}

/* Builds a voter with TEST_CP_CHANNEL_COUNT channels registered, backed
 * by the mock mailbox above - the fixture the checkpoint-integration
 * tests below use. `channels` must outlive `voter` (voter only stores
 * pointers to the registered channel storage). */
static void cp_init_voter(sapi_channel_t channels[TEST_CP_CHANNEL_COUNT], sapi_voter_t *voter)
{
    sapi_voter_config_t voter_cfg;
    size_t i;

    memset(&voter_cfg, 0, sizeof(voter_cfg));
    voter_cfg.voting_strategy = SAPI_VOTING_2OO2;
    voter_cfg.channel_timeout_ms = 50U;
    assert(sapi_voter_init(voter, &voter_cfg) == SAPI_STATUS_OK);

    for (i = 0U; i < TEST_CP_CHANNEL_COUNT; i++)
    {
        sapi_channel_config_t chan_cfg;

        memset(&chan_cfg, 0, sizeof(chan_cfg));
        chan_cfg.channel_handle = g_cp_channel_handles[i];
        chan_cfg.send = cp_mock_backend_send;
        chan_cfg.recv = cp_mock_backend_recv;
        assert(sapi_channel_init(&channels[i], &chan_cfg) == SAPI_STATUS_OK);
        assert(sapi_voter_register_channel(voter, &channels[i]) == SAPI_STATUS_OK);
    }
}

/* Replicates sapi_appmanager_checkpoint_mark()'s own fold algorithm
 * (encode_le(signature) || encode_le(mark_hash), then CRC64) so a test
 * can compute, independently, the exact checkpoint_id a given sequence of
 * marks will produce - verifies the real algorithm end-to-end (via the
 * same public sapi_checksum_crc64()) rather than just trusting it.
 * Deliberately kept in lockstep with sapi_appmanager.c's own (private)
 * implementation: if that algorithm ever changes, this helper (and the
 * tests using it) must change with it - exactly the point, a silent
 * behavior change here should fail loudly, not pass by coincidence. */
static uint64_t cp_replicate_mark_fold(uint64_t signature, const char *text)
{
    uint8_t buf[16];
    sapi_crc64_t mark_hash = sapi_checksum_crc64((const uint8_t *)text, strlen(text));
    size_t i;

    for (i = 0U; i < 8U; i++)
    {
        buf[i] = (uint8_t)((signature >> (8U * i)) & 0xFFU);
    }
    for (i = 0U; i < 8U; i++)
    {
        buf[8U + i] = (uint8_t)(((uint64_t)mark_hash >> (8U * i)) & 0xFFU);
    }
    return (uint64_t)sapi_checksum_crc64(buf, sizeof(buf));
}

static void test_checkpoint_fold_signature_is_deterministic(void)
{
    uint32_t seed_fold_a = sapi_appmanager_checkpoint_fold_signature(SAPI_APPMANAGER_CHECKPOINT_SIGNATURE_SEED);
    uint32_t seed_fold_b = sapi_appmanager_checkpoint_fold_signature(SAPI_APPMANAGER_CHECKPOINT_SIGNATURE_SEED);
    uint32_t other_fold = sapi_appmanager_checkpoint_fold_signature(0x1234567890ABCDEFULL);

    /* The seed's own two 32-bit halves are equal (0xFFFFFFFF ^ 0xFFFFFFFF),
     * so a cycle with zero marks always computes checkpoint_id == 0 -
     * documented (see the SEED macro's own doc), not an accident. */
    assert(seed_fold_a == 0U);
    assert(seed_fold_a == seed_fold_b);
    assert(seed_fold_a != other_fold);
}

static void test_checkpoint_mark_sequence_is_order_sensitive(void)
{
    uint64_t after_a_then_b = cp_replicate_mark_fold(
        cp_replicate_mark_fold(SAPI_APPMANAGER_CHECKPOINT_SIGNATURE_SEED, "mark-a"), "mark-b");
    uint64_t after_b_then_a = cp_replicate_mark_fold(
        cp_replicate_mark_fold(SAPI_APPMANAGER_CHECKPOINT_SIGNATURE_SEED, "mark-b"), "mark-a");
    uint64_t after_a_only = cp_replicate_mark_fold(SAPI_APPMANAGER_CHECKPOINT_SIGNATURE_SEED, "mark-a");

    /* Same two marks, different ORDER -> different signature: proves the
     * fold is sensitive to program-path order, not just to the SET of
     * marks hit - the whole point of ADR-034 ("did A and B take the same
     * program path", not just "the same set of code locations"). */
    assert(sapi_appmanager_checkpoint_fold_signature(after_a_then_b)
           != sapi_appmanager_checkpoint_fold_signature(after_b_then_a));
    assert(sapi_appmanager_checkpoint_fold_signature(after_a_then_b)
           != sapi_appmanager_checkpoint_fold_signature(after_a_only));
}

static sapi_status_t hook_execute_marks_a_then_b(void *context)
{
    (void)context;
    g_execute_calls++;
    SAPI_CHECKPOINT_MARK_LABEL("mark-a");
    SAPI_CHECKPOINT_MARK_LABEL("mark-b");
    return SAPI_STATUS_OK;
}

static void test_checkpoint_success_runs_after_post_execute_each_cycle(void)
{
    sapi_channel_t vc[TEST_CP_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_appmanager_checkpoint_config_t cp_cfg;
    sapi_appmanager_config_t config;
    uint32_t expected_id;

    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .pre_execute = hook_pre_ok,
        .execute = hook_execute_marks_a_then_b,
        .on_checkpoint_result = hook_on_checkpoint_result_tracks,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };

    cp_init_voter(vc, &voter);
    cp_clear_replies();

    /* Every cycle makes the exact same two marks, in the same order (this
     * ops table has no other mark call sites), so the folded checkpoint_id
     * is the SAME constant every cycle - seed the mailbox once, not
     * per-cycle (unlike the old iteration_count scheme this replaced,
     * which needed re-seeding every cycle to stay in sync). */
    expected_id = sapi_appmanager_checkpoint_fold_signature(
        cp_replicate_mark_fold(
            cp_replicate_mark_fold(SAPI_APPMANAGER_CHECKPOINT_SIGNATURE_SEED, "mark-a"), "mark-b"));
    cp_seed_valid_reply(0, expected_id);
    cp_seed_valid_reply(1, expected_id);

    cp_cfg.voter = &voter;
    cp_cfg.max_delay_ms = 100U;
    cp_cfg.expected_node_count = TEST_CP_CHANNEL_COUNT;
    cp_cfg.watchdog = NULL;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 3U;
    config.error_threshold = 0U;
    config.checkpoint = &cp_cfg;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    /* Every cycle's checkpoint succeeded (same marks -> same signature
     * every time), so pre_execute/execute both ran all 3 times and no
     * error was ever counted. */
    assert(g_pre_calls == 3);
    assert(g_execute_calls == 3);
    assert(g_on_checkpoint_result_calls == 3);
    assert(g_on_checkpoint_result_committed_true_calls == 3);

    sapi_appmanager_state_t stats;
    assert(sapi_appmanager_get_stats(&stats) == SAPI_STATUS_OK);
    assert(stats.error_count == 0U);
}

/* Regression test for a real integration bug found while wiring this
 * feature into safeAPIRBC2oo2's channel_ab.c: sapi_appmanager_run() must
 * NOT reject a config whose checkpoint->voter is NULL at the moment it
 * is first called - a realistic integrator populates that target inside
 * their OWN init() (e.g. a checkpoint transport that isn't opened/
 * sapi_voter_init()-ed until application startup), which runs AFTER
 * this validation would otherwise have already rejected it. Also covers
 * toggling voter back to NULL mid-run (e.g. to pause checkpointing while
 * an underlying transport is known down).
 *
 * Originally asserted that each such cycle was handled "like any other
 * recoverable stage failure" (counted as an error, execute() skipped) -
 * that assertion was itself wrong, and encoded the exact starvation bug
 * test_checkpoint_paused_still_calls_on_checkpoint_result_committed()
 * below now covers: treating a deliberately-paused checkpoint as a
 * FAILURE (rather than simply not-applicable-this-cycle, the same as
 * checkpoint == NULL) meant execute() - and every other per-cycle safety
 * check, including sapi_watchdog_timer_tick() - never ran for as long as
 * voter stayed NULL. Updated to assert the corrected behavior: no error
 * recorded, execute() runs every cycle regardless of voter's own
 * NULL-ness, and (ADR-034) on_checkpoint_result still fires with
 * committed=true every cycle despite checkpointing being paused. */
static void test_checkpoint_null_vital_channel_is_not_a_startup_error(void)
{
    sapi_appmanager_checkpoint_config_t cp_cfg;
    sapi_appmanager_config_t config;

    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .execute = hook_execute_ok,
        .on_checkpoint_result = hook_on_checkpoint_result_tracks,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };

    cp_cfg.voter = NULL; /* not yet populated - simulates "init() would set this, but hasn't run yet" */
    cp_cfg.max_delay_ms = 20U;
    cp_cfg.expected_node_count = 1U;
    cp_cfg.watchdog = NULL;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 3U;
    config.error_threshold = 0U;
    config.checkpoint = &cp_cfg;

    /* Must NOT return EXIT_FAILURE just because vital_channel is NULL at
     * call time - that used to be rejected up front, incorrectly. */
    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    /* Checkpoint being paused (voter == NULL) is not-applicable-this-
     * cycle, not a failure - execute() runs every cycle regardless. */
    assert(g_execute_calls == 3);
    assert(g_on_checkpoint_result_calls == 3);
    assert(g_on_checkpoint_result_committed_true_calls == 3);

    sapi_appmanager_state_t stats;
    assert(sapi_appmanager_get_stats(&stats) == SAPI_STATUS_OK);
    assert(stats.error_count == 0U);
}

/** ADR-034: checkpoint now runs LAST, after pre_execute()/execute()/
 *  post_execute() have all had a chance to run and fold their own marks
 *  into this cycle's signature - so an unconfirmed checkpoint no longer
 *  pre-empts those stages the way it used to (the old test this replaced
 *  was named "...before_pre_execute" and asserted the opposite: zero
 *  pre_execute/execute calls). Renamed and rewritten accordingly. */
static void test_checkpoint_timeout_diverts_after_cycle_runs_once(void)
{
    sapi_channel_t vc[TEST_CP_CHANNEL_COUNT];
    sapi_voter_t voter;
    sapi_appmanager_checkpoint_config_t cp_cfg;
    sapi_appmanager_config_t config;

    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .pre_execute = hook_pre_ok,
        .execute = hook_execute_ok,
        .post_execute = hook_post_ok,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };

    cp_init_voter(vc, &voter);
    cp_clear_replies(); /* neither channel replies - checkpoint always fails */

    cp_cfg.voter = &voter;
    cp_cfg.max_delay_ms = 20U;
    cp_cfg.expected_node_count = TEST_CP_CHANNEL_COUNT;
    cp_cfg.watchdog = NULL;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 0U; /* would run forever - the diverting handler ends this, not a limit */
    config.error_threshold = 0U;
    config.checkpoint = &cp_cfg;

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, cp_diverting_handler) == SAPI_STATUS_OK);

    g_cp_handler_calls = 0;
    if (setjmp(g_cp_jmp) == 0)
    {
        (void)sapi_appmanager_run(&config);
        /* Must never reach here: an unconfirmed checkpoint always
         * diverts via longjmp before sapi_appmanager_run() can return. */
        assert(0);
    }
    else
    {
        assert(g_cp_handler_calls == 1);
        assert(g_cp_captured_level == SAPI_SAFESTATE_LEVEL_SAFE);
        assert(g_cp_captured_reason == SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT);
        /* The checkpoint now runs LAST (ADR-034) - pre_execute/execute/
         * post_execute all ran exactly ONCE (this cycle's own work) before
         * the checkpoint failed and diverted control away. */
        assert(g_pre_calls == 1);
        assert(g_execute_calls == 1);
        assert(g_post_calls == 1);
        /* This longjmp() is what makes this level's own never-returns
         * contract (REQ-COMMON-SAFESTATE-002) testable at all - but it
         * means sapi_appmanager_run() above never reached its own
         * SHUTDOWN phase, so its internal lifecycle state (and the
         * ADR-026 setup-phase lock) is left stuck at RUNNING/locked.
         * See sapi_appmanager_reset_state()'s own doc - this is exactly
         * the case it exists for. */
        sapi_appmanager_reset_state();
    }
}

/** ADR-034 replaces the old starvation-regression test this covered
 *  ("test_checkpoint_paused_skips_stage_not_starves_cycle", and the
 *  sapi_appmanager_pace_failed_checkpoint() mechanism it exercised) - that
 *  bug can no longer occur BY CONSTRUCTION now that checkpoint always
 *  runs LAST: pre_execute()/execute()/post_execute() already ran before
 *  checkpoint could ever pace or skip anything, paused or not. What's
 *  left to verify here is the ADR-034 contract itself: a paused (voter ==
 *  NULL) checkpoint still calls ops->on_checkpoint_result() every cycle,
 *  always with committed=true (see that field's own doc - "does not have
 *  to special-case whether checkpointing happened to be active"). */
static void test_checkpoint_paused_still_calls_on_checkpoint_result_committed(void)
{
    sapi_appmanager_checkpoint_config_t cp_cfg;
    sapi_appmanager_config_t config;

    static const sapi_appmanager_operations_t ops = {
        .init = fake_init_ok,
        .pre_execute = hook_pre_ok,
        .execute = hook_execute_ok,
        .post_execute = hook_post_ok,
        .on_checkpoint_result = hook_on_checkpoint_result_tracks,
        .shutdown = fake_shutdown,
        .get_name = fake_get_name,
        .get_version = fake_get_version
    };

    cp_cfg.voter = NULL; /* paused - must be skipped outright, not fed to sapi_channel_checkpoint() */
    cp_cfg.max_delay_ms = 20U;
    cp_cfg.expected_node_count = 1U;
    cp_cfg.watchdog = NULL;

    reset_counters();
    reset_hook_tracking();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 3U;
    config.error_threshold = 0U;
    config.checkpoint = &cp_cfg;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);

    assert(g_pre_calls == 3);
    assert(g_execute_calls == 3);
    assert(g_post_calls == 3);
    assert(g_on_checkpoint_result_calls == 3);
    assert(g_on_checkpoint_result_committed_true_calls == 3);
}

int main(void)
{
    assert(sapi_checksum_crc64_init(SAPI_CRC64_ERTMS) == SAPI_STATUS_OK);

    {
        size_t i;
        for (i = 0U; i < TEST_CP_CHANNEL_COUNT; i++)
        {
            g_cp_channel_index[i] = (int)i;
            g_cp_channel_handles[i] = &g_cp_channel_index[i];
        }
    }

    test_config_validation();
    test_init_failure_still_calls_shutdown();
    test_bounded_run_calls_in_order_with_correct_counts();
    test_shutdown_failure_is_logged_but_not_fatal();
    test_error_threshold_stops_run_early();
    test_request_shutdown_stops_infinite_run();
    test_install_default_signal_handlers();

    test_pre_post_hooks_called_in_order();
    test_hooks_left_null_is_backward_compatible();
    test_pre_execute_failure_skips_execute_and_post();
    test_execute_failure_skips_post_execute();
    test_checkpoint_fold_signature_is_deterministic();
    test_checkpoint_mark_sequence_is_order_sensitive();
    test_checkpoint_success_runs_after_post_execute_each_cycle();
    test_checkpoint_null_vital_channel_is_not_a_startup_error();
    test_checkpoint_timeout_diverts_after_cycle_runs_once();
    test_checkpoint_paused_still_calls_on_checkpoint_result_committed();
    return 0;
}
