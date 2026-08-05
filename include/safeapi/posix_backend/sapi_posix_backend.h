/**
 * @file sapi_posix_backend.h
 * @brief Real POSIX/Linux backend for every OAL service (ADR-018).
 *
 * A single sapi_posix_backend_register_all() call at startup registers
 * this file's implementation with every OAL service
 * (timer/ipc/task/log/nvm/reboot/memory) - the same "one entry point"
 * pattern sapi_appmanager already uses elsewhere. Individual
 * sapi_posix_backend_<service>() accessors are also exposed for callers
 * who want to register only some services with the real backend and
 * mock/stub the rest (e.g. in tests, or a hybrid deployment).
 *
 * See ADR-018 for what "real" means for each service and the specific,
 * deliberate simplifications each one makes (documented per-service in
 * that ADR's section 2.3, not repeated here).
 */
#ifndef SAPI_POSIX_BACKEND_H
#define SAPI_POSIX_BACKEND_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/timer/sapi_timer.h"
#include "safeapi/ipc/sapi_ipc.h"
#include "safeapi/task/sapi_task.h"
#include "safeapi/log/sapi_log.h"
#include "safeapi/nvm/sapi_nvm.h"
#include "safeapi/reboot/sapi_reboot.h"
#include "safeapi/memory/sapi_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers this file's POSIX backend with every OAL service in
 *        one call. Call once at process startup, before any other
 *        sapi_<service>_* function.
 * @return SAPI_STATUS_OK if every service registered successfully;
 *         the first non-OK status encountered otherwise (registration
 *         stops at the first failure - a partially-registered state is
 *         itself a startup fault, not something to paper over).
 */
sapi_status_t sapi_posix_backend_register_all(void);

/** @brief The POSIX sapi_timer backend (one pthread per timer). */
const sapi_timer_backend_t *sapi_posix_backend_timer(void);
/** @brief The POSIX sapi_ipc backend (pipe() per channel). */
const sapi_ipc_backend_t *sapi_posix_backend_ipc(void);
/** @brief The POSIX sapi_task backend (one pthread per task). */
const sapi_task_backend_t *sapi_posix_backend_task(void);
/** @brief The POSIX sapi_log backend (non-blocking write to stderr). */
const sapi_log_backend_t *sapi_posix_backend_log(void);
/** @brief The POSIX sapi_nvm backend (file-backed, FNV-1a-64 integrity). */
const sapi_nvm_backend_t *sapi_posix_backend_nvm(void);
/** @brief The POSIX sapi_reboot backend (re-exec via /proc/self/exe). */
const sapi_reboot_backend_t *sapi_posix_backend_reboot(void);
/** @brief The POSIX sapi_memory backend (static arena, no malloc). */
const sapi_mem_pool_backend_t *sapi_posix_backend_memory(void);

#ifdef __cplusplus
}
#endif

#endif /* SAPI_POSIX_BACKEND_H */
