/**
 * @file sapi_posix_backend_memory.c
 * @brief POSIX sapi_memory backend: a single fixed-size static byte arena
 *        with a per-pool intrusive free list carved out of it at
 *        sapi_mem_pool_create() time (ADR-018 section 2.3).
 *
 * This is static partitioning, not malloc()/free(): the arena's total
 * size (SAPI_POSIX_MEM_ARENA_SIZE, default 1 MiB) is a compile-time
 * constant, every pool's claim against it is a one-way bump allocation
 * checked against remaining arena space (SAPI_STATUS_RESOURCE_EXHAUSTED
 * rather than growing anything), and no memory is ever returned to the
 * arena once a pool claims it - only individual blocks are recycled
 * within their own pool's free list. Matches REQ-OAL-MEM-001: pools are
 * expected to be reserved during system initialization.
 *
 * Known constraint: the free list is intrusive (a free block's first
 * sizeof(void*) bytes store the pointer to the next free block in the
 * same pool), so block_size must be >= sizeof(void*). A pool requesting a
 * smaller block_size is rejected with SAPI_STATUS_INVALID_PARAM rather
 * than silently corrupting adjacent memory.
 *
 * Known simplification: the global arena cursor (g_arena_used) and each
 * pool's free-list head are not protected by a lock. REQ-OAL-MEM-001
 * frames pool *reservation* as an initialization-time activity (a single
 * thread, before concurrent operation begins), so backend_create() being
 * unsynchronized matches that expectation; acquire()/release() on an
 * already-created pool being called concurrently from multiple threads is
 * NOT safe with this backend and is a documented limitation, not an
 * oversight - a production backend needing concurrent acquire/release
 * would need a lock or a lock-free free-list (e.g. compare-and-swap),
 * deliberately left out of this first cut.
 */
#include "safeapi/posix_backend/sapi_posix_backend.h"

#include <stdint.h>
#include <string.h>

#ifndef SAPI_POSIX_MEM_ARENA_SIZE
#define SAPI_POSIX_MEM_ARENA_SIZE (1024U * 1024U)
#endif

static uint8_t g_arena[SAPI_POSIX_MEM_ARENA_SIZE];
static size_t g_arena_used = 0U;

typedef struct
{
    uint8_t *base;
    size_t   block_size;
    size_t   block_count;
    size_t   free_count;
    void    *free_list_head;
} posix_mem_pool_state_t;

/* C99-compatible static assert: posix_mem_pool_state_t must fit inside
 * sapi_mem_pool_storage_t's reserved bytes. */
typedef char posix_mem_pool_storage_fits_[(sizeof(posix_mem_pool_state_t) <= sizeof(sapi_mem_pool_storage_t)) ? 1
                                                                                                                 : -1];

static sapi_status_t backend_create(sapi_mem_pool_storage_t *storage, const sapi_mem_pool_config_t *config,
                                     sapi_mem_pool_handle_t *out_handle)
{
    posix_mem_pool_state_t *state;
    size_t total_bytes;
    size_t i;
    uint8_t *base;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL) || (config->block_size == 0U)
        || (config->block_count == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (config->block_size < sizeof(void *))
    {
        return SAPI_STATUS_INVALID_PARAM; /* too small for the intrusive free list, see file header */
    }

    total_bytes = config->block_size * config->block_count;
    if ((total_bytes / config->block_size) != config->block_count)
    {
        return SAPI_STATUS_INVALID_PARAM; /* block_size * block_count overflowed size_t */
    }
    if (total_bytes > (SAPI_POSIX_MEM_ARENA_SIZE - g_arena_used))
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    base = &g_arena[g_arena_used];
    g_arena_used += total_bytes;

    for (i = 0U; i < config->block_count; i++)
    {
        uint8_t *block = base + (i * config->block_size);
        void *next = (i + 1U < config->block_count) ? (void *)(base + ((i + 1U) * config->block_size)) : NULL;

        memcpy(block, &next, sizeof(next));
    }

    state = (posix_mem_pool_state_t *)(void *)storage;
    state->base = base;
    state->block_size = config->block_size;
    state->block_count = config->block_count;
    state->free_count = config->block_count;
    state->free_list_head = (void *)base;

    *out_handle = (sapi_mem_pool_handle_t)(void *)storage;
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_acquire(sapi_mem_pool_handle_t handle, void **out_block)
{
    posix_mem_pool_state_t *state = (posix_mem_pool_state_t *)(void *)handle;
    void *block;
    void *next;

    if ((state == NULL) || (out_block == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (state->free_list_head == NULL)
    {
        *out_block = NULL;
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    block = state->free_list_head;
    memcpy(&next, block, sizeof(next));
    state->free_list_head = next;
    state->free_count -= 1U;
    *out_block = block;
    return SAPI_STATUS_OK;
}

static bool block_belongs_to_pool(const posix_mem_pool_state_t *state, const void *block)
{
    const uint8_t *b = (const uint8_t *)block;
    size_t byte_offset;

    if ((b < state->base) || (b >= (state->base + (state->block_size * state->block_count))))
    {
        return false;
    }
    byte_offset = (size_t)(b - state->base);
    return (byte_offset % state->block_size) == 0U;
}

static sapi_status_t backend_release(sapi_mem_pool_handle_t handle, void *block)
{
    posix_mem_pool_state_t *state = (posix_mem_pool_state_t *)(void *)handle;

    if ((state == NULL) || (block == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!block_belongs_to_pool(state, block))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    memcpy(block, &state->free_list_head, sizeof(state->free_list_head));
    state->free_list_head = block;
    state->free_count += 1U;
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_stats(sapi_mem_pool_handle_t handle, size_t *out_free_blocks, size_t *out_used_blocks)
{
    posix_mem_pool_state_t *state = (posix_mem_pool_state_t *)(void *)handle;

    if ((state == NULL) || (out_free_blocks == NULL) || (out_used_blocks == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_free_blocks = state->free_count;
    *out_used_blocks = state->block_count - state->free_count;
    return SAPI_STATUS_OK;
}

static const sapi_mem_pool_backend_t s_posix_memory_backend = { backend_create, backend_acquire, backend_release,
                                                                  backend_stats };

const sapi_mem_pool_backend_t *sapi_posix_backend_memory(void)
{
    return &s_posix_memory_backend;
}
