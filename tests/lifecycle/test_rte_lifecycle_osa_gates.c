/* REQ-LIFECYCLE-003 and the OSA rule "resources only during initialisation": once the setup phase is locked,
 * every setup-only OAL entry point refuses with RTE_STATUS_INVALID_STATE and reports the violation through
 * rte_safestate_enter(DEGRADED, RTE_SAFESTATE_REASON_SETUP_AFTER_INIT). The gate comes before the OSAdapter
 * check, so the gated calls never reach an OSAdapter (the unlocked calls run before any is registered).
 *
 * REQ-LIFECYCLE-001 for the 13 OAL OSAdapter registration functions (rte_osadapter_fsou_register() is covered by
 * tests/fsou/test_rte_fsou.c): a "previous" OSAdapter is registered for each module just before the lock; after
 * the lock a re-registration with a "refused" OSAdapter returns RTE_STATUS_INVALID_STATE, reports once, and keeps
 * the previous one - a non-gated call of the module still dispatches to the previous OSAdapter, never the refused
 * one. */
#include <assert.h>
#include <string.h>

#include "rte/oal/clocksync/rte_clocksync.h"
#include "rte/oal/flow/rte_flow.h"
#include "rte/oal/ipc/rte_ipc.h"
#include "rte/oal/log/rte_log.h"
#include "rte/oal/memory/rte_memory.h"
#include "rte/oal/mutex/rte_mutex.h"
#include "rte/oal/netlink/rte_netlink.h"
#include "rte/oal/nvm/rte_nvm.h"
#include "rte/oal/platform/rte_platform.h"
#include "rte/oal/random/rte_random.h"
#include "rte/oal/reboot/rte_reboot.h"
#include "rte/oal/task/rte_task.h"
#include "rte/oal/timer/rte_timer.h"
#include "rte/redundancy/channel_service/rte_channel_service.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte/utils/safestate/rte_safestate.h"
#include "rte_osadapter/clocksync/rte_osadapter_clocksync.h"
#include "rte_osadapter/flow/rte_osadapter_flow.h"
#include "rte_osadapter/ipc/rte_osadapter_ipc.h"
#include "rte_osadapter/log/rte_osadapter_log.h"
#include "rte_osadapter/memory/rte_osadapter_memory.h"
#include "rte_osadapter/mutex/rte_osadapter_mutex.h"
#include "rte_osadapter/netlink/rte_osadapter_netlink.h"
#include "rte_osadapter/nvm/rte_osadapter_nvm.h"
#include "rte_osadapter/platform/rte_osadapter_platform.h"
#include "rte_osadapter/random/rte_osadapter_random.h"
#include "rte_osadapter/reboot/rte_osadapter_reboot.h"
#include "rte_osadapter/task/rte_osadapter_task.h"
#include "rte_osadapter/timer/rte_osadapter_timer.h"

static int s_reports;
static rte_safestate_reason_t s_last_reason;

/* Calls into the "previous" vtables (registered before the lock) and into the "refused" ones (offered after it).
 * Each vtable fills only the slot its module's probe dispatches to (every other slot NULL). A previous stub returns
 * RTE_STATUS_OK, a refused stub RTE_STATUS_HARDWARE_FAULT: after the refused registrations each probe must reach the
 * previous stub - a replaced OSAdapter would return HARDWARE_FAULT, a cleared one NOT_INITIALIZED. */
static int s_previous_osa_calls;
static int s_refused_osa_calls;

static rte_status_t previous_clocksync_get_offset_ms(int64_t *out_offset_ms)
{
    (void)out_offset_ms;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_flow_close(rte_flow_handle_t handle)
{
    (void)handle;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_ipc_destroy(rte_ipc_handle_t handle)
{
    (void)handle;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static void previous_log_write(rte_log_level_t level, const char *tag, const char *message)
{
    (void)level;
    (void)tag;
    (void)message;
    s_previous_osa_calls++;
}

static rte_status_t previous_mem_pool_acquire(rte_mem_pool_handle_t handle, void **out_block)
{
    (void)handle;
    (void)out_block;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_mutex_unlock(rte_mutex_handle_t handle)
{
    (void)handle;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_netlink_close(rte_netlink_handle_t handle)
{
    (void)handle;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_nvm_sync(rte_nvm_handle_t handle)
{
    (void)handle;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_platform_realtime_init(uint32_t rt_priority)
{
    (void)rt_priority;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_random_fill(uint8_t *out, size_t len)
{
    (void)out;
    (void)len;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_reboot_request(uint16_t reason_code)
{
    (void)reason_code;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_task_suspend(rte_task_handle_t handle)
{
    (void)handle;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t previous_timer_now(rte_timestamp_ms_t *out_now_ms)
{
    (void)out_now_ms;
    s_previous_osa_calls++;
    return RTE_STATUS_OK;
}

static rte_status_t refused_clocksync_get_offset_ms(int64_t *out_offset_ms)
{
    (void)out_offset_ms;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_flow_close(rte_flow_handle_t handle)
{
    (void)handle;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_ipc_destroy(rte_ipc_handle_t handle)
{
    (void)handle;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static void refused_log_write(rte_log_level_t level, const char *tag, const char *message)
{
    (void)level;
    (void)tag;
    (void)message;
    s_refused_osa_calls++;
}

static rte_status_t refused_mem_pool_acquire(rte_mem_pool_handle_t handle, void **out_block)
{
    (void)handle;
    (void)out_block;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_mutex_unlock(rte_mutex_handle_t handle)
{
    (void)handle;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_netlink_close(rte_netlink_handle_t handle)
{
    (void)handle;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_nvm_sync(rte_nvm_handle_t handle)
{
    (void)handle;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_platform_realtime_init(uint32_t rt_priority)
{
    (void)rt_priority;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_random_fill(uint8_t *out, size_t len)
{
    (void)out;
    (void)len;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_reboot_request(uint16_t reason_code)
{
    (void)reason_code;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_task_suspend(rte_task_handle_t handle)
{
    (void)handle;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static rte_status_t refused_timer_now(rte_timestamp_ms_t *out_now_ms)
{
    (void)out_now_ms;
    s_refused_osa_calls++;
    return RTE_STATUS_HARDWARE_FAULT;
}

static const rte_osadapter_clocksync_t s_previous_clocksync = { previous_clocksync_get_offset_ms, NULL };
static const rte_osadapter_flow_t s_previous_flow = { NULL, NULL, NULL, previous_flow_close, NULL, NULL };
static const rte_osadapter_ipc_t s_previous_ipc = { NULL, NULL, NULL, previous_ipc_destroy };
static const rte_osadapter_log_t s_previous_log = { NULL, previous_log_write };
static const rte_osadapter_memory_t s_previous_memory = { NULL, previous_mem_pool_acquire, NULL, NULL };
static const rte_osadapter_mutex_t s_previous_mutex = { NULL, NULL, previous_mutex_unlock, NULL, NULL };
static const rte_osadapter_netlink_t s_previous_netlink = { NULL, NULL, NULL, previous_netlink_close };
static const rte_osadapter_nvm_t s_previous_nvm = { NULL, NULL, NULL, previous_nvm_sync, NULL };
static const rte_osadapter_platform_t s_previous_platform = { previous_platform_realtime_init };
static const rte_osadapter_random_t s_previous_random = { previous_random_fill };
static const rte_osadapter_reboot_t s_previous_reboot = { previous_reboot_request };
static const rte_osadapter_task_t s_previous_task = { NULL, NULL, previous_task_suspend, NULL };
static const rte_osadapter_timer_t s_previous_timer = { NULL, NULL, NULL, NULL, previous_timer_now };

static const rte_osadapter_clocksync_t s_refused_clocksync = { refused_clocksync_get_offset_ms, NULL };
static const rte_osadapter_flow_t s_refused_flow = { NULL, NULL, NULL, refused_flow_close, NULL, NULL };
static const rte_osadapter_ipc_t s_refused_ipc = { NULL, NULL, NULL, refused_ipc_destroy };
static const rte_osadapter_log_t s_refused_log = { NULL, refused_log_write };
static const rte_osadapter_memory_t s_refused_memory = { NULL, refused_mem_pool_acquire, NULL, NULL };
static const rte_osadapter_mutex_t s_refused_mutex = { NULL, NULL, refused_mutex_unlock, NULL, NULL };
static const rte_osadapter_netlink_t s_refused_netlink = { NULL, NULL, NULL, refused_netlink_close };
static const rte_osadapter_nvm_t s_refused_nvm = { NULL, NULL, NULL, refused_nvm_sync, NULL };
static const rte_osadapter_platform_t s_refused_platform = { refused_platform_realtime_init };
static const rte_osadapter_random_t s_refused_random = { refused_random_fill };
static const rte_osadapter_reboot_t s_refused_reboot = { refused_reboot_request };
static const rte_osadapter_task_t s_refused_task = { NULL, NULL, refused_task_suspend, NULL };
static const rte_osadapter_timer_t s_refused_timer = { NULL, NULL, NULL, NULL, refused_timer_now };

/* One refused registration: RTE_STATUS_INVALID_STATE, and its SETUP_AFTER_INIT report brings the running count to
 * expected_reports (one more than before the call). */
static void expect_refused(rte_status_t status, int expected_reports)
{
    assert(status == RTE_STATUS_INVALID_STATE);
    assert(s_reports == expected_reports);
    assert(s_last_reason == RTE_SAFESTATE_REASON_SETUP_AFTER_INIT);
}

static void degraded_handler(rte_safestate_level_t level, rte_safestate_reason_t reason, const char *file,
                             int32_t line, const char *message)
{
    (void)file;
    (void)line;
    (void)message;
    assert(level == RTE_SAFESTATE_LEVEL_DEGRADED);
    s_reports++;
    s_last_reason = reason;
}

static void task_entry(void *arg)
{
    (void)arg;
}

int main(void)
{
    rte_task_storage_t task_storage;
    rte_task_config_t task_config;
    rte_task_handle_t task_handle = NULL;
    rte_ipc_storage_t ipc_storage;
    rte_ipc_config_t ipc_config;
    rte_ipc_handle_t ipc_handle = NULL;
    rte_nvm_storage_t nvm_storage;
    rte_nvm_config_t nvm_config;
    rte_nvm_handle_t nvm_handle = NULL;
    rte_channel_service_t channel;
    rte_mutex_storage_t mutex_storage;
    rte_mutex_handle_t mutex_handle = NULL;
    rte_mem_pool_storage_t pool_storage;
    rte_mem_pool_config_t pool_config;
    rte_mem_pool_handle_t pool_handle = NULL;
    int64_t offset_ms = 0;
    void *block = NULL;
    uint8_t random_bytes[8];
    rte_timestamp_ms_t now_ms = 0U;
    int dummy = 0;

    assert(rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_DEGRADED, degraded_handler) == RTE_STATUS_OK);

    (void)memset(&task_config, 0, sizeof(task_config));
    task_config.entry = task_entry;
    (void)memset(&ipc_config, 0, sizeof(ipc_config));
    ipc_config.name = "gate";
    ipc_config.message_size = 8U;
    ipc_config.queue_depth = 1U;
    (void)memset(&nvm_config, 0, sizeof(nvm_config));
    nvm_config.region_name = "gate";
    nvm_config.region_size = 8U;
    /* Non-zero sizes: a zero size is refused with RTE_STATUS_INVALID_PARAM before the gate is reached. */
    (void)memset(&pool_config, 0, sizeof(pool_config));
    pool_config.block_size = 8U;
    pool_config.block_count = 1U;

    /* Unlocked: the gate lets the call through (it then fails on the missing OSAdapter, not on the lock), no report. */
    assert(rte_task_create(&task_storage, &task_config, &task_handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_mutex_create(&mutex_storage, &mutex_handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_mem_pool_create(&pool_storage, &pool_config, &pool_handle) == RTE_STATUS_NOT_INITIALIZED);
    assert(s_reports == 0);

    /* REQ-LIFECYCLE-001: the previous OSAdapter of each of the 13 modules, registered while still unlocked (no
     * report). None of the gated calls below reaches it: each refuses at the gate, before its OSAdapter check. */
    assert(rte_osadapter_clocksync_register(&s_previous_clocksync) == RTE_STATUS_OK);
    assert(rte_osadapter_flow_register(&s_previous_flow) == RTE_STATUS_OK);
    assert(rte_osadapter_ipc_register(&s_previous_ipc) == RTE_STATUS_OK);
    assert(rte_osadapter_log_register(&s_previous_log) == RTE_STATUS_OK);
    assert(rte_osadapter_memory_register(&s_previous_memory) == RTE_STATUS_OK);
    assert(rte_osadapter_mutex_register(&s_previous_mutex) == RTE_STATUS_OK);
    assert(rte_osadapter_netlink_register(&s_previous_netlink) == RTE_STATUS_OK);
    assert(rte_osadapter_nvm_register(&s_previous_nvm) == RTE_STATUS_OK);
    assert(rte_osadapter_platform_register(&s_previous_platform) == RTE_STATUS_OK);
    assert(rte_osadapter_random_register(&s_previous_random) == RTE_STATUS_OK);
    assert(rte_osadapter_reboot_register(&s_previous_reboot) == RTE_STATUS_OK);
    assert(rte_osadapter_task_register(&s_previous_task) == RTE_STATUS_OK);
    assert(rte_osadapter_timer_register(&s_previous_timer) == RTE_STATUS_OK);
    assert(s_reports == 0);

    rte_lifecycle_lock();

    assert(rte_task_create(&task_storage, &task_config, &task_handle) == RTE_STATUS_INVALID_STATE);
    assert(rte_task_start((rte_task_handle_t)(void *)&dummy) == RTE_STATUS_INVALID_STATE);
    assert(rte_timer_start((rte_timer_handle_t)(void *)&dummy) == RTE_STATUS_INVALID_STATE);
    assert(rte_ipc_create(&ipc_storage, &ipc_config, &ipc_handle) == RTE_STATUS_INVALID_STATE);
    assert(rte_nvm_open(&nvm_storage, &nvm_config, &nvm_handle) == RTE_STATUS_INVALID_STATE);
    assert(rte_channel_service_setup(&channel, "gate") == RTE_STATUS_INVALID_STATE);
    assert(s_reports == 6);

    /* REQ-LIFECYCLE-001/-003: the mutex and memory-pool constructors. The handles are pre-set to a non-NULL value
     * so the check that a refused constructor leaves its out handle NULL is not satisfied by the initialiser; each
     * refusal must add exactly one report. */
    mutex_handle = (rte_mutex_handle_t)(void *)&dummy;
    assert(rte_mutex_create(&mutex_storage, &mutex_handle) == RTE_STATUS_INVALID_STATE);
    assert(mutex_handle == NULL);
    assert(s_reports == 7);
    pool_handle = (rte_mem_pool_handle_t)(void *)&dummy;
    assert(rte_mem_pool_create(&pool_storage, &pool_config, &pool_handle) == RTE_STATUS_INVALID_STATE);
    assert(pool_handle == NULL);
    assert(s_reports == 8);
    assert(s_last_reason == RTE_SAFESTATE_REASON_SETUP_AFTER_INIT);

    /* REQ-LIFECYCLE-001/-003: the 13 OAL OSAdapter registrations, still locked. A NULL adapter is refused with
     * RTE_STATUS_INVALID_PARAM before the gate, so each gets a non-NULL vtable; each refusal adds exactly one report. */
    expect_refused(rte_osadapter_clocksync_register(&s_refused_clocksync), 9);
    expect_refused(rte_osadapter_flow_register(&s_refused_flow), 10);
    expect_refused(rte_osadapter_ipc_register(&s_refused_ipc), 11);
    expect_refused(rte_osadapter_log_register(&s_refused_log), 12);
    expect_refused(rte_osadapter_memory_register(&s_refused_memory), 13);
    expect_refused(rte_osadapter_mutex_register(&s_refused_mutex), 14);
    expect_refused(rte_osadapter_netlink_register(&s_refused_netlink), 15);
    expect_refused(rte_osadapter_nvm_register(&s_refused_nvm), 16);
    expect_refused(rte_osadapter_platform_register(&s_refused_platform), 17);
    expect_refused(rte_osadapter_random_register(&s_refused_random), 18);
    expect_refused(rte_osadapter_reboot_register(&s_refused_reboot), 19);
    expect_refused(rte_osadapter_task_register(&s_refused_task), 20);
    expect_refused(rte_osadapter_timer_register(&s_refused_timer), 21);

    /* REQ-LIFECYCLE-001: each refused registration kept the previous OSAdapter. Every probe below is not
     * lifecycle-gated and gets valid arguments, so it passes its parameter checks and dispatches to the registered
     * OSAdapter: the previous stub (RTE_STATUS_OK, one more previous call each), never the refused one. rte_log has
     * no status to probe, so its probe is the counters alone. */
    assert(rte_clocksync_get_offset_ms(&offset_ms) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 1);
    assert(rte_flow_close((rte_flow_handle_t)(void *)&dummy) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 2);
    assert(rte_ipc_destroy((rte_ipc_handle_t)(void *)&dummy) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 3);
    rte_log_write(RTE_LOG_LEVEL_ERROR, "gate", "probe");
    assert(s_previous_osa_calls == 4);
    assert(rte_mem_pool_acquire((rte_mem_pool_handle_t)(void *)&dummy, &block) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 5);
    assert(rte_mutex_unlock((rte_mutex_handle_t)(void *)&dummy) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 6);
    assert(rte_netlink_close((rte_netlink_handle_t)(void *)&dummy) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 7);
    assert(rte_nvm_sync((rte_nvm_handle_t)(void *)&dummy) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 8);
    assert(rte_platform_realtime_init(1U) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 9);
    assert(rte_random_fill(random_bytes, sizeof(random_bytes)) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 10);
    assert(rte_reboot_request(0U) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 11);
    assert(rte_task_suspend((rte_task_handle_t)(void *)&dummy) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 12);
    assert(rte_timer_now(&now_ms) == RTE_STATUS_OK);
    assert(s_previous_osa_calls == 13);
    assert(s_refused_osa_calls == 0);
    /* The probes are not setup-only calls, so they add no report. */
    assert(s_reports == 21);

    rte_lifecycle_unlock();
    return 0;
}
