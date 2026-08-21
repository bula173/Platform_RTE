/* Tests for the sapi_mem_pool validate-then-dispatch API (ADR-005): see
 * tests/nvm/test_sapi_nvm.c for the pattern this follows. */
#include <assert.h>
#include "safeapi/memory/sapi_memory.h"
#include "safeapi_backend/memory/sapi_memory_backend.h"

static unsigned char g_block[8];

static sapi_status_t mock_create(sapi_mem_pool_storage_t *storage,
                                  const sapi_mem_pool_config_t *config,
                                  sapi_mem_pool_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    *out_handle = (sapi_mem_pool_handle_t)(void *)1;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_acquire(sapi_mem_pool_handle_t handle, void **out_block)
{
    (void)handle;
    *out_block = (void *)g_block;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_release(sapi_mem_pool_handle_t handle, void *block)
{
    (void)handle;
    (void)block;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_stats(sapi_mem_pool_handle_t handle,
                                 size_t *out_free_blocks, size_t *out_used_blocks)
{
    (void)handle;
    *out_free_blocks = 3U;
    *out_used_blocks = 1U;
    return SAPI_STATUS_OK;
}

/* Partial backend: no slots implemented -> exercises NOT_SUPPORTED paths. */
static const sapi_mem_pool_backend_t g_partial_backend = {
    NULL, NULL, NULL, NULL
};

/* Full backend: every slot implemented -> exercises success paths. */
static const sapi_mem_pool_backend_t g_full_backend = {
    mock_create, mock_acquire, mock_release, mock_stats
};

int main(void)
{
    sapi_mem_pool_storage_t storage;
    sapi_mem_pool_handle_t handle = NULL;
    sapi_mem_pool_config_t config;
    void *block = NULL;
    size_t free_blocks = 0U;
    size_t used_blocks = 0U;

    config.block_size = 16U;
    config.block_count = 4U;

    /* --- sapi_mem_pool_create: INVALID_PARAM paths --- */
    assert(sapi_mem_pool_create(NULL, &config, &handle) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_mem_pool_create(&storage, NULL, &handle) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_mem_pool_create(&storage, &config, NULL) == SAPI_STATUS_INVALID_PARAM);

    {
        sapi_mem_pool_config_t bad_size = config;
        bad_size.block_size = 0U;
        assert(sapi_mem_pool_create(&storage, &bad_size, &handle) == SAPI_STATUS_INVALID_PARAM);
    }
    {
        sapi_mem_pool_config_t bad_count = config;
        bad_count.block_count = 0U;
        assert(sapi_mem_pool_create(&storage, &bad_count, &handle) == SAPI_STATUS_INVALID_PARAM);
    }

    /* --- Not yet registered: NOT_INITIALIZED paths --- */
    assert(sapi_mem_pool_create(&storage, &config, &handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_mem_pool_acquire((sapi_mem_pool_handle_t)(void *)1, &block) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_mem_pool_release((sapi_mem_pool_handle_t)(void *)1, (void *)g_block) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_mem_pool_stats((sapi_mem_pool_handle_t)(void *)1, &free_blocks, &used_blocks) == SAPI_STATUS_NOT_INITIALIZED);

    /* --- sapi_mem_pool_acquire: INVALID_PARAM paths --- */
    assert(sapi_mem_pool_acquire(NULL, &block) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_mem_pool_acquire((sapi_mem_pool_handle_t)(void *)1, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* --- sapi_mem_pool_release: INVALID_PARAM paths --- */
    assert(sapi_mem_pool_release(NULL, (void *)g_block) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_mem_pool_release((sapi_mem_pool_handle_t)(void *)1, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* --- sapi_mem_pool_stats: INVALID_PARAM paths --- */
    assert(sapi_mem_pool_stats(NULL, &free_blocks, &used_blocks) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_mem_pool_stats((sapi_mem_pool_handle_t)(void *)1, NULL, &used_blocks) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_mem_pool_stats((sapi_mem_pool_handle_t)(void *)1, &free_blocks, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* --- register_backend --- */
    assert(sapi_mem_pool_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_mem_pool_register_backend(&g_partial_backend) == SAPI_STATUS_OK);

    /* --- Partial backend registered: NOT_SUPPORTED paths --- */
    assert(sapi_mem_pool_create(&storage, &config, &handle) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_mem_pool_acquire((sapi_mem_pool_handle_t)(void *)1, &block) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_mem_pool_release((sapi_mem_pool_handle_t)(void *)1, (void *)g_block) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_mem_pool_stats((sapi_mem_pool_handle_t)(void *)1, &free_blocks, &used_blocks) == SAPI_STATUS_NOT_SUPPORTED);

    /* --- Full backend registered: success paths --- */
    assert(sapi_mem_pool_register_backend(&g_full_backend) == SAPI_STATUS_OK);

    handle = NULL;
    assert(sapi_mem_pool_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);

    block = NULL;
    assert(sapi_mem_pool_acquire(handle, &block) == SAPI_STATUS_OK);
    assert(block == (void *)g_block);

    assert(sapi_mem_pool_release(handle, block) == SAPI_STATUS_OK);

    free_blocks = 0U;
    used_blocks = 0U;
    assert(sapi_mem_pool_stats(handle, &free_blocks, &used_blocks) == SAPI_STATUS_OK);
    assert(free_blocks == 3U);
    assert(used_blocks == 1U);

    return 0;
}
