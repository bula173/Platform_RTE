/* Tests for sapi_appmanager (previously untested): lifecycle ordering,
 * max_iterations/error_threshold bounds, sapi_appmanager_request_shutdown(),
 * and the POSIX-only sapi_appmanager_install_default_signal_handlers()
 * convenience (see that function's own doc in sapi_appmanager.h for why
 * it's a deliberate, scoped exception to this framework's usual
 * OS-agnosticism).
 */
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include "safeapi/appmanager/sapi_appmanager.h"

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
        NULL, fake_execute_ok, fake_shutdown, fake_get_name, fake_get_version
    };

    assert(sapi_appmanager_run(NULL) == EXIT_FAILURE);

    config.ops = NULL;
    config.context = NULL;
    config.max_iterations = 1U;
    config.error_threshold = 0U;
    assert(sapi_appmanager_run(&config) == EXIT_FAILURE);

    config.ops = &incomplete_ops;
    assert(sapi_appmanager_run(&config) == EXIT_FAILURE);
}

static void test_init_failure_still_calls_shutdown(void)
{
    static const sapi_appmanager_operations_t ops = {
        fake_init_fail, fake_execute_ok, fake_shutdown, fake_get_name, fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 5U;
    config.error_threshold = 0U;

    assert(sapi_appmanager_run(&config) == EXIT_FAILURE);
    assert(g_init_calls == 1);
    assert(g_execute_calls == 0); /* init failed - execute must never run */
    assert(g_shutdown_calls == 1); /* shutdown always runs, even on init failure */
}

static void test_bounded_run_calls_in_order_with_correct_counts(void)
{
    static const sapi_appmanager_operations_t ops = {
        fake_init_ok, fake_execute_ok, fake_shutdown, fake_get_name, fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 4U;
    config.error_threshold = 0U;

    assert(sapi_appmanager_run(&config) == EXIT_SUCCESS);
    assert(g_init_calls == 1);
    assert(g_execute_calls == 4); /* stopped exactly at max_iterations */
    assert(g_shutdown_calls == 1);
    assert(g_last_seen_state_in_execute == (int)SAPI_APP_STATE_RUNNING);
    assert(sapi_appmanager_get_state() == SAPI_APP_STATE_SHUTDOWN);
}

static void test_error_threshold_stops_run_early(void)
{
    static const sapi_appmanager_operations_t ops = {
        fake_init_ok, fake_execute_always_fails, fake_shutdown, fake_get_name, fake_get_version
    };
    sapi_appmanager_config_t config;
    sapi_appmanager_state_t stats;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 100U; /* would never reach this - threshold hits first */
    config.error_threshold = 3U;

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
        fake_init_ok, fake_execute_requests_shutdown_after_3, fake_shutdown, fake_get_name, fake_get_version
    };
    sapi_appmanager_config_t config;

    reset_counters();
    config.ops = &ops;
    config.context = NULL;
    config.max_iterations = 0U; /* infinite - only request_shutdown() can end this */
    config.error_threshold = 0U;

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
        fake_init_ok, fake_execute_raises_sigint_once, fake_shutdown, fake_get_name, fake_get_version
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

int main(void)
{
    test_config_validation();
    test_init_failure_still_calls_shutdown();
    test_bounded_run_calls_in_order_with_correct_counts();
    test_error_threshold_stops_run_early();
    test_request_shutdown_stops_infinite_run();
    test_install_default_signal_handlers();
    return 0;
}
