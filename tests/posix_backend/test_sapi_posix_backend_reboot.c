/* _DEFAULT_SOURCE: setenv() below is a glibc/BSD-ish extension whose
 * declaration is gated behind this - needed before any header is
 * included. */
#define _DEFAULT_SOURCE

/* Test for the real POSIX sapi_reboot backend (ADR-018): a real
 * request() call re-execs this very binary (see
 * sapi_posix_backend_reboot.c), which is why this test cannot share a
 * process with test_sapi_posix_backend.c's assertions - a successful
 * reboot request never returns to the caller that issued it.
 *
 * Strategy: fork a child process. The child sets a marker environment
 * variable, then calls the backend's request(). If execv() succeeds, the
 * child process image is replaced and restarts at main() with the marker
 * still set (environment survives execv) plus SAPI_REBOOT_REASON_CODE set
 * by the backend - main() sees the marker, verifies the reason code, and
 * exits(0) instead of re-running the fork dance forever. The parent
 * waits for the child and checks it exited 0. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "safeapi/posix_backend/sapi_posix_backend.h"

#define TEST_REASON_CODE     4242U
#define REEXEC_MARKER_ENV    "SAPI_TEST_REBOOT_REEXEC_CHECK"

static int run_reexec_check(void)
{
    const char *reason_str = getenv("SAPI_REBOOT_REASON_CODE");
    long reason_value;

    if (reason_str == NULL)
    {
        (void)fprintf(stderr, "reexec check: SAPI_REBOOT_REASON_CODE missing\n");
        return 1;
    }
    reason_value = strtol(reason_str, NULL, 10);
    if (reason_value != (long)TEST_REASON_CODE)
    {
        (void)fprintf(stderr, "reexec check: expected reason %u, got %ld\n", TEST_REASON_CODE, reason_value);
        return 1;
    }
    return 0;
}

int main(void)
{
    pid_t pid;
    int status;

    if (getenv(REEXEC_MARKER_ENV) != NULL)
    {
        /* We are the re-exec'd process image; report success/failure by
         * exit code and stop - do NOT fall through to forking again. */
        return run_reexec_check();
    }

    assert(sapi_reboot_register_backend(sapi_posix_backend_reboot()) == SAPI_STATUS_OK);

    pid = fork();
    assert(pid >= 0);

    if (pid == 0)
    {
        const sapi_reboot_backend_t *backend = sapi_posix_backend_reboot();
        sapi_status_t request_status;

        (void)setenv(REEXEC_MARKER_ENV, "1", 1);
        request_status = backend->request(TEST_REASON_CODE);
        /* Only reached if execv() failed - a real reboot request never
         * returns on success. */
        (void)fprintf(stderr, "child: reboot request unexpectedly returned status %d\n", (int)request_status);
        _exit(2);
    }

    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    return 0;
}
