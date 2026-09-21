/* Tests for the rte_task validate-then-dispatch API (ADR-005): see
 * tests/nvm/test_rte_nvm.c for the pattern this follows. */
#include <assert.h>
#include "safeapi/oal/task/rte_task.h"
#include "safeapi_osadapter/task/rte_osadapter_task.h"

static void mock_entry(void *user_ctx)
{
    (void)user_ctx;
}

static rte_status_t mock_create(rte_task_storage_t *storage,
                                  const rte_task_config_t *config,
                                  rte_task_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    *out_handle = (rte_task_handle_t)(void *)1;
    return RTE_STATUS_OK;
}

static rte_status_t mock_start(rte_task_handle_t handle)
{
    (void)handle;
    return RTE_STATUS_OK;
}

static rte_status_t mock_suspend(rte_task_handle_t handle)
{
    (void)handle;
    return RTE_STATUS_OK;
}

static rte_status_t mock_destroy(rte_task_handle_t handle)
{
    (void)handle;
    return RTE_STATUS_OK;
}

/* Partial OSAdapter: no slots implemented -> exercises NOT_SUPPORTED paths. */
static const rte_osadapter_task_t g_partial_osadapter = {
    NULL, NULL, NULL, NULL
};

/* Full OSAdapter: every slot implemented -> exercises success paths. */
static const rte_osadapter_task_t g_full_osadapter = {
    mock_create, mock_start, mock_suspend, mock_destroy
};

int main(void)
{
    rte_task_storage_t storage;
    rte_task_handle_t handle = NULL;
    rte_task_config_t config;

    config.name = "cyclic_task";
    config.entry = mock_entry;
    config.user_ctx = NULL;
    config.period_ms = 10U;
    config.priority = 5U;
    config.stack_size = 256U;

    /* --- rte_task_create: INVALID_PARAM paths --- */
    assert(rte_task_create(NULL, &config, &handle) == RTE_STATUS_INVALID_PARAM);
    assert(rte_task_create(&storage, NULL, &handle) == RTE_STATUS_INVALID_PARAM);
    assert(rte_task_create(&storage, &config, NULL) == RTE_STATUS_INVALID_PARAM);

    {
        rte_task_config_t bad_entry = config;
        bad_entry.entry = NULL;
        assert(rte_task_create(&storage, &bad_entry, &handle) == RTE_STATUS_INVALID_PARAM);
    }

    /* --- Not yet registered: NOT_INITIALIZED paths --- */
    assert(rte_task_create(&storage, &config, &handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_task_start((rte_task_handle_t)(void *)1) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_task_suspend((rte_task_handle_t)(void *)1) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_task_destroy((rte_task_handle_t)(void *)1) == RTE_STATUS_NOT_INITIALIZED);

    /* --- rte_task_start/suspend/destroy: INVALID_PARAM paths --- */
    assert(rte_task_start(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_task_suspend(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_task_destroy(NULL) == RTE_STATUS_INVALID_PARAM);

    /* --- register_osadapter --- */
    assert(rte_osadapter_task_register(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_osadapter_task_register(&g_partial_osadapter) == RTE_STATUS_OK);

    /* --- Partial OSAdapter registered: NOT_SUPPORTED paths --- */
    assert(rte_task_create(&storage, &config, &handle) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_task_start((rte_task_handle_t)(void *)1) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_task_suspend((rte_task_handle_t)(void *)1) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_task_destroy((rte_task_handle_t)(void *)1) == RTE_STATUS_NOT_SUPPORTED);

    /* --- Full OSAdapter registered: success paths --- */
    assert(rte_osadapter_task_register(&g_full_osadapter) == RTE_STATUS_OK);

    handle = NULL;
    assert(rte_task_create(&storage, &config, &handle) == RTE_STATUS_OK);
    assert(handle != NULL);

    assert(rte_task_start(handle) == RTE_STATUS_OK);
    assert(rte_task_suspend(handle) == RTE_STATUS_OK);
    assert(rte_task_destroy(handle) == RTE_STATUS_OK);

    return 0;
}
