/**
 * @file sapi_timer.c
 * @brief Timer service: validates parameters, then dispatches to the
 *        backend registered via sapi_timer_register_backend() (ADR-005).
 */
#include "safeapi/timer/sapi_timer.h"

static const sapi_timer_backend_t *s_backend = NULL;

sapi_status_t sapi_timer_register_backend(const sapi_timer_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_timer_create(sapi_timer_storage_t *storage,
                                 const sapi_timer_config_t *config,
                                 sapi_timer_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->callback == NULL) || (config->period_ms == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, config, out_handle);
}

sapi_status_t sapi_timer_start(sapi_timer_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->start == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->start(handle);
}

sapi_status_t sapi_timer_stop(sapi_timer_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->stop == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->stop(handle);
}

sapi_status_t sapi_timer_destroy(sapi_timer_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->destroy == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->destroy(handle);
}

sapi_status_t sapi_timer_now(sapi_timestamp_ms_t *out_now_ms)
{
    if (out_now_ms == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_now_ms = 0U;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->now == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->now(out_now_ms);
}
