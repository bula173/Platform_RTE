#define _POSIX_C_SOURCE 200809L

/**
 * @file sapi_posix_backend_task.c
 * @brief POSIX sapi_task backend: one pthread per task (ADR-018 section
 *        2.3). period_ms > 0 runs entry in a clock_nanosleep-paced loop;
 *        period_ms == 0 runs entry once.
 *
 * Known simplification (stated in ADR-018): sapi_task has no native
 * "suspend, keep state, resume later" primitive in POSIX threads without
 * signal-based hacks this backend does not use. suspend() here stops the
 * thread cleanly (same as destroy() minus freeing the handle); start()
 * after suspend() begins a fresh run from the top of entry's cycle rather
 * than resuming mid-cycle. Acceptable for a first cut - a caller that
 * needs true pause/resume semantics needs a backend with more OS support
 * than a portable pthread implementation can offer.
 *
 * Priority: applied via pthread_setschedparam() with SCHED_FIFO when the
 * process holds the privilege to do so (typically CAP_SYS_NICE or root).
 * If that fails, the task still runs - just under the default scheduling
 * policy - rather than failing task creation outright, since most
 * development/CI environments cannot grant real-time scheduling privilege.
 * This fallback is a deliberate ADR-018 choice, not a silent bug.
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
    sapi_task_config_t config;
    volatile sig_atomic_t running;
    volatile sig_atomic_t should_stop;
    sapi_task_handle_t self_handle;
} posix_task_state_t;

/* C99-compatible static assert: posix_task_state_t must fit inside
 * sapi_task_storage_t's reserved bytes. */
typedef char posix_task_storage_fits_[(sizeof(posix_task_state_t) <= sizeof(sapi_task_storage_t)) ? 1 : -1];

static void ms_to_timespec(sapi_duration_ms_t ms, struct timespec *out_ts)
{
    out_ts->tv_sec = (time_t)(ms / 1000U);
    out_ts->tv_nsec = (long)((ms % 1000U) * 1000000L);
}

/** Best-effort SCHED_FIFO priority request. Failure is not fatal - the
 * task keeps running under whatever scheduling policy it already has
 * (see file header). priority is clamped into the platform's valid
 * SCHED_FIFO range rather than passed through unchecked. */
static void apply_priority_best_effort(pthread_t thread, uint32_t priority)
{
    struct sched_param sched;
    int min_prio = sched_get_priority_min(SCHED_FIFO);
    int max_prio = sched_get_priority_max(SCHED_FIFO);
    int wanted;

    if ((min_prio < 0) || (max_prio < 0))
    {
        return;
    }

    wanted = (int)priority;
    if (wanted < min_prio)
    {
        wanted = min_prio;
    }
    if (wanted > max_prio)
    {
        wanted = max_prio;
    }

    memset(&sched, 0, sizeof(sched));
    sched.sched_priority = wanted;
    (void)pthread_setschedparam(thread, SCHED_FIFO, &sched);
}

static void *task_thread_main(void *arg)
{
    posix_task_state_t *state = (posix_task_state_t *)arg;

    if (state->config.period_ms == 0U)
    {
        if (state->should_stop == 0)
        {
            state->config.entry(state->config.user_ctx);
        }
    }
    else
    {
        struct timespec period_ts;

        ms_to_timespec(state->config.period_ms, &period_ts);

        while (state->should_stop == 0)
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

            state->config.entry(state->config.user_ctx);
        }
    }

    state->running = 0;
    return NULL;
}

static sapi_status_t backend_create(sapi_task_storage_t *storage, const sapi_task_config_t *config,
                                     sapi_task_handle_t *out_handle)
{
    posix_task_state_t *state;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL) || (config->entry == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    state = (posix_task_state_t *)(void *)storage;
    memset(state, 0, sizeof(*state));
    state->config = *config;
    state->self_handle = (sapi_task_handle_t)(void *)storage;
    *out_handle = state->self_handle;
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_suspend(sapi_task_handle_t handle)
{
    posix_task_state_t *state = (posix_task_state_t *)(void *)handle;

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

static sapi_status_t backend_start(sapi_task_handle_t handle)
{
    posix_task_state_t *state = (posix_task_state_t *)(void *)handle;
    sapi_status_t suspend_status;
    pthread_attr_t attr;
    pthread_attr_t *attr_ptr = NULL;

    if (state == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Starting an already-running task means "restart from the top" -
     * see file header. */
    suspend_status = backend_suspend(handle);
    if (suspend_status != SAPI_STATUS_OK)
    {
        return suspend_status;
    }

    state->should_stop = 0;

    if (state->config.stack_size > 0U)
    {
        if (pthread_attr_init(&attr) == 0)
        {
            (void)pthread_attr_setstacksize(&attr, state->config.stack_size);
            attr_ptr = &attr;
        }
    }

    if (pthread_create(&state->thread, attr_ptr, task_thread_main, state) != 0)
    {
        if (attr_ptr != NULL)
        {
            (void)pthread_attr_destroy(attr_ptr);
        }
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    state->running = 1;

    apply_priority_best_effort(state->thread, state->config.priority);

    if (attr_ptr != NULL)
    {
        (void)pthread_attr_destroy(attr_ptr);
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_destroy(sapi_task_handle_t handle)
{
    return backend_suspend(handle);
}

static const sapi_task_backend_t s_posix_task_backend = { backend_create, backend_start, backend_suspend,
                                                            backend_destroy };

const sapi_task_backend_t *sapi_posix_backend_task(void)
{
    return &s_posix_task_backend;
}
