#define _POSIX_C_SOURCE 200809L

/**
 * @file sapi_posix_backend_timer.c
 * @brief POSIX sapi_timer backend: one pthread per timer, sleeping via
 *        clock_nanosleep(CLOCK_MONOTONIC) (ADR-018 section 2.3).
 */
#include "safeapi/posix_backend/sapi_posix_backend.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

typedef struct
{
    pthread_t thread;
    sapi_timer_config_t config;
    volatile sig_atomic_t running;
    volatile sig_atomic_t should_stop;
    sapi_timer_handle_t self_handle;
} posix_timer_state_t;

/* C99-compatible static assert: posix_timer_state_t must fit inside
 * sapi_timer_storage_t's reserved bytes (a negative array size is a
 * compile error). */
typedef char posix_timer_storage_fits_[(sizeof(posix_timer_state_t) <= sizeof(sapi_timer_storage_t)) ? 1 : -1];

static void ms_to_timespec(sapi_duration_ms_t ms, struct timespec *out_ts)
{
    out_ts->tv_sec = (time_t)(ms / 1000U);
    out_ts->tv_nsec = (long)((ms % 1000U) * 1000000L);
}

static void *timer_thread_main(void *arg)
{
    posix_timer_state_t *state = (posix_timer_state_t *)arg;
    struct timespec period_ts;

    ms_to_timespec(state->config.period_ms, &period_ts);

    for (;;)
    {
        struct timespec remaining = period_ts;
        int rc;

        do
        {
            rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &remaining, &remaining);
        } while ((rc == EINTR) && (state->should_stop == 0));

        if (state->should_stop != 0)
        {
            break;
        }

        state->config.callback(state->self_handle, state->config.user_ctx);

        if (state->config.mode != SAPI_TIMER_MODE_PERIODIC)
        {
            break;
        }
    }

    state->running = 0;
    return NULL;
}

static sapi_status_t backend_create(sapi_timer_storage_t *storage, const sapi_timer_config_t *config,
                                     sapi_timer_handle_t *out_handle)
{
    posix_timer_state_t *state;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL) || (config->callback == NULL)
        || (config->period_ms == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    state = (posix_timer_state_t *)(void *)storage;
    memset(state, 0, sizeof(*state));
    state->config = *config;
    state->self_handle = (sapi_timer_handle_t)(void *)storage;
    *out_handle = state->self_handle;
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_stop(sapi_timer_handle_t handle)
{
    posix_timer_state_t *state = (posix_timer_state_t *)(void *)handle;

    if (state == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (state->running != 0)
    {
        state->should_stop = 1;
        (void)pthread_join(state->thread, NULL);
        state->running = 0;
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_start(sapi_timer_handle_t handle)
{
    posix_timer_state_t *state = (posix_timer_state_t *)(void *)handle;
    sapi_status_t stop_status;

    if (state == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* sapi_timer_start() on an already-running timer means "restart":
     * stop the old thread cleanly before starting a fresh one. */
    stop_status = backend_stop(handle);
    if (stop_status != SAPI_STATUS_OK)
    {
        return stop_status;
    }

    state->should_stop = 0;
    if (pthread_create(&state->thread, NULL, timer_thread_main, state) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    state->running = 1;
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_destroy(sapi_timer_handle_t handle)
{
    return backend_stop(handle);
}

static sapi_status_t backend_now(sapi_timestamp_ms_t *out_now_ms)
{
    struct timespec ts;

    if (out_now_ms == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        *out_now_ms = 0U;
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    *out_now_ms = (sapi_timestamp_ms_t)(((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL));
    return SAPI_STATUS_OK;
}

static const sapi_timer_backend_t s_posix_timer_backend = { backend_create, backend_start, backend_stop,
                                                              backend_destroy, backend_now };

const sapi_timer_backend_t *sapi_posix_backend_timer(void)
{
    return &s_posix_timer_backend;
}
