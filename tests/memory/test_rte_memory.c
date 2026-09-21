/* Tests for the rte_mem_pool validate-then-dispatch API (ADR-005): see
 * tests/nvm/test_rte_nvm.c for the pattern this follows. */
#include <assert.h>
#include "safeapi/oal/memory/rte_memory.h"
#include "safeapi_osadapter/memory/rte_osadapter_memory.h"

static unsigned char g_block[8];

static rte_status_t mock_create(rte_mem_pool_storage_t *storage,
                                  const rte_mem_pool_config_t *config,
                                  rte_mem_pool_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    *out_handle = (rte_mem_pool_handle_t)(void *)1;
    return RTE_STATUS_OK;
}

static rte_status_t mock_acquire(rte_mem_pool_handle_t handle, void **out_block)
{
    (void)handle;
    *out_block = (void *)g_block;
    return RTE_STATUS_OK;
}

static rte_status_t mock_release(rte_mem_pool_handle_t handle, void *block)
{
    (void)handle;
    (void)block;
    return RTE_STATUS_OK;
}

static rte_status_t mock_stats(rte_mem_pool_handle_t handle,
                                 size_t *out_free_blocks, size_t *out_used_blocks)
{
    (void)handle;
    *out_free_blocks = 3U;
    *out_used_blocks = 1U;
    return RTE_STATUS_OK;
}

/* Partial OSAdapter: no slots implemented -> exercises NOT_SUPPORTED paths. */
static const rte_osadapter_memory_t g_partial_osadapter = {
    NULL, NULL, NULL, NULL
};

/* Full OSAdapter: every slot implemented -> exercises success paths. */
static const rte_osadapter_memory_t g_full_osadapter = {
    mock_create, mock_acquire, mock_release, mock_stats
};

int main(void)
{
    rte_mem_pool_storage_t storage;
    rte_mem_pool_handle_t handle = NULL;
    rte_mem_pool_config_t config;
    void *block = NULL;
    size_t free_blocks = 0U;
    size_t used_blocks = 0U;

    config.block_size = 16U;
    config.block_count = 4U;

    /* --- rte_mem_pool_create: INVALID_PARAM paths --- */
    assert(rte_mem_pool_create(NULL, &config, &handle) == RTE_STATUS_INVALID_PARAM);
    assert(rte_mem_pool_create(&storage, NULL, &handle) == RTE_STATUS_INVALID_PARAM);
    assert(rte_mem_pool_create(&storage, &config, NULL) == RTE_STATUS_INVALID_PARAM);

    {
        rte_mem_pool_config_t bad_size = config;
        bad_size.block_size = 0U;
        assert(rte_mem_pool_create(&storage, &bad_size, &handle) == RTE_STATUS_INVALID_PARAM);
    }
    {
        rte_mem_pool_config_t bad_count = config;
        bad_count.block_count = 0U;
        assert(rte_mem_pool_create(&storage, &bad_count, &handle) == RTE_STATUS_INVALID_PARAM);
    }

    /* --- Not yet registered: NOT_INITIALIZED paths --- */
    assert(rte_mem_pool_create(&storage, &config, &handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_mem_pool_acquire((rte_mem_pool_handle_t)(void *)1, &block) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_mem_pool_release((rte_mem_pool_handle_t)(void *)1, (void *)g_block) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_mem_pool_stats((rte_mem_pool_handle_t)(void *)1, &free_blocks, &used_blocks) == RTE_STATUS_NOT_INITIALIZED);

    /* --- rte_mem_pool_acquire: INVALID_PARAM paths --- */
    assert(rte_mem_pool_acquire(NULL, &block) == RTE_STATUS_INVALID_PARAM);
    assert(rte_mem_pool_acquire((rte_mem_pool_handle_t)(void *)1, NULL) == RTE_STATUS_INVALID_PARAM);

    /* --- rte_mem_pool_release: INVALID_PARAM paths --- */
    assert(rte_mem_pool_release(NULL, (void *)g_block) == RTE_STATUS_INVALID_PARAM);
    assert(rte_mem_pool_release((rte_mem_pool_handle_t)(void *)1, NULL) == RTE_STATUS_INVALID_PARAM);

    /* --- rte_mem_pool_stats: INVALID_PARAM paths --- */
    assert(rte_mem_pool_stats(NULL, &free_blocks, &used_blocks) == RTE_STATUS_INVALID_PARAM);
    assert(rte_mem_pool_stats((rte_mem_pool_handle_t)(void *)1, NULL, &used_blocks) == RTE_STATUS_INVALID_PARAM);
    assert(rte_mem_pool_stats((rte_mem_pool_handle_t)(void *)1, &free_blocks, NULL) == RTE_STATUS_INVALID_PARAM);

    /* --- register_osadapter --- */
    assert(rte_osadapter_memory_register(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_osadapter_memory_register(&g_partial_osadapter) == RTE_STATUS_OK);

    /* --- Partial OSAdapter registered: NOT_SUPPORTED paths --- */
    assert(rte_mem_pool_create(&storage, &config, &handle) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_mem_pool_acquire((rte_mem_pool_handle_t)(void *)1, &block) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_mem_pool_release((rte_mem_pool_handle_t)(void *)1, (void *)g_block) == RTE_STATUS_NOT_SUPPORTED);
    assert(rte_mem_pool_stats((rte_mem_pool_handle_t)(void *)1, &free_blocks, &used_blocks) == RTE_STATUS_NOT_SUPPORTED);

    /* --- Full OSAdapter registered: success paths --- */
    assert(rte_osadapter_memory_register(&g_full_osadapter) == RTE_STATUS_OK);

    handle = NULL;
    assert(rte_mem_pool_create(&storage, &config, &handle) == RTE_STATUS_OK);
    assert(handle != NULL);

    block = NULL;
    assert(rte_mem_pool_acquire(handle, &block) == RTE_STATUS_OK);
    assert(block == (void *)g_block);

    assert(rte_mem_pool_release(handle, block) == RTE_STATUS_OK);

    free_blocks = 0U;
    used_blocks = 0U;
    assert(rte_mem_pool_stats(handle, &free_blocks, &used_blocks) == RTE_STATUS_OK);
    assert(free_blocks == 3U);
    assert(used_blocks == 1U);

    return 0;
}
