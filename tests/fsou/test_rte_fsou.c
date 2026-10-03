/**
 * @file test_rte_fsou.c
 * @brief Tests for the optional FSOU output-permission seam (ADR-041, REQ-FSOU-001..003).
 *
 * The module keeps process-wide state (the registered OSAdapter and the presence latch), so the cases run in a fixed
 * order: everything that needs "nothing registered" comes first. The "FSOU present" report is covered by
 * test_rte_fsou_present.c, a separate process.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rte/oal/fsou/rte_fsou.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte/utils/safestate/rte_safestate.h"
#include "rte_osadapter/log/rte_osadapter_log.h"
#include "rte_osadapter/rte_osadapter.h"

/* Stub FSOU: counts calls, records the last argument, returns a settable status. */
static int s_grant_calls;
static int s_revoke_calls;
static uint32_t s_last_cycle;
static rte_safestate_reason_t s_last_reason;
static rte_status_t s_stub_status = RTE_STATUS_OK;

static rte_status_t stub_grant(uint32_t cycle_id)
{
    s_grant_calls++;
    s_last_cycle = cycle_id;
    return s_stub_status;
}

static rte_status_t stub_revoke(rte_safestate_reason_t reason)
{
    s_revoke_calls++;
    s_last_reason = reason;
    return s_stub_status;
}

static const rte_osadapter_fsou_t s_full = { stub_grant, stub_revoke };
static const rte_osadapter_fsou_t s_grant_only = { stub_grant, NULL };
static const rte_osadapter_fsou_t s_revoke_only = { NULL, stub_revoke };

/* Log capture for REQ-FSOU-003. */
static int s_log_lines;
static char s_last_log[64];

static rte_status_t log_init(void)
{
    return RTE_STATUS_OK;
}

static void log_write(rte_log_level_t level, const char *tag, const char *message)
{
    assert(level == RTE_LOG_LEVEL_INFO);
    assert((tag != NULL) && (strcmp(tag, "FSOU") == 0));
    s_log_lines++;
    (void)snprintf(s_last_log, sizeof(s_last_log), "%s", (message != NULL) ? message : "");
}

static const rte_osadapter_log_t s_log = { log_init, log_write };

/* The setup-lock rejection is reported at DEGRADED (REQ-LIFECYCLE-003), which returns. */
static int s_degraded_reports;

static void degraded_handler(rte_safestate_level_t level, rte_safestate_reason_t reason, const char *file,
                             int32_t line, const char *message)
{
    (void)reason;
    (void)file;
    (void)line;
    (void)message;
    assert(level == RTE_SAFESTATE_LEVEL_DEGRADED);
    s_degraded_reports++;
}

static void reset_stub(void)
{
    s_grant_calls = 0;
    s_revoke_calls = 0;
    s_last_cycle = 0U;
    s_last_reason = RTE_SAFESTATE_REASON_UNSPECIFIED;
    s_stub_status = RTE_STATUS_OK;
}

/* REQ-FSOU-002: nothing registered. */
static void test_unregistered(void)
{
    printf("test_unregistered...\n");
    assert(!rte_fsou_is_present());
    assert(rte_fsou_grant(1U) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_fsou_revoke(RTE_SAFESTATE_REASON_CHECKPOINT_TIMEOUT) == RTE_STATUS_NOT_INITIALIZED);
}

/* REQ-FSOU-001: NULL refused, nothing registered by it. */
static void test_register_null(void)
{
    printf("test_register_null...\n");
    assert(rte_osadapter_fsou_register(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(!rte_fsou_is_present());
}

/* A bundle with fsou == NULL leaves the FSOU unregistered. */
static void test_bundle_without_fsou(void)
{
    rte_osadapter_bundle_t bundle;

    printf("test_bundle_without_fsou...\n");
    (void)memset(&bundle, 0, sizeof(bundle));
    bundle.log = &s_log;
    assert(rte_osadapter_register_all(&bundle) == RTE_STATUS_OK);
    assert(!rte_fsou_is_present());
    assert(rte_fsou_grant(2U) == RTE_STATUS_NOT_INITIALIZED);
}

/* REQ-FSOU-003: absent case logged once, a second call logs nothing. */
static void test_report_absent_once(void)
{
    printf("test_report_absent_once...\n");
    s_log_lines = 0;
    rte_fsou_report_presence();
    assert(s_log_lines == 1);
    assert(strcmp(s_last_log, "no FSOU registered") == 0);
    rte_fsou_report_presence();
    assert(s_log_lines == 1);
}

/* REQ-FSOU-001: setup-only - refused after the setup lock, earlier state kept. */
static void test_register_after_setup_lock(void)
{
    rte_osadapter_bundle_t bundle;

    printf("test_register_after_setup_lock...\n");
    assert(rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_DEGRADED, degraded_handler) == RTE_STATUS_OK);
    s_degraded_reports = 0;
    rte_lifecycle_lock();
    assert(rte_osadapter_fsou_register(&s_full) == RTE_STATUS_INVALID_STATE);
    assert(!rte_fsou_is_present());

    (void)memset(&bundle, 0, sizeof(bundle));
    bundle.fsou = &s_full;
    assert(rte_osadapter_register_all(&bundle) == RTE_STATUS_INVALID_STATE);
    assert(!rte_fsou_is_present());
    assert(s_degraded_reports == 2);
    rte_lifecycle_unlock();
}

/* Bundle with fsou registers it; REQ-FSOU-002 registered case passes arguments and status through. */
static void test_bundle_with_fsou(void)
{
    rte_osadapter_bundle_t bundle;

    printf("test_bundle_with_fsou...\n");
    (void)memset(&bundle, 0, sizeof(bundle));
    bundle.fsou = &s_full;
    assert(rte_osadapter_register_all(&bundle) == RTE_STATUS_OK);
    assert(rte_fsou_is_present());

    reset_stub();
    assert(rte_fsou_grant(42U) == RTE_STATUS_OK);
    assert((s_grant_calls == 1) && (s_last_cycle == 42U) && (s_revoke_calls == 0));
    assert(rte_fsou_revoke(RTE_SAFESTATE_REASON_CHECKPOINT_TIMEOUT) == RTE_STATUS_OK);
    assert((s_revoke_calls == 1) && (s_last_reason == RTE_SAFESTATE_REASON_CHECKPOINT_TIMEOUT));
    assert(s_grant_calls == 1);

    s_stub_status = RTE_STATUS_HARDWARE_FAULT;
    assert(rte_fsou_grant(43U) == RTE_STATUS_HARDWARE_FAULT);
    assert((s_grant_calls == 2) && (s_last_cycle == 43U));
    assert(rte_fsou_revoke(RTE_SAFESTATE_REASON_ASSERT_FAILED) == RTE_STATUS_HARDWARE_FAULT);
    assert((s_revoke_calls == 2) && (s_last_reason == RTE_SAFESTATE_REASON_ASSERT_FAILED));

    /* A later bundle without fsou does not unregister it. */
    (void)memset(&bundle, 0, sizeof(bundle));
    assert(rte_osadapter_register_all(&bundle) == RTE_STATUS_OK);
    assert(rte_fsou_is_present());
}

/* REQ-FSOU-002: a NULL slot is NOT_SUPPORTED, the other slot still dispatches. */
static void test_partial_adapters(void)
{
    printf("test_partial_adapters...\n");
    assert(rte_osadapter_fsou_register(&s_grant_only) == RTE_STATUS_OK);
    reset_stub();
    assert(rte_fsou_revoke(RTE_SAFESTATE_REASON_UNSPECIFIED) == RTE_STATUS_NOT_SUPPORTED);
    assert(s_revoke_calls == 0);
    assert(rte_fsou_grant(7U) == RTE_STATUS_OK);
    assert((s_grant_calls == 1) && (s_last_cycle == 7U));

    assert(rte_osadapter_fsou_register(&s_revoke_only) == RTE_STATUS_OK);
    reset_stub();
    assert(rte_fsou_grant(8U) == RTE_STATUS_NOT_SUPPORTED);
    assert(s_grant_calls == 0);
    assert(rte_fsou_revoke(RTE_SAFESTATE_REASON_SETUP_AFTER_INIT) == RTE_STATUS_OK);
    assert((s_revoke_calls == 1) && (s_last_reason == RTE_SAFESTATE_REASON_SETUP_AFTER_INIT));
}

/* REQ-FSOU-003: the latch holds after registration too - still no second line. */
static void test_report_latched_after_registration(void)
{
    printf("test_report_latched_after_registration...\n");
    s_log_lines = 0;
    rte_fsou_report_presence();
    assert(s_log_lines == 0);
}

int main(void)
{
    printf("Running test_rte_fsou...\n");
    test_unregistered();
    test_register_null();
    test_bundle_without_fsou();
    test_report_absent_once();
    test_register_after_setup_lock();
    test_bundle_with_fsou();
    test_partial_adapters();
    test_report_latched_after_registration();
    printf("All test_rte_fsou tests passed!\n");
    return 0;
}
