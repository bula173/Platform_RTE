/* _GNU_SOURCE (implies _POSIX_C_SOURCE) rather than _POSIX_C_SOURCE alone:
 * F_SETPIPE_SZ below is a Linux/glibc extension, not standard POSIX, and
 * is itself guarded with #ifdef so this file still builds - just without
 * that best-effort pipe-sizing call - on a non-glibc POSIX target. */
#define _GNU_SOURCE

/**
 * @file sapi_posix_backend_ipc.c
 * @brief POSIX sapi_ipc backend: one pipe() per channel (ADR-018 section
 *        2.2/2.3 - a mutex+condvar ring buffer would not fit sapi_ipc's
 *        64-byte opaque storage on glibc; a pipe lets the kernel own the
 *        queue instead).
 *
 * Known simplification (stated in ADR-018): a message larger than the
 * pipe's remaining buffer that partially transfers before timeout_ms
 * expires leaves a partial message in the pipe for the next receive() -
 * acceptable for a first cut, not hardened for production.
 */
#include "safeapi/posix_backend/sapi_posix_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
    int read_fd;
    int write_fd;
    size_t message_size;
} posix_ipc_state_t;

typedef char posix_ipc_storage_fits_[(sizeof(posix_ipc_state_t) <= sizeof(sapi_ipc_storage_t)) ? 1 : -1];

/** Milliseconds remaining until deadline, clamped to >= 0; -1 if now()
 *  itself fails (caller treats that as "no time left"). */
static int remaining_ms(const struct timespec *deadline)
{
    struct timespec now;
    int64_t delta_ms;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return -1;
    }
    delta_ms = ((int64_t)deadline->tv_sec - (int64_t)now.tv_sec) * 1000;
    delta_ms += ((int64_t)deadline->tv_nsec - (int64_t)now.tv_nsec) / 1000000;
    if (delta_ms < 0)
    {
        delta_ms = 0;
    }
    return (int)delta_ms;
}

static void compute_deadline(sapi_duration_ms_t timeout_ms, struct timespec *out_deadline)
{
    (void)clock_gettime(CLOCK_MONOTONIC, out_deadline);
    out_deadline->tv_sec += (time_t)(timeout_ms / 1000U);
    out_deadline->tv_nsec += (long)((timeout_ms % 1000U) * 1000000L);
    if (out_deadline->tv_nsec >= 1000000000L)
    {
        out_deadline->tv_sec += 1;
        out_deadline->tv_nsec -= 1000000000L;
    }
}

/** Shared poll-then-transfer loop. read_not_write selects direction;
 *  write_src/read_dst are mutually exclusive (only the one matching
 *  read_not_write's value is dereferenced) - kept as two parameters
 *  rather than one non-const void* so neither caller has to cast away
 *  const to use this helper. */
static sapi_status_t transfer_all(int fd, const void *write_src, void *read_dst, size_t size,
                                   sapi_duration_ms_t timeout_ms, bool read_not_write)
{
    struct timespec deadline;
    size_t done = 0;
    const uint8_t *src_bytes = (const uint8_t *)write_src;
    uint8_t *dst_bytes = (uint8_t *)read_dst;

    compute_deadline(timeout_ms, &deadline);

    while (done < size)
    {
        struct pollfd pfd;
        int rc;
        ssize_t n;
        int wait_ms = remaining_ms(&deadline);

        if (wait_ms <= 0)
        {
            return SAPI_STATUS_TIMEOUT;
        }

        pfd.fd = fd;
        pfd.events = read_not_write ? POLLIN : POLLOUT;
        pfd.revents = 0;
        rc = poll(&pfd, 1, wait_ms);
        if (rc == 0)
        {
            return SAPI_STATUS_TIMEOUT;
        }
        if (rc < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return SAPI_STATUS_INTERNAL_ERROR;
        }

        n = read_not_write ? read(fd, dst_bytes + done, size - done) : write(fd, src_bytes + done, size - done);
        if (n < 0)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
            {
                continue;
            }
            return SAPI_STATUS_INTERNAL_ERROR;
        }
        if (n == 0)
        {
            /* read() returning 0 means the write end was closed. */
            return SAPI_STATUS_INTERNAL_ERROR;
        }
        done += (size_t)n;
    }

    return SAPI_STATUS_OK;
}

static sapi_status_t backend_create(sapi_ipc_storage_t *storage, const sapi_ipc_config_t *config,
                                     sapi_ipc_handle_t *out_handle)
{
    posix_ipc_state_t *state;
    int fds[2];
    long requested_capacity;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL) || (config->message_size == 0U)
        || (config->queue_depth == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (pipe(fds) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    (void)fcntl(fds[0], F_SETFL, O_NONBLOCK);
    (void)fcntl(fds[1], F_SETFL, O_NONBLOCK);

    /* Best-effort: try to size the kernel pipe buffer to roughly
     * message_size * queue_depth so REQ-OAL-IPC-001's "bounded" queue
     * depth is approximated. Failure (permission, exceeds system limit)
     * is not fatal - the pipe still works with its default capacity. */
    requested_capacity = (long)(config->message_size * config->queue_depth);
#ifdef F_SETPIPE_SZ
    (void)fcntl(fds[1], F_SETPIPE_SZ, requested_capacity);
#else
    (void)requested_capacity; /* best-effort sizing unavailable on this target */
#endif

    state = (posix_ipc_state_t *)(void *)storage;
    state->read_fd = fds[0];
    state->write_fd = fds[1];
    state->message_size = config->message_size;
    *out_handle = (sapi_ipc_handle_t)(void *)storage;
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_send(sapi_ipc_handle_t handle, const void *message, size_t message_size,
                                   sapi_duration_ms_t timeout_ms)
{
    posix_ipc_state_t *state = (posix_ipc_state_t *)(void *)handle;

    if ((state == NULL) || (message == NULL) || (message_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    return transfer_all(state->write_fd, message, NULL, message_size, timeout_ms, false);
}

static sapi_status_t backend_receive(sapi_ipc_handle_t handle, void *out_message, size_t buffer_size,
                                      sapi_duration_ms_t timeout_ms)
{
    posix_ipc_state_t *state = (posix_ipc_state_t *)(void *)handle;

    if ((state == NULL) || (out_message == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    return transfer_all(state->read_fd, NULL, out_message, buffer_size, timeout_ms, true);
}

static sapi_status_t backend_destroy(sapi_ipc_handle_t handle)
{
    posix_ipc_state_t *state = (posix_ipc_state_t *)(void *)handle;

    if (state == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    (void)close(state->read_fd);
    (void)close(state->write_fd);
    return SAPI_STATUS_OK;
}

static const sapi_ipc_backend_t s_posix_ipc_backend = { backend_create, backend_send, backend_receive,
                                                          backend_destroy };

const sapi_ipc_backend_t *sapi_posix_backend_ipc(void)
{
    return &s_posix_ipc_backend;
}
