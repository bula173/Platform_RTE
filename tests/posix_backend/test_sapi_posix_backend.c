/* _DEFAULT_SOURCE: setenv() below is a glibc/BSD-ish extension whose
 * declaration is gated behind this (or _XOPEN_SOURCE 500..600, which
 * would in turn hide other symbols this file doesn't need) - needed
 * before any header is included. */
#define _DEFAULT_SOURCE

/* Integration tests for the real POSIX backend (ADR-018): unlike the other
 * services' unit tests, these register the actual sapi_posix_backend_*()
 * implementations (not a mock) and exercise them through the normal
 * sapi_<service>_* dispatch API, so a failure here means the real backend
 * - not just the dispatch layer - is broken. Covers timer, ipc, task, log,
 * nvm, memory. sapi_reboot is deliberately excluded (see
 * test_sapi_posix_backend_reboot.c: exec() replaces this process image, so
 * it cannot share a test binary with the others). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "safeapi/posix_backend/sapi_posix_backend.h"

static void sleep_ms(unsigned int ms)
{
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000U);
    ts.tv_nsec = (long)((ms % 1000U) * 1000000L);
    (void)nanosleep(&ts, NULL);
}

/* --------------------------------------------------------------------- */
/* timer                                                                  */
/* --------------------------------------------------------------------- */

static volatile int g_timer_fire_count = 0;

static void timer_callback(sapi_timer_handle_t handle, void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    g_timer_fire_count++;
}

static void test_timer(void)
{
    sapi_timer_storage_t storage;
    sapi_timer_handle_t handle = NULL;
    sapi_timer_config_t config;
    sapi_timestamp_ms_t t1 = 0U;
    sapi_timestamp_ms_t t2 = 0U;

    assert(sapi_timer_register_backend(sapi_posix_backend_timer()) == SAPI_STATUS_OK);

    memset(&config, 0, sizeof(config));
    config.mode = SAPI_TIMER_MODE_PERIODIC;
    config.period_ms = 20U;
    config.callback = timer_callback;

    assert(sapi_timer_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);
    assert(sapi_timer_start(handle) == SAPI_STATUS_OK);

    sleep_ms(150U); /* ~150ms: expect several 20ms periods to fire */

    assert(sapi_timer_stop(handle) == SAPI_STATUS_OK);
    assert(g_timer_fire_count >= 3);
    assert(sapi_timer_destroy(handle) == SAPI_STATUS_OK);

    assert(sapi_timer_now(&t1) == SAPI_STATUS_OK);
    sleep_ms(10U);
    assert(sapi_timer_now(&t2) == SAPI_STATUS_OK);
    assert(t2 > t1);
}

/* --------------------------------------------------------------------- */
/* ipc                                                                    */
/* --------------------------------------------------------------------- */

static void test_ipc(void)
{
    sapi_ipc_storage_t storage;
    sapi_ipc_handle_t handle = NULL;
    sapi_ipc_config_t config;
    const char message[] = "checkpoint-ok";
    char received[sizeof(message)];

    assert(sapi_ipc_register_backend(sapi_posix_backend_ipc()) == SAPI_STATUS_OK);

    memset(&config, 0, sizeof(config));
    config.name = "test_channel";
    config.message_size = sizeof(message);
    config.queue_depth = 4U;

    assert(sapi_ipc_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);

    assert(sapi_ipc_send(handle, message, sizeof(message), 100U) == SAPI_STATUS_OK);
    memset(received, 0, sizeof(received));
    assert(sapi_ipc_receive(handle, received, sizeof(received), 100U) == SAPI_STATUS_OK);
    assert(memcmp(message, received, sizeof(message)) == 0);

    /* Nothing queued: receive must time out, not block indefinitely. */
    assert(sapi_ipc_receive(handle, received, sizeof(received), 50U) == SAPI_STATUS_TIMEOUT);

    assert(sapi_ipc_destroy(handle) == SAPI_STATUS_OK);
}

/* --------------------------------------------------------------------- */
/* task                                                                   */
/* --------------------------------------------------------------------- */

static volatile int g_task_run_once_count = 0;
static volatile int g_task_periodic_count = 0;

static void task_entry_run_once(void *user_ctx)
{
    (void)user_ctx;
    g_task_run_once_count++;
}

static void task_entry_periodic(void *user_ctx)
{
    (void)user_ctx;
    g_task_periodic_count++;
}

static void test_task(void)
{
    sapi_task_storage_t once_storage;
    sapi_task_handle_t once_handle = NULL;
    sapi_task_config_t once_config;
    sapi_task_storage_t periodic_storage;
    sapi_task_handle_t periodic_handle = NULL;
    sapi_task_config_t periodic_config;

    assert(sapi_task_register_backend(sapi_posix_backend_task()) == SAPI_STATUS_OK);

    memset(&once_config, 0, sizeof(once_config));
    once_config.name = "run_once";
    once_config.entry = task_entry_run_once;
    once_config.period_ms = 0U;
    once_config.priority = 0U;
    once_config.stack_size = 0U;

    assert(sapi_task_create(&once_storage, &once_config, &once_handle) == SAPI_STATUS_OK);
    assert(sapi_task_start(once_handle) == SAPI_STATUS_OK);
    sleep_ms(50U);
    assert(g_task_run_once_count == 1);
    assert(sapi_task_destroy(once_handle) == SAPI_STATUS_OK);

    memset(&periodic_config, 0, sizeof(periodic_config));
    periodic_config.name = "periodic";
    periodic_config.entry = task_entry_periodic;
    periodic_config.period_ms = 20U;
    periodic_config.priority = 0U;
    periodic_config.stack_size = 65536U; /* exercise pthread_attr_setstacksize path */

    assert(sapi_task_create(&periodic_storage, &periodic_config, &periodic_handle) == SAPI_STATUS_OK);
    assert(sapi_task_start(periodic_handle) == SAPI_STATUS_OK);
    sleep_ms(150U);
    assert(sapi_task_suspend(periodic_handle) == SAPI_STATUS_OK);
    assert(g_task_periodic_count >= 3);
    assert(sapi_task_destroy(periodic_handle) == SAPI_STATUS_OK);
}

/* --------------------------------------------------------------------- */
/* log                                                                    */
/* --------------------------------------------------------------------- */

static void test_log(void)
{
    assert(sapi_log_register_backend(sapi_posix_backend_log()) == SAPI_STATUS_OK);
    assert(sapi_log_init() == SAPI_STATUS_OK);

    /* Non-blocking, best-effort: there is nothing to assert on the return
     * value (sapi_log_write() returns void, per REQ-OAL-LOG-001), just
     * that these calls do not crash, including with NULL tag/message. */
    sapi_log_write(SAPI_LOG_LEVEL_INFO, "test", "posix log backend smoke test");
    sapi_log_write(SAPI_LOG_LEVEL_ERROR, NULL, NULL);
}

/* --------------------------------------------------------------------- */
/* nvm                                                                    */
/* --------------------------------------------------------------------- */

static void test_nvm(void)
{
    sapi_nvm_storage_t storage;
    sapi_nvm_handle_t handle = NULL;
    sapi_nvm_config_t config;
    const uint8_t pattern[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    uint8_t read_back[16];
    char path[512];
    FILE *fp;

    /* Keep this test's region file out of the source tree and give it a
     * name unlikely to collide with a parallel test run. */
    (void)setenv("SAPI_POSIX_NVM_DIR", "/tmp", 1);

    assert(sapi_nvm_register_backend(sapi_posix_backend_nvm()) == SAPI_STATUS_OK);

    memset(&config, 0, sizeof(config));
    config.region_name = "sapi_test_region";
    config.region_size = sizeof(pattern);

    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);

    assert(sapi_nvm_write(handle, 0U, pattern, sizeof(pattern)) == SAPI_STATUS_OK);
    memset(read_back, 0, sizeof(read_back));
    assert(sapi_nvm_read(handle, 0U, read_back, sizeof(read_back)) == SAPI_STATUS_OK);
    assert(memcmp(pattern, read_back, sizeof(pattern)) == 0);

    assert(sapi_nvm_sync(handle) == SAPI_STATUS_OK);
    assert(sapi_nvm_close(handle) == SAPI_STATUS_OK);

    /* Corrupt one data byte directly on disk, bypassing the API, then
     * reopen and confirm the whole-region hash trailer catches it
     * (REQ-OAL-NVM-001). */
    (void)snprintf(path, sizeof(path), "/tmp/sapi_nvm_%s.dat", config.region_name);
    fp = fopen(path, "r+b");
    assert(fp != NULL);
    assert(fseek(fp, 0, SEEK_SET) == 0);
    {
        uint8_t corrupted_byte = (uint8_t)(pattern[0] ^ 0xFFU);
        assert(fwrite(&corrupted_byte, 1U, 1U, fp) == 1U);
    }
    (void)fclose(fp);

    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(sapi_nvm_read(handle, 0U, read_back, sizeof(read_back)) == SAPI_STATUS_DATA_CORRUPTION);
    assert(sapi_nvm_close(handle) == SAPI_STATUS_OK);

    (void)remove(path);
}

/* --------------------------------------------------------------------- */
/* memory                                                                 */
/* --------------------------------------------------------------------- */

static void test_memory(void)
{
    sapi_mem_pool_storage_t storage;
    sapi_mem_pool_handle_t handle = NULL;
    sapi_mem_pool_config_t config;
    void *blocks[4];
    void *overflow_block = (void *)1; /* sentinel to confirm it's set to NULL on exhaustion */
    size_t free_blocks = 0U;
    size_t used_blocks = 0U;
    size_t i;

    assert(sapi_mem_pool_register_backend(sapi_posix_backend_memory()) == SAPI_STATUS_OK);

    memset(&config, 0, sizeof(config));
    config.block_size = sizeof(void *); /* minimum viable for the intrusive free list */
    config.block_count = 4U;

    assert(sapi_mem_pool_create(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);

    for (i = 0U; i < 4U; i++)
    {
        assert(sapi_mem_pool_acquire(handle, &blocks[i]) == SAPI_STATUS_OK);
        assert(blocks[i] != NULL);
    }

    assert(sapi_mem_pool_acquire(handle, &overflow_block) == SAPI_STATUS_RESOURCE_EXHAUSTED);
    assert(overflow_block == NULL);

    assert(sapi_mem_pool_stats(handle, &free_blocks, &used_blocks) == SAPI_STATUS_OK);
    assert(free_blocks == 0U);
    assert(used_blocks == 4U);

    assert(sapi_mem_pool_release(handle, blocks[0]) == SAPI_STATUS_OK);
    assert(sapi_mem_pool_stats(handle, &free_blocks, &used_blocks) == SAPI_STATUS_OK);
    assert(free_blocks == 1U);
    assert(used_blocks == 3U);

    /* Releasing a pointer that never came from this pool must be rejected,
     * not corrupt the free list. */
    {
        int not_from_pool;
        assert(sapi_mem_pool_release(handle, &not_from_pool) == SAPI_STATUS_INVALID_PARAM);
    }
}

int main(void)
{
    test_timer();
    test_ipc();
    test_task();
    test_log();
    test_nvm();
    test_memory();
    return 0;
}
