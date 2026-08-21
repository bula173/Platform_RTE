/* Tests for the sapi_nvm validate-then-dispatch API (ADR-005): see
 * test_sapi_timer.c for the pattern this follows. */
#include <assert.h>
#include "safeapi/nvm/sapi_nvm.h"
#include "safeapi_backend/nvm/sapi_nvm_backend.h"

static int g_mock_open_calls = 0;
static int g_mock_read_calls = 0;
static int g_mock_write_calls = 0;
static int g_mock_sync_calls = 0;
static int g_mock_close_calls = 0;

static sapi_status_t mock_open(sapi_nvm_storage_t *storage,
                                const sapi_nvm_config_t *config,
                                sapi_nvm_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    g_mock_open_calls++;
    *out_handle = (sapi_nvm_handle_t)(void *)1;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_read(sapi_nvm_handle_t handle, size_t offset,
                                void *out_buffer, size_t buffer_size)
{
    (void)handle;
    (void)offset;
    (void)out_buffer;
    (void)buffer_size;
    g_mock_read_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_write(sapi_nvm_handle_t handle, size_t offset,
                                 const void *buffer, size_t buffer_size)
{
    (void)handle;
    (void)offset;
    (void)buffer;
    (void)buffer_size;
    g_mock_write_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_sync(sapi_nvm_handle_t handle)
{
    (void)handle;
    g_mock_sync_calls++;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_close(sapi_nvm_handle_t handle)
{
    (void)handle;
    g_mock_close_calls++;
    return SAPI_STATUS_OK;
}

static const sapi_nvm_backend_t g_mock_backend __attribute__((unused)) = {
    mock_open, NULL, NULL, NULL, NULL
};

static const sapi_nvm_backend_t g_mock_backend_full __attribute__((unused)) = {
    mock_open, mock_read, mock_write, mock_sync, mock_close
};

int main(void)
{
    sapi_nvm_storage_t storage __attribute__((unused));
    sapi_nvm_handle_t handle __attribute__((unused)) = NULL;

    assert(sapi_nvm_open(NULL, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    sapi_nvm_config_t bad_config __attribute__((unused)) = {0};
    bad_config.region_name = NULL;
    bad_config.region_size = 128U;
    assert(sapi_nvm_open(&storage, &bad_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    /* Invalid config (region_size == 0) rejected even with a valid name. */
    sapi_nvm_config_t zero_size_config __attribute__((unused)) = {0};
    zero_size_config.region_name = "train_db";
    zero_size_config.region_size = 0U;
    assert(sapi_nvm_open(&storage, &zero_size_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    sapi_nvm_config_t config __attribute__((unused)) = {0};
    config.region_name = "train_db";
    config.region_size = 128U;

    /* No backend registered yet. */
    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_NOT_INITIALIZED);

    /* Null-parameter rejection for read/write/sync/close happens before
     * any backend is consulted, independently of one another. */
    unsigned char buf[4] __attribute__((unused));
    assert(sapi_nvm_read(NULL, 0U, buf, sizeof(buf)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_read(handle, 0U, NULL, sizeof(buf)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_read(handle, 0U, buf, 0U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_write(NULL, 0U, buf, sizeof(buf)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_write(handle, 0U, NULL, sizeof(buf)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_write(handle, 0U, buf, 0U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_sync(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_close(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* No backend registered yet: valid params, but nothing to dispatch to,
     * for every remaining entry point. */
    sapi_nvm_handle_t dummy_handle = (sapi_nvm_handle_t)(void *)1;
    assert(sapi_nvm_read(dummy_handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_nvm_write(dummy_handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_nvm_sync(dummy_handle) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_nvm_close(dummy_handle) == SAPI_STATUS_NOT_INITIALIZED);

    assert(sapi_nvm_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_register_backend(&g_mock_backend) == SAPI_STATUS_OK);
    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);
    assert(g_mock_open_calls == 1);

    /* read/write/sync/close all have NULL vtable slots in this mock. */
    assert(sapi_nvm_read(handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_nvm_write(handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_nvm_sync(handle) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_nvm_close(handle) == SAPI_STATUS_NOT_SUPPORTED);

    /* A backend with a NULL open slot yields NOT_SUPPORTED for open. */
    static const sapi_nvm_backend_t no_open_backend = {
        NULL, mock_read, mock_write, mock_sync, mock_close
    };
    assert(sapi_nvm_register_backend(&no_open_backend) == SAPI_STATUS_OK);
    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_NOT_SUPPORTED);

    /* A fully-populated backend is actually reached for every entry
     * point, with the same already-validated arguments the caller
     * passed in. */
    assert(sapi_nvm_register_backend(&g_mock_backend_full) == SAPI_STATUS_OK);
    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);
    assert(g_mock_open_calls == 2);

    assert(sapi_nvm_read(handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_OK);
    assert(g_mock_read_calls == 1);

    assert(sapi_nvm_write(handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_OK);
    assert(g_mock_write_calls == 1);

    assert(sapi_nvm_sync(handle) == SAPI_STATUS_OK);
    assert(g_mock_sync_calls == 1);

    assert(sapi_nvm_close(handle) == SAPI_STATUS_OK);
    assert(g_mock_close_calls == 1);

    return 0;
}
