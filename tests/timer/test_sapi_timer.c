/* Tests for the sapi_timer validate-then-dispatch API (ADR-005): parameter
 * validation happens regardless of backend state; SAPI_STATUS_NOT_INITIALIZED
 * is returned before any backend is registered; a registered mock backend
 * is reached with the framework-validated arguments; a NULL vtable slot on
 * an otherwise-registered backend yields SAPI_STATUS_NOT_SUPPORTED. */
#include <assert.h>
#include "safeapi/lifecycle/sapi_lifecycle.h"
#include "safeapi/timer/sapi_timer.h"
#include "safeapi_backend/timer/sapi_timer_backend.h"

static void dummy_callback(sapi_timer_handle_t handle, void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
}

static int g_mock_create_calls = 0;
static int g_mock_start_calls = 0;
static int g_mock_stop_calls = 0;
static int g_mock_destroy_calls = 0;
static int g_mock_now_calls = 0;

static sapi_status_t mock_create(sapi_timer_storage_t *storage,
                                  const sapi_timer_config_t *config,
                                  sapi_timer_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    g_mock_create_calls++;
    *out_handle = (sapi_timer_handle_t)(void *)1; /* arbitrary non-NULL sentinel */
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_start(sapi_timer_handle_t handle)
{
    (void)handle;
    g_mock_start_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_stop(sapi_timer_handle_t handle)
{
    (void)handle;
    g_mock_stop_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_destroy(sapi_timer_handle_t handle)
{
    (void)handle;
    g_mock_destroy_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_now(sapi_timestamp_ms_t *out_now_ms)
{
    g_mock_now_calls++;
    *out_now_ms = 42U;
    return SAPI_STATUS_OK;
}

static const sapi_timer_backend_t g_mock_backend_full __attribute__((unused)) = {
    mock_create, mock_start, mock_stop, mock_destroy, mock_now
};

static const sapi_timer_backend_t g_mock_backend_no_create __attribute__((unused)) = {
    NULL, NULL, NULL, NULL, NULL
};

static const sapi_timer_backend_t g_mock_backend_no_start __attribute__((unused)) = {
    mock_create, NULL, mock_stop, mock_destroy, mock_now
};

static const sapi_timer_backend_t g_mock_backend_no_stop __attribute__((unused)) = {
    mock_create, mock_start, NULL, mock_destroy, mock_now
};

static const sapi_timer_backend_t g_mock_backend_no_destroy __attribute__((unused)) = {
    mock_create, mock_start, mock_stop, NULL, mock_now
};

static const sapi_timer_backend_t g_mock_backend_no_now __attribute__((unused)) = {
    mock_create, mock_start, mock_stop, mock_destroy, NULL
};

int main(void)
{
    sapi_timer_storage_t storage __attribute__((unused));
    sapi_timer_handle_t handle __attribute__((unused)) = NULL;

    /* Null-parameter rejection happens before any backend is consulted. */
    assert(sapi_timer_create(NULL, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* Invalid config (missing callback) rejected even with valid pointers. */
    sapi_timer_config_t bad_config __attribute__((unused)) = {0};
    bad_config.mode = SAPI_TIMER_MODE_ONE_SHOT;
    bad_config.period_ms = 100U;
    bad_config.callback = NULL;
    assert(sapi_timer_create(&storage, &bad_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    /* Invalid config (period_ms == 0) rejected even with a valid callback. */
    sapi_timer_config_t zero_period_config __attribute__((unused)) = {0};
    zero_period_config.mode = SAPI_TIMER_MODE_ONE_SHOT;
    zero_period_config.period_ms = 0U;
    zero_period_config.callback = dummy_callback;
    assert(sapi_timer_create(&storage, &zero_period_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    sapi_timer_config_t config __attribute__((unused)) = {0};
    config.mode = SAPI_TIMER_MODE_PERIODIC;
    config.period_ms = 100U;
    config.callback = dummy_callback;
    config.user_ctx = NULL;

    /* No backend registered yet: valid params, but nothing to dispatch to. */
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_NOT_INITIALIZED);

    /* Null-parameter rejection for start/stop/destroy/now happens before
     * any backend is consulted, independently of one another. */
    assert(sapi_timer_start(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_timer_stop(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_timer_destroy(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_timer_now(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* No backend registered yet: valid params, but nothing to dispatch to,
     * for every remaining entry point. */
    sapi_timer_handle_t dummy_handle = (sapi_timer_handle_t)(void *)1;
    sapi_timestamp_ms_t now_ms;
    assert(sapi_timer_start(dummy_handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_timer_stop(dummy_handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_timer_destroy(dummy_handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_timer_now(&now_ms) == SAPI_STATUS_NOT_INITIALIZED);

    /* Registering NULL is rejected. */
    assert(sapi_timer_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* A backend with a NULL create slot yields NOT_SUPPORTED. */
    assert(sapi_timer_register_backend(&g_mock_backend_no_create) == SAPI_STATUS_OK);
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_NOT_SUPPORTED);

    /* A backend with a NULL start/stop/destroy/now slot yields
     * NOT_SUPPORTED for that entry point specifically, independently of
     * the others. */
    assert(sapi_timer_register_backend(&g_mock_backend_no_start) == SAPI_STATUS_OK);
    assert(sapi_timer_start(dummy_handle) == SAPI_STATUS_NOT_SUPPORTED);

    assert(sapi_timer_register_backend(&g_mock_backend_no_stop) == SAPI_STATUS_OK);
    assert(sapi_timer_stop(dummy_handle) == SAPI_STATUS_NOT_SUPPORTED);

    assert(sapi_timer_register_backend(&g_mock_backend_no_destroy) == SAPI_STATUS_OK);
    assert(sapi_timer_destroy(dummy_handle) == SAPI_STATUS_NOT_SUPPORTED);

    assert(sapi_timer_register_backend(&g_mock_backend_no_now) == SAPI_STATUS_OK);
    assert(sapi_timer_now(&now_ms) == SAPI_STATUS_NOT_SUPPORTED);

    /* A fully-populated backend is actually reached, with the same
     * already-validated arguments the caller passed in. */
    assert(sapi_timer_register_backend(&g_mock_backend_full) == SAPI_STATUS_OK);
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(g_mock_create_calls == 1);
    assert(handle != NULL);

    assert(sapi_timer_start(handle) == SAPI_STATUS_OK);
    assert(g_mock_start_calls == 1);

    assert(sapi_timer_stop(handle) == SAPI_STATUS_OK);
    assert(g_mock_stop_calls == 1);

    assert(sapi_timer_destroy(handle) == SAPI_STATUS_OK);
    assert(g_mock_destroy_calls == 1);

    assert(sapi_timer_now(&now_ms) == SAPI_STATUS_OK);
    assert(g_mock_now_calls == 1);
    assert(now_ms == 42U);

    /* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
     * locked, sapi_timer_create() refuses even with a fully-valid config
     * and a working backend. Only create() is gated (a setup-only call);
     * start/stop/destroy/now are runtime operations on an
     * already-created timer and stay unaffected. */
    sapi_lifecycle_lock();
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_INVALID_STATE);
    sapi_lifecycle_unlock();
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_OK);

    return 0;
}
