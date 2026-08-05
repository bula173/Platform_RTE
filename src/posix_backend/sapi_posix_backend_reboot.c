/* _XOPEN_SOURCE 700 (implies _POSIX_C_SOURCE 200809L) rather than
 * _POSIX_C_SOURCE alone: setenv()/putenv() below are XSI extensions, not
 * base POSIX. */
#define _XOPEN_SOURCE 700

/**
 * @file sapi_posix_backend_reboot.c
 * @brief POSIX sapi_reboot backend: execv() re-exec of the current binary
 *        via /proc/self/exe (ADR-018 section 2.3).
 *
 * This is explicitly a stand-in, not a real safety-relevant reboot path.
 * A normal (non-root, non-embedded) Linux process cannot reset the CPU;
 * re-exec is the closest available approximation - it restarts the
 * process image from main() with the original argv, which exercises the
 * same "system comes back up from a clean start" application-level
 * contract a real reset would, without actually resetting anything below
 * the process. A real deployment on real hardware needs a real reset
 * mechanism (watchdog-triggered hardware reset, supervisory process
 * that respawns this one, etc.) - see ADR-018 section 3.
 *
 * reason_code is passed via an environment variable
 * (SAPI_REBOOT_REASON_CODE) rather than argv, so it survives the re-exec
 * without disturbing the caller's original argv[] the new process image
 * receives; a real backend on real hardware would instead persist it to
 * NVM/black-box storage before resetting (REQ-OAL-REBOOT-010's doc
 * comment), which is out of this stand-in's scope.
 */
#include "safeapi/posix_backend/sapi_posix_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* /proc/self/exe is Linux-specific (not portable POSIX); this backend is
 * documented as POSIX-ish/Linux-primary in ADR-018 section 2.4. */
#define SAPI_POSIX_SELF_EXE_PATH "/proc/self/exe"

extern char **environ;

static sapi_status_t backend_request(uint16_t reason_code)
{
    char reason_env[32];
    int written;

    written = snprintf(reason_env, sizeof(reason_env), "SAPI_REBOOT_REASON_CODE=%u", (unsigned int)reason_code);
    if ((written > 0) && ((size_t)written < sizeof(reason_env)))
    {
        (void)putenv(reason_env);
    }

    /* argv[0] here is a placeholder identifying the re-exec'd image; the
     * kernel resolves the actual executable from /proc/self/exe, not from
     * this string. Passing NULL-terminated {"sapi-reboot", NULL} keeps this
     * backend self-contained rather than requiring the caller's original
     * argv to be threaded through sapi_reboot_register_backend(). */
    {
        char *const argv_stub[] = { (char *)"sapi-reboot", NULL };

        (void)execv(SAPI_POSIX_SELF_EXE_PATH, argv_stub);
    }

    /* Reached only if execv() failed. */
    return SAPI_STATUS_NOT_SUPPORTED;
}

static const sapi_reboot_backend_t s_posix_reboot_backend = { backend_request };

const sapi_reboot_backend_t *sapi_posix_backend_reboot(void)
{
    return &s_posix_reboot_backend;
}
