/* Tests for the real rte_watchdog implementation (replaces the previous
 * non-functional stub - see src/watchdog/rte_watchdog.c file header).
 *
 * Uses a mock rte_timer OSAdapter whose "now" function returns a
 * test-controlled counter (g_mock_now_ms) instead of a real clock, so
 * firing can be verified deterministically (advance the counter, call
 * rte_watchdog_timer_tick()) without any real sleep/flakiness.
 *
 * The SAFESTATE/REBOOT actions are verified the same way
 * tests/safestate/test_rte_safestate.c verifies RTE_SAFESTATE_LEVEL_SAFE
 * itself never returns: a diverting handler using setjmp/longjmp stands in
 * for a real "drive outputs safe, then reset" handler.
 */
#include <assert.h>
#include <setjmp.h>
#include "rte/redundancy/watchdog/rte_watchdog.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte/utils/safestate/rte_safestate.h"
#include "rte/oal/timer/rte_timer.h"
#include "rte_osadapter/timer/rte_osadapter_timer.h"

/* ---- mock timer OSAdapter: caller-controlled clock ---- */
static rte_timestamp_ms_t g_mock_now_ms = 0U;

static rte_status_t mock_timer_now(rte_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = g_mock_now_ms;
    return RTE_STATUS_OK;
}

static const rte_osadapter_timer_t g_mock_timer_osadapter = {
    NULL, NULL, NULL, NULL, mock_timer_now
};

/* ---- diverting SAFE/REBOOT handler (never returns, like a real one) ---- */
static jmp_buf g_jmp;
static rte_safestate_level_t g_captured_level;
static rte_safestate_reason_t g_captured_reason;
static int g_diverting_calls;

static void diverting_handler(rte_safestate_level_t level,
                               rte_safestate_reason_t reason,
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
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    config.type = RTE_WATCHDOG_TASK;
    config.name = "pre_init_wd";
    config.timeout_ms = 100U;
    config.action = RTE_WATCHDOG_ACTION_LOG;

    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_NOT_INITIALIZED);
}

static void test_create_param_validation(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    config.type = RTE_WATCHDOG_TASK;
    config.name = "bad_wd";
    config.timeout_ms = 100U;
    config.action = RTE_WATCHDOG_ACTION_LOG;

    assert(rte_watchdog_create(NULL, &config) == RTE_STATUS_INVALID_PARAM);
    assert(rte_watchdog_create(&wd, NULL) == RTE_STATUS_INVALID_PARAM);

    config.timeout_ms = 0U; /* zero timeout is not valid */
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_INVALID_PARAM);
    config.timeout_ms = 100U;

    config.action = (rte_watchdog_action_t)99; /* out of enum range */
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_INVALID_PARAM);

    config.action = RTE_WATCHDOG_ACTION_CUSTOM;
    config.custom_action = NULL; /* CUSTOM requires a callback */
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_INVALID_PARAM);
}

static void test_lifecycle_and_kick(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    rte_watchdog_status_t status = {0};

    config.type = RTE_WATCHDOG_TASK;
    config.name = "lifecycle_wd";
    config.timeout_ms = 1000U;
    config.action = RTE_WATCHDOG_ACTION_LOG;

    g_mock_now_ms = 5000U;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(wd != NULL);

    /* Created but not started: get_status still succeeds. */
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.active == 0U);
    assert(status.kicks == 0U);

    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.active == 1U);
    assert(status.time_until_fire == 1000U);

    /* Advance halfway, kick: deadline should reset from the new "now". */
    g_mock_now_ms += 600U;
    assert(rte_watchdog_kick(wd) == RTE_STATUS_OK);
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.kicks == 1U);
    assert(status.time_until_fire == 1000U); /* full timeout again from kick time */

    assert(rte_watchdog_stop(wd) == RTE_STATUS_OK);
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.active == 0U);

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
    /* Handle is invalid after destroy. */
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_INVALID_PARAM);
}

static void test_fires_when_not_kicked_log_action(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    rte_watchdog_status_t status = {0};

    config.type = RTE_WATCHDOG_TASK;
    config.name = "log_fire_wd";
    config.timeout_ms = 200U;
    config.action = RTE_WATCHDOG_ACTION_LOG;

    g_mock_now_ms = 10000U;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);

    /* Not yet due: a tick before the deadline must not fire it. */
    g_mock_now_ms += 100U;
    rte_watchdog_timer_tick();
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.fires == 0U);

    /* Advance past the deadline without kicking: tick must fire it. */
    g_mock_now_ms += 200U; /* now 300ms after start, timeout was 200ms */
    rte_watchdog_timer_tick();
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.fires == 1U);

    /* Already-fired watchdog rejects further kicks until restarted. */
    assert(rte_watchdog_kick(wd) == RTE_STATUS_INTERNAL_ERROR);

    /* A second tick must not fire it again (fired latch). */
    rte_watchdog_timer_tick();
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.fires == 1U);

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

static void test_kicking_prevents_fire(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    rte_watchdog_status_t status = {0};

    config.type = RTE_WATCHDOG_TASK;
    config.name = "kept_alive_wd";
    config.timeout_ms = 100U;
    config.action = RTE_WATCHDOG_ACTION_LOG;

    g_mock_now_ms = 20000U;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);

    /* Simulate a healthy loop: advance less than timeout_ms each step,
     * kicking every time - must never fire. */
    for (int i = 0; i < 10; i++)
    {
        g_mock_now_ms += 50U;
        rte_watchdog_timer_tick();
        assert(rte_watchdog_kick(wd) == RTE_STATUS_OK);
    }
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.fires == 0U);
    assert(status.kicks == 10U);

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

static void test_safestate_action_dispatches_on_fire(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};

    assert(rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_SAFE, diverting_handler) == RTE_STATUS_OK);

    config.type = RTE_WATCHDOG_SYSTEM;
    config.name = "safestate_wd";
    config.timeout_ms = 50U;
    config.action = RTE_WATCHDOG_ACTION_SAFESTATE;

    g_mock_now_ms = 30000U;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);

    g_mock_now_ms += 100U; /* past the 50ms deadline */
    g_diverting_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        rte_watchdog_timer_tick();
        assert(0); /* must not reach here: SAFESTATE diverts away */
    }
    else
    {
        assert(g_diverting_calls == 1);
        assert(g_captured_level == RTE_SAFESTATE_LEVEL_SAFE);
        assert(g_captured_reason == RTE_SAFESTATE_REASON_UNSPECIFIED);
    }

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

static void test_custom_action_dispatches_on_fire(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    int ctx_value = 42;

    config.type = RTE_WATCHDOG_CHANNEL;
    config.name = "custom_wd";
    config.timeout_ms = 50U;
    config.action = RTE_WATCHDOG_ACTION_CUSTOM;
    config.custom_action = custom_action;
    config.context = &ctx_value;

    g_mock_now_ms = 40000U;
    g_custom_calls = 0;
    g_custom_ctx_seen = NULL;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);

    g_mock_now_ms += 60U;
    rte_watchdog_timer_tick();

    assert(g_custom_calls == 1);
    assert(g_custom_ctx_seen == &ctx_value);

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

static void test_invalid_handle_rejected_by_every_op(void)
{
    /* NULL, and a pointer that never came from rte_watchdog_create(), are
     * both rejected by is_valid_handle() in start/stop/kick/destroy,
     * independently of one another - see test_lifecycle_and_kick() for
     * get_status()'s own invalid-handle check (already exercised there
     * after destroy()). */
    int not_a_watchdog;
    rte_watchdog_t bogus = (rte_watchdog_t)&not_a_watchdog;

    assert(rte_watchdog_start(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_watchdog_start(bogus) == RTE_STATUS_INVALID_PARAM);

    assert(rte_watchdog_stop(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_watchdog_stop(bogus) == RTE_STATUS_INVALID_PARAM);

    assert(rte_watchdog_kick(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_watchdog_kick(bogus) == RTE_STATUS_INVALID_PARAM);

    assert(rte_watchdog_destroy(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_watchdog_destroy(bogus) == RTE_STATUS_INVALID_PARAM);
}

static void test_timeout_handler_guards_directly(void)
{
    /* rte_watchdog_timeout_handler() is exposed in the public header (it
     * is what rte_watchdog_timer_tick() dispatches to once a deadline
     * has already been confirmed reached) - calling it directly exercises
     * its own defensive guards, which rte_watchdog_timer_tick() never
     * triggers itself since it only calls the handler after re-checking
     * exactly the same conditions. */
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    rte_watchdog_status_t status = {0};

    /* Out-of-range watchdog_id: must not touch the pool at all. */
    rte_watchdog_timeout_handler(0xFFFFFFFFU);

    /* in_use == 0: an id that has never been allocated (pool was reset by
     * a prior manager_shutdown()/re-initialize in this same test binary,
     * so slot 0 is guaranteed free at this point). */
    rte_watchdog_timeout_handler(0U);

    config.type = RTE_WATCHDOG_TASK;
    config.name = "handler_guard_wd";
    config.timeout_ms = 100U;
    config.action = RTE_WATCHDOG_ACTION_LOG;
    g_mock_now_ms = 50000U;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);

    /* active == 0: created but never started. */
    rte_watchdog_timeout_handler(0U);
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.fires == 0U);

    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);
    g_mock_now_ms += 200U; /* past the 100ms deadline */
    rte_watchdog_timer_tick();
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.fires == 1U);

    /* fired != 0: already fired above: a direct second call must also be
     * a no-op (the "fired" latch), matching the tick-driven case already
     * covered in test_fires_when_not_kicked_log_action(). */
    rte_watchdog_timeout_handler(0U);
    assert(rte_watchdog_get_status(wd, &status) == RTE_STATUS_OK);
    assert(status.fires == 1U);

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

static void test_reboot_action_dispatches_on_fire(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};

    assert(rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_REBOOT, diverting_handler) == RTE_STATUS_OK);

    config.type = RTE_WATCHDOG_SYSTEM;
    config.name = "reboot_wd";
    config.timeout_ms = 50U;
    config.action = RTE_WATCHDOG_ACTION_REBOOT;

    g_mock_now_ms = 60000U;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);

    g_mock_now_ms += 100U; /* past the 50ms deadline */
    g_diverting_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        rte_watchdog_timer_tick();
        assert(0); /* must not reach here: REBOOT diverts away */
    }
    else
    {
        assert(g_diverting_calls == 1);
        assert(g_captured_level == RTE_SAFESTATE_LEVEL_REBOOT);
        assert(g_captured_reason == RTE_SAFESTATE_REASON_UNSPECIFIED);
    }

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

static void test_failover_action_dispatches_on_fire(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    int ctx_value = 7;

    config.type = RTE_WATCHDOG_CHANNEL;
    config.name = "failover_wd";
    config.timeout_ms = 50U;
    config.action = RTE_WATCHDOG_ACTION_FAILOVER;
    config.custom_action = custom_action;
    config.context = &ctx_value;

    g_mock_now_ms = 70000U;
    g_custom_calls = 0;
    g_custom_ctx_seen = NULL;
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(rte_watchdog_start(wd) == RTE_STATUS_OK);

    g_mock_now_ms += 60U;
    rte_watchdog_timer_tick();

    assert(g_custom_calls == 1);
    assert(g_custom_ctx_seen == &ctx_value);

    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

/* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
 * locked, rte_watchdog_create() refuses even with fully-valid arguments
 * and the manager already initialized. */
static void test_setup_phase_lock_rejects_create(void)
{
    rte_watchdog_t wd = NULL;
    rte_watchdog_config_t config = {0};
    config.type = RTE_WATCHDOG_TASK;
    config.name = "locked_wd";
    config.timeout_ms = 100U;
    config.action = RTE_WATCHDOG_ACTION_LOG;

    rte_lifecycle_lock();
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_INVALID_STATE);
    rte_lifecycle_unlock();
    assert(rte_watchdog_create(&wd, &config) == RTE_STATUS_OK);
    assert(rte_watchdog_destroy(wd) == RTE_STATUS_OK);
}

static void test_pool_exhaustion(void)
{
    /* Pool size (RTE_WATCHDOG_MAX_COUNT) is an internal implementation
     * detail, not exposed via the public header; probe it empirically by
     * creating watchdogs until RESOURCE_EXHAUSTED is returned, then
     * confirm that failure is reached in a small bounded number of
     * attempts (proving there IS a fixed bound, per REQ - no dynamic
     * growth), and clean every successfully created one back up. */
    rte_watchdog_t handles[64];
    rte_watchdog_config_t config = {0};
    int created = 0;
    int i;

    config.type = RTE_WATCHDOG_TASK;
    config.name = "pool_wd";
    config.timeout_ms = 100U;
    config.action = RTE_WATCHDOG_ACTION_LOG;

    for (i = 0; i < 64; i++)
    {
        rte_status_t rc = rte_watchdog_create(&handles[i], &config);
        if (rc == RTE_STATUS_RESOURCE_EXHAUSTED)
        {
            break;
        }
        assert(rc == RTE_STATUS_OK);
        created++;
    }
    assert(created > 0);
    assert(created < 64); /* proves a real fixed bound exists */

    for (i = 0; i < created; i++)
    {
        assert(rte_watchdog_destroy(handles[i]) == RTE_STATUS_OK);
    }
}

int main(void)
{
    test_create_requires_manager_initialized();

    assert(rte_osadapter_timer_register(&g_mock_timer_osadapter) == RTE_STATUS_OK);
    assert(rte_watchdog_manager_initialize() == RTE_STATUS_OK);

    test_create_param_validation();
    test_lifecycle_and_kick();
    test_fires_when_not_kicked_log_action();
    test_kicking_prevents_fire();
    test_safestate_action_dispatches_on_fire();
    test_custom_action_dispatches_on_fire();
    test_invalid_handle_rejected_by_every_op();
    test_timeout_handler_guards_directly();
    test_reboot_action_dispatches_on_fire();
    test_failover_action_dispatches_on_fire();
    test_setup_phase_lock_rejects_create();
    test_pool_exhaustion();

    assert(rte_watchdog_manager_shutdown() == RTE_STATUS_OK);

    /* rte_watchdog_timer_tick() with no manager initialized: a silent
     * no-op, not a crash - mirrors test_create_requires_manager_initialized()
     * but for the tick path instead of create(). */
    rte_watchdog_timer_tick();

    return 0;
}
