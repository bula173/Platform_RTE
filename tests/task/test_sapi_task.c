/* Tests for the sapi_task validate-then-dispatch API (ADR-005): see
 * tests/nvm/test_sapi_nvm.c for the pattern this follows. */
#include <assert.h>
#include "safeapi/task/sapi_task.h"
#include "safeapi_backend/task/sapi_task_backend.h"

static void mock_entry(void *user_ctx)
{
    (void)user_ctx;
}

static sapi_status_t mock_create(sapi_task_storage_t *storage,
                                  const sapi_task_config_t *config,
                                  sapi_task_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    *out_handle = (sapi_task_handle_t)(void *)1;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_start(sapi_task_handle_t handle)
{
    (void)handle;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_suspend(sapi_task_handle_t handle)
{
    (void)handle;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_destroy(sapi_task_handle_t handle)
{
    (void)handle;
    return SAPI_STATUS_OK;
}

/* Partial backend: no slots implemented -> exercises NOT_SUPPORTED paths. */
static const sapi_task_backend_t g_partial_backend = {
    NULL, NULL, NULL, NULL
};

/* Full backend: every slot implemented -> exercises success paths. */
static const sapi_task_backend_t g_full_backend = {
    mock_create, mock_start, mock_suspend, mock_destroy
};

int main(void)
{
    sapi_task_storage_t storage;
    sapi_task_handle_t handle = NULL;
    sapi_task_config_t config;

    config.name = "cyclic_task";
    config.entry = mock_entry;
    config.user_ctx = NULL;
    config.period_ms = 10U;
    config.priority = 5U;
    config.stack_size = 256U;

    /* --- sapi_task_create: INVALID_PARAM paths --- */
    assert(sapi_task_create(NULL, &config, &handle) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_task_create(&storage, NULL, &handle) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_task_create(&storage, &config, NULL) == SAPI_STATUS_INVALID_PARAM);

    {
        sapi_task_config_t bad_entry = config;
        bad_entry.entry = NULL;
        assert(sapi_task_create(&storage, &bad_entry, &handle) == SAPI_STATUS_INVALID_PARAM);
    }

    /* --- Not yet registered: NOT_INITIALIZED paths --- */
    assert(sapi_task_create(&storage, &config, &handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_task_start((sapi_task_handle_t)(void *)1) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_task_suspend((sapi_task_handle_t)(void *)1) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_task_destroy((sapi_task_handle_t)(void *)1) == SAPI_STATUS_NOT_INITIALIZED);

    /* --- sapi_task_start/suspend/destroy: INVALID_PARAM paths --- */
    assert(sapi_task_start(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_task_suspend(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_task_destroy(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* --- register_backend --- */
    assert(sapi_task_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_task_register_backend(&g_partial_backend) == SAPI_STATUS_OK);

    /* --- Partial backend registered: NOT_SUPPORTED paths --- */
    assert(sapi_task_create(&storage, &config, &handle) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_task_start((sapi_task_handle_t)(void *)1) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_task_suspend((sapi_task_handle_t)(void *)1) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_task_destroy((sapi_task_handle_t)(void *)1) == SAPI_STATUS_NOT_SUPPORTED);

    /* --- Full backend registered: success paths --- */
    assert(sapi_task_register_backend(&g_full_backend) == SAPI_STATUS_OK);

    handle = NULL;
    assert(sapi_task_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);

    assert(sapi_task_start(handle) == SAPI_STATUS_OK);
    assert(sapi_task_suspend(handle) == SAPI_STATUS_OK);
    assert(sapi_task_destroy(handle) == SAPI_STATUS_OK);

    return 0;
}
