/* Tests for sapi_cross_comparator (ADR-025): 2-way comparison between
 * registered sapi_channel_t links. */
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "safeapi/redundancy/cross_comparator/sapi_cross_comparator.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi/utils/safestate/sapi_safestate.h"

typedef struct
{
    uint8_t recv_buffer[8];
    sapi_status_t recv_status;
} mock_channel_t;

static mock_channel_t g_mock[2];

static sapi_status_t mock_send(void *channel_handle, const void *data, size_t data_size)
{
    (void)channel_handle;
    (void)data;
    (void)data_size;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_recv(void *channel_handle, void *data, size_t data_size, uint32_t timeout_ms)
{
    mock_channel_t *ch = (mock_channel_t *)channel_handle;
    (void)timeout_ms;
    if (ch->recv_status == SAPI_STATUS_OK)
    {
        memcpy(data, ch->recv_buffer, data_size);
    }
    return ch->recv_status;
}

static void reset_mocks(void)
{
    memset(&g_mock, 0, sizeof(g_mock));
    g_mock[0].recv_status = SAPI_STATUS_OK;
    g_mock[1].recv_status = SAPI_STATUS_OK;
}

static void init_channel(sapi_channel_storage_t *storage, uint32_t idx)
{
    sapi_channel_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.channel_handle = &g_mock[idx];
    cfg.send = mock_send;
    cfg.recv = mock_recv;
    assert(sapi_channel_init(storage, &cfg) == SAPI_STATUS_OK);
}

static jmp_buf g_jmp;
static int g_handler_calls;

static void diverting_handler(sapi_safestate_level_t level, sapi_safestate_reason_t reason,
                               const char *file, int32_t line, const char *message)
{
    (void)level;
    (void)reason;
    (void)file;
    (void)line;
    (void)message;
    g_handler_calls++;
    longjmp(g_jmp, 1);
}

static int g_disagreement_calls;
static sapi_voting_result_t g_last_result;

static void disagreement_callback(void *context, sapi_voting_result_t result)
{
    (void)context;
    g_disagreement_calls++;
    g_last_result = result;
}

static bool always_equal_compare(const void *a, const void *b, size_t size, void *ctx)
{
    (void)a;
    (void)b;
    (void)size;
    (void)ctx;
    return true;
}

static void test_init_and_register(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch[3];

    memset(&cfg, 0, sizeof(cfg));

    assert(sapi_cross_comparator_init(NULL, &cfg) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_init(&storage, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks();
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);

    assert(sapi_cross_comparator_register_channel(NULL, &ch[0]) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_register_channel(&storage, NULL) == SAPI_STATUS_INVALID_PARAM);

    {
        sapi_cross_comparator_storage_t not_init;
        memset(&not_init, 0, sizeof(not_init));
        assert(sapi_cross_comparator_register_channel(&not_init, &ch[0]) == SAPI_STATUS_NOT_INITIALIZED);
    }

    assert(sapi_cross_comparator_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);
    /* A 3rd registration is rejected. */
    assert(sapi_cross_comparator_register_channel(&storage, &ch[2]) == SAPI_STATUS_RESOURCE_EXHAUSTED);
}

static void test_execute_validation(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch[1];
    sapi_voting_result_t result;

    memset(&cfg, 0, sizeof(cfg));
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    assert(sapi_cross_comparator_execute(NULL, 4U, &result, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_execute(&storage, 0U, &result, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);
    /* Only 0 channels registered so far. */
    assert(sapi_cross_comparator_execute(&storage, 4U, &result, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    reset_mocks();
    init_channel(&ch[0], 0U);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    /* Only 1 of 2 required channels registered. */
    assert(sapi_cross_comparator_execute(&storage, 4U, &result, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    {
        uint8_t oversized_size_marker[SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE + 1U];
        (void)oversized_size_marker;
        assert(sapi_cross_comparator_execute(&storage, SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE + 1U, &result, NULL,
                                              NULL)
               == SAPI_STATUS_RESOURCE_EXHAUSTED);
    }
}

static void test_execute_agreement(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch[2];
    sapi_voting_result_t result;
    uint8_t out_data[4];
    size_t out_size = 0U;

    memset(&cfg, 0, sizeof(cfg));
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks();
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0x42, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x42, sizeof(g_mock[1].recv_buffer));

    assert(sapi_cross_comparator_execute(&storage, sizeof(out_data), &result, out_data, &out_size)
           == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);
    assert(out_size == sizeof(out_data));
    assert(out_data[0] == 0x42U);

    /* NULL out_data/out_size on the AGREED path is fine (caller just
     * wants to know the result). */
    assert(sapi_cross_comparator_execute(&storage, sizeof(out_data), &result, NULL, NULL) == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);

    {
        uint32_t healthy = 0U;
        uint32_t disagreements = 0U;
        assert(sapi_cross_comparator_get_aggregated_health(&storage, &healthy, &disagreements) == SAPI_STATUS_OK);
        assert(healthy == 2U);
        assert(disagreements == 0U);
    }
    assert(sapi_cross_comparator_get_aggregated_health(NULL, NULL, NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_execute_disagreement_triggers_safestate(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch[2];

    memset(&cfg, 0, sizeof(cfg));
    cfg.log_disagreements = true;
    cfg.safestate_level = SAPI_SAFESTATE_LEVEL_SAFE;
    cfg.safestate_reason = SAPI_SAFESTATE_REASON_UNSPECIFIED;
    cfg.on_disagreement = disagreement_callback;
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks();
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer));

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler) == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_cross_comparator_execute(&storage, 4U, NULL, NULL, NULL);
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
    }
}

/* Per the RCA/OCORA PI-API compatibility change (see sapi_cross_comparator.h's
 * own note), an application can no longer opt out of the safestate
 * transition on disagreement - the Platform always owns it. What an
 * application CAN still configure is which level/reason, and it is
 * guaranteed its own on_disagreement callback runs first (e.g. this is
 * where safeAPIRBC2oo2GP's own cleanup - flushing peer-negotiation
 * state, raising an alarm, closing its own links - happens before
 * control does not return). This test exercises a REBOOT-level
 * configuration (as safeAPIRBC2oo2GP itself uses, rather than the
 * SAFE-level default the previous test exercises) and verifies the
 * callback-then-transition ordering. */
static void test_execute_disagreement_triggers_reboot_level(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch[2];

    memset(&cfg, 0, sizeof(cfg));
    cfg.safestate_level = SAPI_SAFESTATE_LEVEL_REBOOT;
    cfg.safestate_reason = (sapi_safestate_reason_t)(SAPI_SAFESTATE_REASON_APPLICATION_BASE + 1U);
    cfg.on_disagreement = disagreement_callback;
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks();
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0xAA, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0xBB, sizeof(g_mock[1].recv_buffer));

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_REBOOT, diverting_handler) == SAPI_STATUS_OK);

    g_disagreement_calls = 0;
    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_cross_comparator_execute(&storage, 4U, NULL, NULL, NULL);
        assert(0);
    }
    else
    {
        /* on_disagreement ran BEFORE the transition. */
        assert(g_disagreement_calls == 1);
        assert(g_last_result == SAPI_VOTING_DISAGREED);
        assert(g_handler_calls == 1);
    }

    {
        uint32_t disagreements = 0U;
        assert(sapi_cross_comparator_get_aggregated_health(&storage, NULL, &disagreements) == SAPI_STATUS_OK);
        assert(disagreements == 1U);
    }
}

static void test_execute_timeout_and_unhealthy(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch[2];
    sapi_voting_result_t result;
    size_t out_size = 123U;

    memset(&cfg, 0, sizeof(cfg));
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks();
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    /* One channel marked unhealthy: INSUFFICIENT_QUORUM without any I/O. */
    assert(sapi_channel_set_healthy(&ch[1], false) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_execute(&storage, 4U, &result, NULL, &out_size) == SAPI_STATUS_HARDWARE_FAULT);
    assert(result == SAPI_VOTING_INSUFFICIENT_QUORUM);
    assert(out_size == 0U);
    assert(sapi_channel_set_healthy(&ch[1], true) == SAPI_STATUS_OK);

    /* One channel times out. */
    g_mock[0].recv_status = SAPI_STATUS_TIMEOUT;
    assert(sapi_cross_comparator_execute(&storage, 4U, &result, NULL, NULL) == SAPI_STATUS_HARDWARE_FAULT);
    assert(result == SAPI_VOTING_TIMEOUT);
    g_mock[0].recv_status = SAPI_STATUS_OK;

    /* One channel fails with a non-timeout error. */
    g_mock[0].recv_status = SAPI_STATUS_HARDWARE_FAULT;
    assert(sapi_cross_comparator_execute(&storage, 4U, &result, NULL, NULL) == SAPI_STATUS_HARDWARE_FAULT);
    assert(result == SAPI_VOTING_INSUFFICIENT_QUORUM);
}

static void test_custom_compare_fn(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch[2];
    sapi_voting_result_t result;

    memset(&cfg, 0, sizeof(cfg));
    cfg.compare = always_equal_compare;
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks();
    init_channel(&ch[0], 0U);
    init_channel(&ch[1], 1U);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[0]) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_register_channel(&storage, &ch[1]) == SAPI_STATUS_OK);

    memset(g_mock[0].recv_buffer, 0x11, sizeof(g_mock[0].recv_buffer));
    memset(g_mock[1].recv_buffer, 0x99, sizeof(g_mock[1].recv_buffer));

    assert(sapi_cross_comparator_execute(&storage, 4U, &result, NULL, NULL) == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);
}

/* sapi_cross_comparator_execute_buffers() (RCA/OCORA Phase 2b - see
 * sapi_cross_comparator.h's own doc): no channel registration needed at
 * all, just two already-in-hand buffers. */
static void test_execute_buffers_agreement(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    uint8_t local_buf[4] = {0x42U, 0x42U, 0x42U, 0x42U};
    uint8_t peer_buf[4] = {0x42U, 0x42U, 0x42U, 0x42U};
    sapi_voting_result_t result;
    uint8_t out_data[4];
    size_t out_size = 0U;

    memset(&cfg, 0, sizeof(cfg));
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    /* Deliberately no sapi_cross_comparator_register_channel() call. */
    assert(sapi_cross_comparator_execute_buffers(&storage, local_buf, peer_buf, sizeof(local_buf), &result,
                                                  out_data, &out_size) == SAPI_STATUS_OK);
    assert(result == SAPI_VOTING_AGREED);
    assert(out_size == sizeof(local_buf));
    assert(memcmp(out_data, local_buf, sizeof(local_buf)) == 0);
}

static void test_execute_buffers_disagreement_triggers_safestate(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    uint8_t local_buf[4] = {0xAAU, 0xAAU, 0xAAU, 0xAAU};
    uint8_t peer_buf[4] = {0xBBU, 0xBBU, 0xBBU, 0xBBU};

    memset(&cfg, 0, sizeof(cfg));
    cfg.log_disagreements = true;
    cfg.safestate_level = SAPI_SAFESTATE_LEVEL_SAFE;
    cfg.safestate_reason = SAPI_SAFESTATE_REASON_UNSPECIFIED;
    cfg.on_disagreement = disagreement_callback;
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler) == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_cross_comparator_execute_buffers(&storage, local_buf, peer_buf, sizeof(local_buf), NULL, NULL,
                                                     NULL);
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
    }
}

static void test_execute_buffers_validation(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    uint8_t buf[4] = {0};
    uint8_t oversized[SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE + 1U];

    memset(&cfg, 0, sizeof(cfg));
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    assert(sapi_cross_comparator_execute_buffers(NULL, buf, buf, sizeof(buf), NULL, NULL, NULL) ==
           SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_execute_buffers(&storage, NULL, buf, sizeof(buf), NULL, NULL, NULL) ==
           SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_execute_buffers(&storage, buf, NULL, sizeof(buf), NULL, NULL, NULL) ==
           SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_execute_buffers(&storage, buf, buf, 0U, NULL, NULL, NULL) ==
           SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cross_comparator_execute_buffers(&storage, oversized, oversized, sizeof(oversized), NULL, NULL,
                                                  NULL) == SAPI_STATUS_RESOURCE_EXHAUSTED);

    {
        sapi_cross_comparator_storage_t uninit;
        memset(&uninit, 0, sizeof(uninit));
        assert(sapi_cross_comparator_execute_buffers(&uninit, buf, buf, sizeof(buf), NULL, NULL, NULL) ==
               SAPI_STATUS_NOT_INITIALIZED);
    }
}

/* REQ-LIFECYCLE-001 (ADR-026): once the application's setup phase is
 * locked, sapi_cross_comparator_init()/_register_channel() refuse even
 * with fully-valid arguments. */
static void test_lifecycle_lock(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;
    sapi_channel_storage_t ch;

    memset(&cfg, 0, sizeof(cfg));
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    reset_mocks();
    init_channel(&ch, 0U);

    sapi_lifecycle_lock();
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_INVALID_STATE);
    assert(sapi_cross_comparator_register_channel(&storage, &ch) == SAPI_STATUS_INVALID_STATE);
    sapi_lifecycle_unlock();
    assert(sapi_cross_comparator_register_channel(&storage, &ch) == SAPI_STATUS_OK);
}

static void test_destroy(void)
{
    sapi_cross_comparator_storage_t storage;
    sapi_cross_comparator_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    assert(sapi_cross_comparator_init(&storage, &cfg) == SAPI_STATUS_OK);

    assert(sapi_cross_comparator_destroy(NULL) == SAPI_STATUS_OK);
    assert(sapi_cross_comparator_destroy(&storage) == SAPI_STATUS_OK);
}

int main(void)
{
    test_init_and_register();
    test_execute_validation();
    test_execute_agreement();
    test_execute_disagreement_triggers_safestate();
    test_execute_disagreement_triggers_reboot_level();
    test_execute_timeout_and_unhealthy();
    test_custom_compare_fn();
    test_execute_buffers_agreement();
    test_execute_buffers_disagreement_triggers_safestate();
    test_execute_buffers_validation();
    test_lifecycle_lock();
    test_destroy();
    return 0;
}
