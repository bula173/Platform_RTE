/* Tests for the rte_timer validate-then-dispatch API (ADR-005): parameter
 * validation happens regardless of backend state; RTE_STATUS_NOT_INITIALIZED
 * is returned before any backend is registered; a registered mock backend
 * is reached with the framework-validated arguments; a NULL vtable slot on
 * an otherwise-registered backend yields RTE_STATUS_NOT_SUPPORTED. */
#include <assert.h>
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi/oal/timer/rte_timer.h"
#include "safeapi_backend/timer/rte_timer_backend.h"

static void dummy_callback(rte_timer_handle_t handle, void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
}

static int g_mock_create_calls = 0;
static int g_mock_start_calls = 0;
static int g_mock_stop_calls = 0;
static int g_mock_destroy_calls = 0;
static int g_mock_now_calls = 0;

static rte_status_t mock_create(rte_timer_storage_t *storage,
                                  const rte_timer_config_t *config,
                                  rte_timer_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    g_mock_create_calls++;
    *out_handle = (rte_timer_handle_t)(void *)1; /* arbitrary non-NULL sentinel */
    return RTE_STATUS_OK;
}

static rte_status_t mock_start(rte_timer_handle_t handle)
{
    (void)handle;
    g_mock_start_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_stop(rte_timer_handle_t handle)
{
    (void)handle;
    g_mock_stop_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_destroy(rte_timer_handle_t handle)
{
    (void)handle;
    g_mock_destroy_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_now(rte_timestamp_ms_t *out_now_ms)
{
    g_mock_now_calls++;
    *out_now_ms = 42U;
    return RTE_STATUS_OK;
}

static const rte_timer_backend_t g_mock_backend_full __attribute__((unused)) = {
    mock_create, mock_start, mock_stop, mock_destroy, mock_now
};

static const rte_timer_backend_t g_mock_backend_no_create __attribute__((unused)) = {
    NULL, NULL, NULL, NULL, NULL
};

static const rte_timer_backend_t g_mock_backend_no_start __attribute__((unused)) = {
    mock_create, NULL, mock_stop, mock_destroy, mock_now
};

static const rte_timer_backend_t g_mock_backend_no_stop __attribute__((unused)) = {
    mock_create, mock_start, NULL, mock_destroy, mock_now
};

static const rte_timer_backend_t g_mock_backend_no_destroy __attribute__((unused)) = {
    mock_create, mock_start, mock_stop, NULL, mock_now
};

static const rte_timer_backend_t g_mock_backend_no_now __attribute__((unused)) = {
    mock_create, mock_start, mock_stop, mock_destroy, NULL
};

int main(void)
{
    rte_timer_storage_t storage __attribute__((unused));
    rte_timer_handle_t handle __attribute__((unused)) = NULL;

    /* Null-parameter rejection happens before any backend is consulted. */
    assert(rte_timer_create(NULL, NULL, NULL) == RTE_STATUS_INVALID_PARAM);

    /* Invalid config (missing callback) rejected even with valid pointers. */
    rte_timer_config_t bad_config __attribute__((unused)) = {0};
    bad_config.mode = RTE_TIMER_MODE_ONE_SHOT;
    bad_config.period_ms = 100U;
    bad_config.callback = NULL;
    assert(rte_timer_create(&storage, &bad_config, &handle) == RTE_STATUS_INVALID_PARAM);

    /* Invalid config (period_ms == 0) rejected even with a valid callback. */
    rte_timer_config_t zero_period_config __attribute__((unused)) = {0};
    zero_period_config.mode = RTE_TIMER_MODE_ONE_SHOT;
    zero_period_config.period_ms = 0U;
    zero_period_config.callback = dummy_callback;
    assert(rte_timer_create(&storage, &zero_period_config, &handle) == RTE_STATUS_INVALID_PARAM);

    rte_timer_config_t config __attribute__((unused)) = {0};
    config.mode = RTE_TIMER_MODE_PERIODIC;
    config.period_ms = 100U;
    config.callback = dummy_callback;
    config.user_ctx = NULL;

    /* No backend registered yet: valid params, but nothing to dispatch to. */
    assert(rte_timer_create(&storage, &config, &handle) == RTE_STATUS_NOT_INITIALIZED);

    /* Null-parameter rejection for start/stop/destroy/now happens before
     * any backend is consulted, independently of one another. */
    assert(rte_timer_start(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_timer_stop(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_timer_destroy(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_timer_now(NULL) == RTE_STATUS_INVALID_PARAM);

    /* No backend registered yet: valid params, but nothing to dispatch to,
     * for every remaining entry point. */
    rte_timer_handle_t dummy_handle = (rte_timer_handle_t)(void *)1;
    rte_timestamp_ms_t now_ms;
    assert(rte_timer_start(dummy_handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_timer_stop(dummy_handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_timer_destroy(dummy_handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_timer_now(&now_ms) == RTE_STATUS_NOT_INITIALIZED);

    /* Registering NULL is rejected. */
    assert(rte_timer_register_backend(NULL) == RTE_STATUS_INVALID_PARAM);

    /* A backend with a NULL create slot yields NOT_SUPPORTED. */
    assert(rte_timer_register_backend(&g_mock_backend_no_create) == RTE_STATUS_OK);
    assert(rte_timer_create(&storage, &config, &handle) == RTE_STATUS_NOT_SUPPORTED);

    /* A backend with a NULL start/stop/destroy/now slot yields
     * NOT_SUPPORTED for that entry point specifically, independently of
     * the others. */
    assert(rte_timer_register_backend(&g_mock_backend_no_start) == RTE_STATUS_OK);
    assert(rte_timer_start(dummy_handle) == RTE_STATUS_NOT_SUPPORTED);

    assert(rte_timer_register_backend(&g_mock_backend_no_stop) == RTE_STATUS_OK);
    assert(rte_timer_stop(dummy_handle) == RTE_STATUS_NOT_SUPPORTED);

    assert(rte_timer_register_backend(&g_mock_backend_no_destroy) == RTE_STATUS_OK);
    assert(rte_timer_destroy(dummy_handle) == RTE_STATUS_NOT_SUPPORTED);

    assert(rte_timer_register_backend(&g_mock_backend_no_now) == RTE_STATUS_OK);
    assert(rte_timer_now(&now_ms) == RTE_STATUS_NOT_SUPPORTED);

    /* A fully-populated backend is actually reached, with the same
     * already-validated arguments the caller passed in. */
    assert(rte_timer_register_backend(&g_mock_backend_full) == RTE_STATUS_OK);
    assert(rte_timer_create(&storage, &config, &handle) == RTE_STATUS_OK);
    assert(g_mock_create_calls == 1);
    assert(handle != NULL);

    assert(rte_timer_start(handle) == RTE_STATUS_OK);
    assert(g_mock_start_calls == 1);

    assert(rte_timer_stop(handle) == RTE_STATUS_OK);
    assert(g_mock_stop_calls == 1);

    assert(rte_timer_destroy(handle) == RTE_STATUS_OK);
    assert(g_mock_destroy_calls == 1);

    assert(rte_timer_now(&now_ms) == RTE_STATUS_OK);
    assert(g_mock_now_calls == 1);
    assert(now_ms == 42U);

    /* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
     * locked, rte_timer_create() refuses even with a fully-valid config
     * and a working backend. Only create() is gated (a setup-only call);
     * start/stop/destroy/now are runtime operations on an
     * already-created timer and stay unaffected. */
    rte_lifecycle_lock();
    assert(rte_timer_create(&storage, &config, &handle) == RTE_STATUS_INVALID_STATE);
    rte_lifecycle_unlock();
    assert(rte_timer_create(&storage, &config, &handle) == RTE_STATUS_OK);

    return 0;
}
