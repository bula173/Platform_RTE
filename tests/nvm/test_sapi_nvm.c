/* Tests for the sapi_nvm validate-then-dispatch API (ADR-005): see
 * test_sapi_timer.c for the pattern this follows. */
#include <assert.h>
#include "safeapi/nvm/sapi_nvm.h"

static sapi_status_t mock_open(sapi_nvm_storage_t *storage,
                                const sapi_nvm_config_t *config,
                                sapi_nvm_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    *out_handle = (sapi_nvm_handle_t)(void *)1;
    return SAPI_STATUS_OK;
}

static const sapi_nvm_backend_t g_mock_backend = {
    mock_open, NULL, NULL, NULL, NULL
};

int main(void)
{
    sapi_nvm_storage_t storage;
    sapi_nvm_handle_t handle = NULL;

    assert(sapi_nvm_open(NULL, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    sapi_nvm_config_t bad_config = {0};
    bad_config.region_name = NULL;
    bad_config.region_size = 128U;
    assert(sapi_nvm_open(&storage, &bad_config, &handle) == SAPI_STATUS_INVALID_PARAM);

    sapi_nvm_config_t config = {0};
    config.region_name = "train_db";
    config.region_size = 128U;

    /* No backend registered yet. */
    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_NOT_INITIALIZED);

    assert(sapi_nvm_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_nvm_register_backend(&g_mock_backend) == SAPI_STATUS_OK);
    assert(sapi_nvm_open(&storage, &config, &handle) == SAPI_STATUS_OK);
    assert(handle != NULL);

    /* read/write/sync/close all have NULL vtable slots in this mock. */
    unsigned char buf[4];
    assert(sapi_nvm_read(handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_nvm_write(handle, 0U, buf, sizeof(buf)) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_nvm_sync(handle) == SAPI_STATUS_NOT_SUPPORTED);
    assert(sapi_nvm_close(handle) == SAPI_STATUS_NOT_SUPPORTED);

    return 0;
}
