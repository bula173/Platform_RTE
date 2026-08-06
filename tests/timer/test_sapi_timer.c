/* Tests for the sapi_timer validate-then-dispatch API (ADR-005): parameter
 * validation happens regardless of backend state; SAPI_STATUS_NOT_INITIALIZED
 * is returned before any backend is registered; a registered mock backend
 * is reached with the framework-validated arguments; a NULL vtable slot on
 * an otherwise-registered backend yields SAPI_STATUS_NOT_SUPPORTED. */
#include <assert.h>
#include "safeapi/timer/sapi_timer.h"
#include "safeapi_backend/timer/sapi_timer_backend.h"

static void dummy_callback(sapi_timer_handle_t handle, void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
}

static int g_mock_create_calls = 0;

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

static const sapi_timer_backend_t g_mock_backend_full __attribute__((unused)) = {
    mock_create, NULL, NULL, NULL, NULL
};

static const sapi_timer_backend_t g_mock_backend_no_create __attribute__((unused)) = {
    NULL, NULL, NULL, NULL, NULL
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

    sapi_timer_config_t config __attribute__((unused)) = {0};
    config.mode = SAPI_TIMER_MODE_PERIODIC;
    config.period_ms = 100U;
    config.callback = dummy_callback;
    config.user_ctx = NULL;

    /* No backend registered yet: valid params, but nothing to dispatch to. */
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_NOT_INITIALIZED);

    /* Registering NULL is rejected. */
    assert(sapi_timer_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* A backend with a NULL create slot yields NOT_SUPPORTED. */
    assert(sapi_timer_register_backend(&g_mock_backend_no_create) == SAPI_STATUS_OK);
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_NOT_SUPPORTED);

    /* A fully-populated backend is actually reached, with the same
     * already-validated arguments the caller passed in. */
    assert(sapi_timer_register_backend(&g_mock_backend_full) == SAPI_STATUS_OK);
    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(g_mock_create_calls == 1);
    assert(handle != NULL);

    return 0;
}
