/* REQ-LIFECYCLE-003 and the OSA rule "resources only during initialisation": once the setup phase is locked,
 * every setup-only OAL entry point refuses with RTE_STATUS_INVALID_STATE and reports the violation through
 * rte_safestate_enter(DEGRADED, RTE_SAFESTATE_REASON_SETUP_AFTER_INIT). The gate comes before the OSAdapter
 * check, so no OSAdapter is registered here. */
#include <assert.h>
#include <string.h>

#include "rte/oal/ipc/rte_ipc.h"
#include "rte/oal/memory/rte_memory.h"
#include "rte/oal/mutex/rte_mutex.h"
#include "rte/oal/nvm/rte_nvm.h"
#include "rte/oal/task/rte_task.h"
#include "rte/oal/timer/rte_timer.h"
#include "rte/redundancy/channel_service/rte_channel_service.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte/utils/safestate/rte_safestate.h"

static int s_reports;
static rte_safestate_reason_t s_last_reason;

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

    rte_lifecycle_unlock();
    return 0;
}
