/* Tests for the real sapi_watchdog implementation (replaces the previous
 * non-functional stub - see src/watchdog/sapi_watchdog.c file header).
 *
 * Uses a mock sapi_timer backend whose "now" function returns a
 * test-controlled counter (g_mock_now_ms) instead of a real clock, so
 * firing can be verified deterministically (advance the counter, call
 * sapi_watchdog_timer_tick()) without any real sleep/flakiness.
 *
 * The SAFESTATE/REBOOT actions are verified the same way
 * tests/safestate/test_sapi_safestate.c verifies SAPI_SAFESTATE_LEVEL_SAFE
 * itself never returns: a diverting handler using setjmp/longjmp stands in
 * for a real "drive outputs safe, then reset" handler.
 */
#include <assert.h>
#include <setjmp.h>
#include "safeapi/watchdog/sapi_watchdog.h"
#include "safeapi/safestate/sapi_safestate.h"
#include "safeapi/timer/sapi_timer.h"
#include "safeapi_backend/timer/sapi_timer_backend.h"

/* ---- mock timer backend: caller-controlled clock ---- */
static sapi_timestamp_ms_t g_mock_now_ms = 0U;

static sapi_status_t mock_timer_now(sapi_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = g_mock_now_ms;
    return SAPI_STATUS_OK;
}

static const sapi_timer_backend_t g_mock_timer_backend = {
    NULL, NULL, NULL, NULL, mock_timer_now
};

/* ---- diverting SAFE/REBOOT handler (never returns, like a real one) ---- */
static jmp_buf g_jmp;
static sapi_safestate_level_t g_captured_level;
static sapi_safestate_reason_t g_captured_reason;
static int g_diverting_calls;

static void diverting_handler(sapi_safestate_level_t level,
                               sapi_safestate_reason_t reason,
                               const char *file,
                               int32_t line,
                               const char *message)
{
    (void)file;
    (void)line;
    (void)message;
    g_captured_level = level;
    g_captured_reason = reason;
    g_diverting_calls++;
    longjmp(g_jmp, 1);
}

/* ---- CUSTOM action callback ---- */
static int g_custom_calls;
static void *g_custom_ctx_seen;

static void custom_action(void *ctx)
{
    g_custom_calls++;
    g_custom_ctx_seen = ctx;
}

static void test_create_requires_manager_initialized(void)
{
    sapi_watchdog_t wd = NULL;
    sapi_watchdog_config_t config = {0};
    config.type = SAPI_WATCHDOG_TASK;
    config.name = "pre_init_wd";
    config.timeout_ms = 100U;
    config.action = SAPI_WATCHDOG_ACTION_LOG;

    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_NOT_INITIALIZED);
}

static void test_create_param_validation(void)
{
    sapi_watchdog_t wd = NULL;
    sapi_watchdog_config_t config = {0};
    config.type = SAPI_WATCHDOG_TASK;
    config.name = "bad_wd";
    config.timeout_ms = 100U;
    config.action = SAPI_WATCHDOG_ACTION_LOG;

    assert(sapi_watchdog_create(NULL, &config) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_watchdog_create(&wd, NULL) == SAPI_STATUS_INVALID_PARAM);

    config.timeout_ms = 0U; /* zero timeout is not valid */
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_INVALID_PARAM);
    config.timeout_ms = 100U;

    config.action = (sapi_watchdog_action_t)99; /* out of enum range */
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_INVALID_PARAM);

    config.action = SAPI_WATCHDOG_ACTION_CUSTOM;
    config.custom_action = NULL; /* CUSTOM requires a callback */
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_INVALID_PARAM);
}

static void test_lifecycle_and_kick(void)
{
    sapi_watchdog_t wd = NULL;
    sapi_watchdog_config_t config = {0};
    sapi_watchdog_status_t status = {0};

    config.type = SAPI_WATCHDOG_TASK;
    config.name = "lifecycle_wd";
    config.timeout_ms = 1000U;
    config.action = SAPI_WATCHDOG_ACTION_LOG;

    g_mock_now_ms = 5000U;
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_OK);
    assert(wd != NULL);

    /* Created but not started: get_status still succeeds. */
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.active == 0U);
    assert(status.kicks == 0U);

    assert(sapi_watchdog_start(wd) == SAPI_STATUS_OK);
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.active == 1U);
    assert(status.time_until_fire == 1000U);

    /* Advance halfway, kick: deadline should reset from the new "now". */
    g_mock_now_ms += 600U;
    assert(sapi_watchdog_kick(wd) == SAPI_STATUS_OK);
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.kicks == 1U);
    assert(status.time_until_fire == 1000U); /* full timeout again from kick time */

    assert(sapi_watchdog_stop(wd) == SAPI_STATUS_OK);
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.active == 0U);

    assert(sapi_watchdog_destroy(wd) == SAPI_STATUS_OK);
    /* Handle is invalid after destroy. */
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_INVALID_PARAM);
}

static void test_fires_when_not_kicked_log_action(void)
{
    sapi_watchdog_t wd = NULL;
    sapi_watchdog_config_t config = {0};
    sapi_watchdog_status_t status = {0};

    config.type = SAPI_WATCHDOG_TASK;
    config.name = "log_fire_wd";
    config.timeout_ms = 200U;
    config.action = SAPI_WATCHDOG_ACTION_LOG;

    g_mock_now_ms = 10000U;
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_OK);
    assert(sapi_watchdog_start(wd) == SAPI_STATUS_OK);

    /* Not yet due: a tick before the deadline must not fire it. */
    g_mock_now_ms += 100U;
    sapi_watchdog_timer_tick();
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.fires == 0U);

    /* Advance past the deadline without kicking: tick must fire it. */
    g_mock_now_ms += 200U; /* now 300ms after start, timeout was 200ms */
    sapi_watchdog_timer_tick();
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.fires == 1U);

    /* Already-fired watchdog rejects further kicks until restarted. */
    assert(sapi_watchdog_kick(wd) == SAPI_STATUS_INTERNAL_ERROR);

    /* A second tick must not fire it again (fired latch). */
    sapi_watchdog_timer_tick();
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.fires == 1U);

    assert(sapi_watchdog_destroy(wd) == SAPI_STATUS_OK);
}

static void test_kicking_prevents_fire(void)
{
    sapi_watchdog_t wd = NULL;
    sapi_watchdog_config_t config = {0};
    sapi_watchdog_status_t status = {0};

    config.type = SAPI_WATCHDOG_TASK;
    config.name = "kept_alive_wd";
    config.timeout_ms = 100U;
    config.action = SAPI_WATCHDOG_ACTION_LOG;

    g_mock_now_ms = 20000U;
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_OK);
    assert(sapi_watchdog_start(wd) == SAPI_STATUS_OK);

    /* Simulate a healthy loop: advance less than timeout_ms each step,
     * kicking every time - must never fire. */
    for (int i = 0; i < 10; i++)
    {
        g_mock_now_ms += 50U;
        sapi_watchdog_timer_tick();
        assert(sapi_watchdog_kick(wd) == SAPI_STATUS_OK);
    }
    assert(sapi_watchdog_get_status(wd, &status) == SAPI_STATUS_OK);
    assert(status.fires == 0U);
    assert(status.kicks == 10U);

    assert(sapi_watchdog_destroy(wd) == SAPI_STATUS_OK);
}

static void test_safestate_action_dispatches_on_fire(void)
{
    sapi_watchdog_t wd = NULL;
    sapi_watchdog_config_t config = {0};

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler) == SAPI_STATUS_OK);

    config.type = SAPI_WATCHDOG_SYSTEM;
    config.name = "safestate_wd";
    config.timeout_ms = 50U;
    config.action = SAPI_WATCHDOG_ACTION_SAFESTATE;

    g_mock_now_ms = 30000U;
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_OK);
    assert(sapi_watchdog_start(wd) == SAPI_STATUS_OK);

    g_mock_now_ms += 100U; /* past the 50ms deadline */
    g_diverting_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        sapi_watchdog_timer_tick();
        assert(0); /* must not reach here: SAFESTATE diverts away */
    }
    else
    {
        assert(g_diverting_calls == 1);
        assert(g_captured_level == SAPI_SAFESTATE_LEVEL_SAFE);
        assert(g_captured_reason == SAPI_SAFESTATE_REASON_UNSPECIFIED);
    }

    assert(sapi_watchdog_destroy(wd) == SAPI_STATUS_OK);
}

static void test_custom_action_dispatches_on_fire(void)
{
    sapi_watchdog_t wd = NULL;
    sapi_watchdog_config_t config = {0};
    int ctx_value = 42;

    config.type = SAPI_WATCHDOG_CHANNEL;
    config.name = "custom_wd";
    config.timeout_ms = 50U;
    config.action = SAPI_WATCHDOG_ACTION_CUSTOM;
    config.custom_action = custom_action;
    config.context = &ctx_value;

    g_mock_now_ms = 40000U;
    g_custom_calls = 0;
    g_custom_ctx_seen = NULL;
    assert(sapi_watchdog_create(&wd, &config) == SAPI_STATUS_OK);
    assert(sapi_watchdog_start(wd) == SAPI_STATUS_OK);

    g_mock_now_ms += 60U;
    sapi_watchdog_timer_tick();

    assert(g_custom_calls == 1);
    assert(g_custom_ctx_seen == &ctx_value);

    assert(sapi_watchdog_destroy(wd) == SAPI_STATUS_OK);
}

static void test_pool_exhaustion(void)
{
    /* Pool size (SAPI_WATCHDOG_MAX_COUNT) is an internal implementation
     * detail, not exposed via the public header; probe it empirically by
     * creating watchdogs until RESOURCE_EXHAUSTED is returned, then
     * confirm that failure is reached in a small bounded number of
     * attempts (proving there IS a fixed bound, per REQ - no dynamic
     * growth), and clean every successfully created one back up. */
    sapi_watchdog_t handles[64];
    sapi_watchdog_config_t config = {0};
    int created = 0;
    int i;

    config.type = SAPI_WATCHDOG_TASK;
    config.name = "pool_wd";
    config.timeout_ms = 100U;
    config.action = SAPI_WATCHDOG_ACTION_LOG;

    for (i = 0; i < 64; i++)
    {
        sapi_status_t rc = sapi_watchdog_create(&handles[i], &config);
        if (rc == SAPI_STATUS_RESOURCE_EXHAUSTED)
        {
            break;
        }
        assert(rc == SAPI_STATUS_OK);
        created++;
    }
    assert(created > 0);
    assert(created < 64); /* proves a real fixed bound exists */

    for (i = 0; i < created; i++)
    {
        assert(sapi_watchdog_destroy(handles[i]) == SAPI_STATUS_OK);
    }
}

int main(void)
{
    test_create_requires_manager_initialized();

    assert(sapi_timer_register_backend(&g_mock_timer_backend) == SAPI_STATUS_OK);
    assert(sapi_watchdog_manager_initialize() == SAPI_STATUS_OK);

    test_create_param_validation();
    test_lifecycle_and_kick();
    test_fires_when_not_kicked_log_action();
    test_kicking_prevents_fire();
    test_safestate_action_dispatches_on_fire();
    test_custom_action_dispatches_on_fire();
    test_pool_exhaustion();

    assert(sapi_watchdog_manager_shutdown() == SAPI_STATUS_OK);
    return 0;
}
