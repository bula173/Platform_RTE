/* Tests for sapi_checksum.c (CRC-64 computation + the "vital message"
 * envelope format) - REQ-CHECKSUM-001..008 in sapi_checksum.h.
 *
 * sapi_checksum_crc64_init() may be called exactly once per process
 * (REQ-CHECKSUM-001: g_checksum_manager is a static, process-lifetime
 * singleton with no de-init/reset entry point). To exercise every branch
 * of its polynomial switch (ERTMS/ISO/XZ) as well as the "not yet
 * initialized" branches of sapi_checksum_crc64() from a single test
 * binary, this file forks a fresh child process per polynomial: each
 * child inherits a copy-on-write, not-yet-initialized copy of the
 * module's static state, so it can independently observe the
 * pre-init behavior and then initialize with its own polynomial without
 * disturbing the parent's (or any sibling's) module state. This is a
 * test-only technique (not production code, not subject to this
 * project's no-recursion/MISRA constraints) - see file-level rationale;
 * gcov coverage counters are accumulated across parent + children because
 * each exits via exit() (normal libc/gcov atexit flush), not _exit().
 *
 * Style follows tests/status/test_sapi_status.c / tests/log/test_sapi_log.c:
 * plain assert()-based main(), no framework, no dynamic memory.
 */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "safeapi/redundancy/checksum/sapi_checksum.h"

/* Runs in a forked child: verifies pre-init behavior of
 * sapi_checksum_crc64() (REQ-CHECKSUM-002), then initializes with
 * `polynomial` and checks the resulting table/polynomial actually work,
 * exercising one arm of sapi_checksum_crc64_init()'s switch per call. */
static void child_test_polynomial(sapi_crc64_polynomial_t polynomial)
{
    const uint8_t data[4] = { 0x01U, 0x02U, 0x03U, 0x04U };
    sapi_crc64_t crc_before;
    sapi_crc64_t crc_a;
    sapi_crc64_t crc_b;

    /* Pre-init: crc64() must return 0 without dereferencing data, and
     * without crashing (REQ-CHECKSUM-002). */
    crc_before = sapi_checksum_crc64(data, sizeof(data));
    assert(crc_before == 0ULL);

    assert(sapi_checksum_crc64_init(polynomial) == SAPI_STATUS_OK);
    assert(sapi_checksum_crc64_get_polynomial() == polynomial);

    /* Deterministic (REQ-CHECKSUM-003): same input -> same output. */
    crc_a = sapi_checksum_crc64(data, sizeof(data));
    crc_b = sapi_checksum_crc64(data, sizeof(data));
    assert(crc_a == crc_b);

    exit(0);
}

/* Forks, runs child_test_polynomial(polynomial) in the child, and
 * asserts the child exited with status 0 in the parent. */
static void run_polynomial_in_child(sapi_crc64_polynomial_t polynomial)
{
    pid_t pid;
    int wstatus = 0;

    pid = fork();
    assert(pid >= 0);
    if (pid == 0)
    {
        child_test_polynomial(polynomial);
        /* Unreachable: child_test_polynomial() always calls exit(). */
        _exit(127);
    }
    assert(waitpid(pid, &wstatus, 0) == pid);
    assert(WIFEXITED(wstatus));
    assert(WEXITSTATUS(wstatus) == 0);
}

/* Builds a valid vital message from `payload`/`payload_size` and asserts
 * creation succeeded; returns via out-param. */
static void make_message(sapi_vital_message_t *msg_out,
                          uint32_t sender_id,
                          uint32_t sequence,
                          const uint8_t *payload,
                          size_t payload_size)
{
    assert(sapi_checksum_vital_message_create(msg_out, sender_id, sequence,
                                               payload, payload_size) == SAPI_STATUS_OK);
}

static void test_crc64_init_invalid_polynomial(void)
{
    /* Default arm of the polynomial switch: does not set `initialized`,
     * so this is safe to run before the real (once-only) init below. */
    assert(sapi_checksum_crc64_init((sapi_crc64_polynomial_t)99) == SAPI_STATUS_INVALID_PARAM);
}

static void test_crc64_polynomials_in_children(void)
{
    run_polynomial_in_child(SAPI_CRC64_ISO);
    run_polynomial_in_child(SAPI_CRC64_XZ);
}

/* From here on, the parent process performs the single real,
 * process-lifetime init (ERTMS) and all remaining tests run against it. */
static void test_crc64_init_real_and_already_initialized(void)
{
    assert(sapi_checksum_crc64_init(SAPI_CRC64_ERTMS) == SAPI_STATUS_OK);
    assert(sapi_checksum_crc64_get_polynomial() == SAPI_CRC64_ERTMS);

    /* REQ-CHECKSUM-001: second call must fail, table/polynomial
     * unchanged. */
    assert(sapi_checksum_crc64_init(SAPI_CRC64_ISO) == SAPI_STATUS_ALREADY_INITIALIZED);
    assert(sapi_checksum_crc64_get_polynomial() == SAPI_CRC64_ERTMS);
}

static void test_crc64_null_and_empty_data(void)
{
    /* data == NULL, size == 0: returns the raw initial value (all-ones),
     * *without* the final XOR - a deliberate quirk of the early return,
     * distinct from the normal (empty-input) path via the loop. */
    assert(sapi_checksum_crc64(NULL, 0U) == 0xFFFFFFFFFFFFFFFFULL);

    /* data == NULL, size != 0: must not dereference data - returns 0
     * (REQ-CHECKSUM-002). */
    assert(sapi_checksum_crc64(NULL, 5U) == 0ULL);
}

static void test_crc64_normal_and_stats(void)
{
    const uint8_t data[8] = { 0xDEU, 0xADU, 0xBEU, 0xEFU, 0x00U, 0x11U, 0x22U, 0x33U };
    sapi_checksum_stats_t before;
    sapi_checksum_stats_t after;
    sapi_crc64_t crc1;
    sapi_crc64_t crc2;

    assert(sapi_checksum_get_stats(&before) == SAPI_STATUS_OK);

    /* Non-empty buffer: normal loop path. */
    crc1 = sapi_checksum_crc64(data, sizeof(data));
    assert(crc1 != 0ULL);

    /* Zero-size but non-NULL data: loop runs 0 times, but (unlike the
     * data == NULL early return) the final XOR still applies, so the
     * all-ones initial value XORs to 0 - still counted in stats. */
    crc2 = sapi_checksum_crc64(data, 0U);
    assert(crc2 == 0ULL);

    assert(sapi_checksum_get_stats(&after) == SAPI_STATUS_OK);
    assert(after.total_checksums == before.total_checksums + 2U);
}

static void test_crc64_verify(void)
{
    const uint8_t data[4] = { 0xAAU, 0xBBU, 0xCCU, 0xDDU };
    sapi_crc64_t good_crc;
    sapi_checksum_result_t result;
    sapi_checksum_stats_t before;
    sapi_checksum_stats_t after;

    /* NULL result_out -> INVALID_PARAM. */
    assert(sapi_checksum_crc64_verify(data, sizeof(data), 0ULL, NULL) == SAPI_STATUS_INVALID_PARAM);

    good_crc = sapi_checksum_crc64(data, sizeof(data));

    /* Matching CRC -> OK, verification_passes incremented. */
    assert(sapi_checksum_get_stats(&before) == SAPI_STATUS_OK);
    assert(sapi_checksum_crc64_verify(data, sizeof(data), good_crc, &result) == SAPI_STATUS_OK);
    assert(result.match == 1U);
    assert(result.computed == good_crc);
    assert(result.expected == good_crc);
    assert(sapi_checksum_get_stats(&after) == SAPI_STATUS_OK);
    assert(after.verification_passes == before.verification_passes + 1U);
    assert(after.verification_failures == before.verification_failures);

    /* Mismatching CRC -> DATA_CORRUPTION, verification_failures
     * incremented. */
    assert(sapi_checksum_get_stats(&before) == SAPI_STATUS_OK);
    assert(sapi_checksum_crc64_verify(data, sizeof(data), good_crc ^ 0x1ULL, &result)
           == SAPI_STATUS_DATA_CORRUPTION);
    assert(result.match == 0U);
    assert(sapi_checksum_get_stats(&after) == SAPI_STATUS_OK);
    assert(after.verification_failures == before.verification_failures + 1U);
    assert(after.verification_passes == before.verification_passes);
}

static void test_vital_message_create_param_validation(void)
{
    sapi_vital_message_t msg;
    uint8_t big_payload[249] = { 0 };
    sapi_checksum_stats_t before;
    sapi_checksum_stats_t after;

    /* msg_out == NULL -> INVALID_PARAM. */
    assert(sapi_checksum_vital_message_create(NULL, 1U, 1U, big_payload, 4U)
           == SAPI_STATUS_INVALID_PARAM);

    /* payload_size > 248 -> INVALID_PARAM, payload_oversize incremented. */
    assert(sapi_checksum_get_stats(&before) == SAPI_STATUS_OK);
    assert(sapi_checksum_vital_message_create(&msg, 1U, 1U, big_payload, sizeof(big_payload))
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_checksum_get_stats(&after) == SAPI_STATUS_OK);
    assert(after.payload_oversize == before.payload_oversize + 1U);

    /* payload == NULL && payload_size != 0 -> INVALID_PARAM. */
    assert(sapi_checksum_vital_message_create(&msg, 1U, 1U, NULL, 4U)
           == SAPI_STATUS_INVALID_PARAM);
}

static void test_vital_message_create_and_verify_roundtrip(void)
{
    sapi_vital_message_t msg;
    const uint8_t payload[16] = "0123456789ABCDE";
    uint8_t out_payload[16] = { 0 };
    uint8_t out_size = 0U;

    make_message(&msg, 0xAAU, 42U, payload, sizeof(payload));
    assert(msg.sender_id == 0xAAU);
    assert(msg.sequence_number == 42U);
    assert(msg.payload_size == (uint8_t)sizeof(payload));
    assert(memcmp(msg.payload, payload, sizeof(payload)) == 0);

    assert(sapi_checksum_vital_message_verify(&msg, 42U, out_payload, sizeof(out_payload), &out_size)
           == SAPI_STATUS_OK);
    assert(out_size == (uint8_t)sizeof(payload));
    assert(memcmp(out_payload, payload, sizeof(payload)) == 0);
}

static void test_vital_message_create_empty_payload(void)
{
    /* payload == NULL && payload_size == 0: valid (false branch of the
     * "(payload != NULL) && (payload_size > 0)" memcpy guard, short-
     * circuited on the first condition). */
    sapi_vital_message_t msg;
    uint8_t out_payload[1] = { 0xFFU };
    uint8_t out_size = 0xFFU;
    uint8_t non_null_but_empty = 0U;

    make_message(&msg, 0xBBU, 7U, NULL, 0U);
    assert(msg.payload_size == 0U);

    assert(sapi_checksum_vital_message_verify(&msg, 7U, out_payload, sizeof(out_payload), &out_size)
           == SAPI_STATUS_OK);
    /* payload_size == 0 branch of verify(): no memcpy performed. */
    assert(out_size == 0U);

    /* payload != NULL but payload_size == 0: valid too, and exercises
     * the *other* way to reach the memcpy guard's false branch (first
     * condition true, second condition false, rather than short-
     * circuited on the first). */
    make_message(&msg, 0xBBU, 8U, &non_null_but_empty, 0U);
    assert(msg.payload_size == 0U);
}

static void test_vital_message_verify_param_validation(void)
{
    sapi_vital_message_t msg;
    const uint8_t payload[4] = { 1U, 2U, 3U, 4U };
    uint8_t out_payload[4] = { 0 };
    uint8_t out_size = 0U;

    make_message(&msg, 1U, 1U, payload, sizeof(payload));

    assert(sapi_checksum_vital_message_verify(NULL, 1U, out_payload, sizeof(out_payload), &out_size)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_checksum_vital_message_verify(&msg, 1U, NULL, sizeof(out_payload), &out_size)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_checksum_vital_message_verify(&msg, 1U, out_payload, sizeof(out_payload), NULL)
           == SAPI_STATUS_INVALID_PARAM);
}

static void test_vital_message_verify_corrupted_crc(void)
{
    sapi_vital_message_t msg;
    const uint8_t payload[4] = { 1U, 2U, 3U, 4U };
    uint8_t out_payload[4] = { 0 };
    uint8_t out_size = 0U;

    make_message(&msg, 1U, 5U, payload, sizeof(payload));
    /* Tamper with a payload byte after CRC was computed. */
    msg.payload[0] ^= 0xFFU;

    assert(sapi_checksum_vital_message_verify(&msg, 5U, out_payload, sizeof(out_payload), &out_size)
           == SAPI_STATUS_DATA_CORRUPTION);
}

static void test_vital_message_verify_sequence_mismatch(void)
{
    sapi_vital_message_t msg;
    const uint8_t payload[4] = { 1U, 2U, 3U, 4U };
    uint8_t out_payload[4] = { 0 };
    uint8_t out_size = 0U;
    sapi_checksum_stats_t before;
    sapi_checksum_stats_t after;

    make_message(&msg, 1U, 10U, payload, sizeof(payload));

    assert(sapi_checksum_get_stats(&before) == SAPI_STATUS_OK);
    assert(sapi_checksum_vital_message_verify(&msg, 11U /* wrong */, out_payload,
                                               sizeof(out_payload), &out_size)
           == SAPI_STATUS_DATA_CORRUPTION);
    assert(sapi_checksum_get_stats(&after) == SAPI_STATUS_OK);
    assert(after.sequence_errors == before.sequence_errors + 1U);
}

static void test_vital_message_verify_oversized_payload(void)
{
    sapi_vital_message_t msg;
    uint8_t payload[32];
    uint8_t out_payload[8]; /* smaller than msg's payload_size */
    uint8_t out_size = 0U;
    sapi_checksum_stats_t before;
    sapi_checksum_stats_t after;
    size_t i;

    for (i = 0U; i < sizeof(payload); i++)
    {
        payload[i] = (uint8_t)i;
    }
    make_message(&msg, 1U, 20U, payload, sizeof(payload));

    assert(sapi_checksum_get_stats(&before) == SAPI_STATUS_OK);
    assert(sapi_checksum_vital_message_verify(&msg, 20U, out_payload, sizeof(out_payload), &out_size)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_checksum_get_stats(&after) == SAPI_STATUS_OK);
    assert(after.payload_oversize == before.payload_oversize + 1U);
}

static void test_get_stats_null(void)
{
    assert(sapi_checksum_get_stats(NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_reset_stats(void)
{
    sapi_checksum_stats_t stats;

    assert(sapi_checksum_reset_stats() == SAPI_STATUS_OK);
    assert(sapi_checksum_get_stats(&stats) == SAPI_STATUS_OK);
    assert(stats.total_checksums == 0U);
    assert(stats.verification_passes == 0U);
    assert(stats.verification_failures == 0U);
    assert(stats.sequence_errors == 0U);
    assert(stats.payload_oversize == 0U);
}

int main(void)
{
    /* Order matters: everything before test_crc64_init_real_and_already_
     * initialized() must not perform the real (process-lifetime, single-
     * shot) init in the parent - the ISO/XZ polynomial and pre-init
     * behavior are exercised in short-lived forked children instead. */
    test_crc64_init_invalid_polynomial();
    test_crc64_polynomials_in_children();

    test_crc64_init_real_and_already_initialized();

    test_crc64_null_and_empty_data();
    test_crc64_normal_and_stats();
    test_crc64_verify();

    test_vital_message_create_param_validation();
    test_vital_message_create_and_verify_roundtrip();
    test_vital_message_create_empty_payload();
    test_vital_message_verify_param_validation();
    test_vital_message_verify_corrupted_crc();
    test_vital_message_verify_sequence_mismatch();
    test_vital_message_verify_oversized_payload();

    test_get_stats_null();

    /* Reset must run last: it zeroes the stats counters that earlier
     * tests asserted deltas against. */
    test_reset_stats();

    return 0;
}
