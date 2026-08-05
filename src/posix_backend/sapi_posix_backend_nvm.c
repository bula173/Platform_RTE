#define _POSIX_C_SOURCE 200809L

/**
 * @file sapi_posix_backend_nvm.c
 * @brief POSIX sapi_nvm backend: one plain file per region, with a
 *        whole-region FNV-1a-64 hash trailer recomputed on every write and
 *        verified on every read (ADR-018 section 2.3), satisfying
 *        REQ-OAL-NVM-001's "every read shall be integrity-checked" via a
 *        single trailing hash rather than a redundant-copy vote.
 *
 * Deliberately does NOT reuse sapi_checksum's CRC-64: that module's
 * lookup tables are known-incomplete placeholders (flagged in ADR-017 and
 * docs/MISRA_COMPLIANCE_REPORT.md). Building this backend's integrity
 * check on top of a hash already known to be broken would just move the
 * problem, not solve it. FNV-1a-64 is simple enough to implement
 * correctly inline, with no lookup table to get wrong.
 *
 * Region files live in the directory named by the SAPI_POSIX_NVM_DIR
 * environment variable, or the current working directory if unset -
 * there is no config field for this in sapi_nvm_config_t, and inventing
 * one would be an API change outside this ADR's scope.
 *
 * Known simplification: verifying a read means hashing the *whole*
 * region on every sapi_nvm_read() call, not just the bytes requested -
 * correct, but O(region_size) per read rather than O(bytes read). Fine
 * for the region sizes a first backend is expected to see; a production
 * backend for large regions would want per-block hashes instead.
 */
#include "safeapi/posix_backend/sapi_posix_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct
{
    int    fd;
    size_t region_size;
} posix_nvm_state_t;

/* C99-compatible static assert: posix_nvm_state_t must fit inside
 * sapi_nvm_storage_t's reserved bytes. */
typedef char posix_nvm_storage_fits_[(sizeof(posix_nvm_state_t) <= sizeof(sapi_nvm_storage_t)) ? 1 : -1];

#define SAPI_POSIX_NVM_TRAILER_SIZE 8U
#define SAPI_POSIX_NVM_PATH_MAX     256U
#define SAPI_POSIX_NVM_CHUNK_SIZE   256U

static const uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
static const uint64_t FNV_PRIME        = 0x100000001b3ULL;

static uint64_t fnv1a64_update(uint64_t hash, const uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0U; i < len; i++)
    {
        hash ^= (uint64_t)data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static void hash_to_bytes(uint64_t hash, uint8_t out[SAPI_POSIX_NVM_TRAILER_SIZE])
{
    size_t i;

    for (i = 0U; i < SAPI_POSIX_NVM_TRAILER_SIZE; i++)
    {
        out[i] = (uint8_t)((hash >> (8U * i)) & 0xFFU);
    }
}

static uint64_t bytes_to_hash(const uint8_t in[SAPI_POSIX_NVM_TRAILER_SIZE])
{
    uint64_t hash = 0U;
    size_t i;

    for (i = 0U; i < SAPI_POSIX_NVM_TRAILER_SIZE; i++)
    {
        hash |= ((uint64_t)in[i]) << (8U * i);
    }
    return hash;
}

/** EINTR-retrying full read at a given offset. Returns true on a
 * complete read of len bytes, false otherwise (short read/error treated
 * alike - both mean the caller cannot trust the data). */
static bool read_at(int fd, size_t offset, void *buffer, size_t len)
{
    uint8_t *dst = (uint8_t *)buffer;
    size_t done = 0U;

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        return false;
    }
    while (done < len)
    {
        ssize_t n = read(fd, dst + done, len - done);

        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        if (n == 0)
        {
            return false; /* short read: not enough data in the file */
        }
        done += (size_t)n;
    }
    return true;
}

/** EINTR-retrying full write at a given offset. */
static bool write_at(int fd, size_t offset, const void *buffer, size_t len)
{
    const uint8_t *src = (const uint8_t *)buffer;
    size_t done = 0U;

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        return false;
    }
    while (done < len)
    {
        ssize_t n = write(fd, src + done, len - done);

        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        done += (size_t)n;
    }
    return true;
}

/** Hashes region_size bytes starting at file offset 0, streaming through
 * a fixed-size stack buffer (no dynamic allocation, per REQ-OAL-NVM-002). */
static bool compute_region_hash(int fd, size_t region_size, uint64_t *out_hash)
{
    uint8_t chunk[SAPI_POSIX_NVM_CHUNK_SIZE];
    uint64_t hash = FNV_OFFSET_BASIS;
    size_t remaining = region_size;
    size_t offset = 0U;

    while (remaining > 0U)
    {
        size_t this_chunk = (remaining < sizeof(chunk)) ? remaining : sizeof(chunk);

        if (!read_at(fd, offset, chunk, this_chunk))
        {
            return false;
        }
        hash = fnv1a64_update(hash, chunk, this_chunk);
        offset += this_chunk;
        remaining -= this_chunk;
    }

    *out_hash = hash;
    return true;
}

/** Recomputes the whole-region hash and (re)writes the trailer. */
static bool rewrite_trailer(int fd, size_t region_size)
{
    uint64_t hash;
    uint8_t trailer[SAPI_POSIX_NVM_TRAILER_SIZE];

    if (!compute_region_hash(fd, region_size, &hash))
    {
        return false;
    }
    hash_to_bytes(hash, trailer);
    return write_at(fd, region_size, trailer, sizeof(trailer));
}

static sapi_status_t backend_open(sapi_nvm_storage_t *storage, const sapi_nvm_config_t *config,
                                   sapi_nvm_handle_t *out_handle)
{
    posix_nvm_state_t *state;
    char path[SAPI_POSIX_NVM_PATH_MAX];
    const char *dir;
    int written;
    int fd;
    struct stat st;
    off_t expected_size;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL) || (config->region_name == NULL)
        || (config->region_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    dir = getenv("SAPI_POSIX_NVM_DIR");
    if (dir == NULL)
    {
        dir = ".";
    }
    written = snprintf(path, sizeof(path), "%s/sapi_nvm_%s.dat", dir, config->region_name);
    if ((written < 0) || ((size_t)written >= sizeof(path)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }

    expected_size = (off_t)(config->region_size + SAPI_POSIX_NVM_TRAILER_SIZE);
    if ((fstat(fd, &st) != 0) || (st.st_size != expected_size))
    {
        /* New or mismatched-size file: (re)initialize as an empty,
         * freshly-hashed region rather than trusting stale/partial
         * content of the wrong size. */
        if ((ftruncate(fd, 0) != 0) || (ftruncate(fd, expected_size) != 0))
        {
            (void)close(fd);
            return SAPI_STATUS_INTERNAL_ERROR;
        }
        if (!rewrite_trailer(fd, config->region_size))
        {
            (void)close(fd);
            return SAPI_STATUS_INTERNAL_ERROR;
        }
    }

    state = (posix_nvm_state_t *)(void *)storage;
    state->fd = fd;
    state->region_size = config->region_size;
    *out_handle = (sapi_nvm_handle_t)(void *)storage;
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_read(sapi_nvm_handle_t handle, size_t offset, void *out_buffer, size_t buffer_size)
{
    posix_nvm_state_t *state = (posix_nvm_state_t *)(void *)handle;
    uint64_t computed_hash;
    uint64_t stored_hash;
    uint8_t trailer[SAPI_POSIX_NVM_TRAILER_SIZE];

    if ((state == NULL) || (out_buffer == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((offset > state->region_size) || (buffer_size > (state->region_size - offset)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (!compute_region_hash(state->fd, state->region_size, &computed_hash)
        || !read_at(state->fd, state->region_size, trailer, sizeof(trailer)))
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    stored_hash = bytes_to_hash(trailer);
    if (computed_hash != stored_hash)
    {
        return SAPI_STATUS_DATA_CORRUPTION;
    }

    if (!read_at(state->fd, offset, out_buffer, buffer_size))
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_write(sapi_nvm_handle_t handle, size_t offset, const void *buffer, size_t buffer_size)
{
    posix_nvm_state_t *state = (posix_nvm_state_t *)(void *)handle;

    if ((state == NULL) || (buffer == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((offset > state->region_size) || (buffer_size > (state->region_size - offset)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (!write_at(state->fd, offset, buffer, buffer_size))
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    if (!rewrite_trailer(state->fd, state->region_size))
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_sync(sapi_nvm_handle_t handle)
{
    posix_nvm_state_t *state = (posix_nvm_state_t *)(void *)handle;

    if (state == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (fsync(state->fd) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    return SAPI_STATUS_OK;
}

static sapi_status_t backend_close(sapi_nvm_handle_t handle)
{
    posix_nvm_state_t *state = (posix_nvm_state_t *)(void *)handle;

    if (state == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    (void)close(state->fd);
    return SAPI_STATUS_OK;
}

static const sapi_nvm_backend_t s_posix_nvm_backend = { backend_open, backend_read, backend_write, backend_sync,
                                                          backend_close };

const sapi_nvm_backend_t *sapi_posix_backend_nvm(void)
{
    return &s_posix_nvm_backend;
}
