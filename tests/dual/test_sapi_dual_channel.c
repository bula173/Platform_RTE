/* Tests for sapi_dual_channel_t (ADR-020 Layer 2): DATA send with
 * per-link ACK accounting and aggregate connection-status/callback
 * tracking, DATA receive with auto-ACK, and the STATE-frame pair used
 * by sapi_dual_negotiator_t.
 *
 * Follows tests/checkpoint/test_sapi_checkpoint.c's own precedent for
 * this exact class of problem: rather than simulating a live two-sided
 * round trip across separate top-level calls in a single-threaded test
 * process (which has no way to let "the other side" run *during* a
 * blocking wait, and a fresh sapi_dual_channel_send() call always
 * advances to a new sequence number each time it's called, so a stale
 * leftover ACK from an earlier attempt would never match a later one
 * anyway), each scenario pre-seeds whatever mailbox content the
 * function under test needs to see, via a throwaway
 * sapi_dual_msgchannel_t standing in for "what the peer would have
 * sent," then makes exactly one call and asserts the outcome. Only the
 * STATE-frame pair (fire-and-forget, no ACK wait) is exercised as a
 * live two-call round trip, since there is no timing race to avoid there. */
#include <assert.h>
#include <string.h>

#include "safeapi/redundancy/checksum/sapi_checksum.h"
#include "safeapi/redundancy/dual/sapi_dual_channel.h"
#include "safeapi/oal/timer/sapi_timer.h"
#include "safeapi_backend/netlink/sapi_netlink_backend.h"
#include "safeapi_backend/timer/sapi_timer_backend.h"

typedef struct
{
    uint8_t buf[sizeof(sapi_vital_message_t)];
    size_t  size;
    int     has_data;
} mock_mailbox_t;

typedef struct
{
    mock_mailbox_t *inbox;
    mock_mailbox_t *outbox;
    int              broken;
    /** Distinct from `broken` (which simulates "peer just hasn't
     *  answered yet" - SAPI_STATUS_TIMEOUT): simulates a genuinely dead
     *  transport (peer closed/reset the connection), matching what
     *  src/posix_backend/sapi_posix_backend_netlink.c's own
     *  transfer_all() actually returns for ECONNRESET/EPIPE/a clean
     *  EOF - SAPI_STATUS_HARDWARE_FAULT - see REQ-DUAL-CHANNEL-008. */
    int              hard_fault;
} mock_link_t;

static sapi_status_t mock_send(sapi_netlink_handle_t handle, const void *message, size_t message_size,
                                sapi_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)handle;

    (void)timeout_ms;
    if (link->hard_fault)
    {
        return SAPI_STATUS_HARDWARE_FAULT;
    }
    if (link->broken)
    {
        return SAPI_STATUS_TIMEOUT;
    }
    if (message_size > sizeof(link->outbox->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    (void)memcpy(link->outbox->buf, message, message_size);
    link->outbox->size     = message_size;
    link->outbox->has_data = 1;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_receive(sapi_netlink_handle_t handle, void *out_message, size_t buffer_size,
                                   sapi_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)handle;

    (void)timeout_ms;
    if (link->hard_fault)
    {
        return SAPI_STATUS_HARDWARE_FAULT;
    }
    if (link->broken || (!link->inbox->has_data))
    {
        return SAPI_STATUS_TIMEOUT;
    }
    if (link->inbox->size > buffer_size)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    (void)memcpy(out_message, link->inbox->buf, link->inbox->size);
    link->inbox->has_data = 0;
    return SAPI_STATUS_OK;
}

static const sapi_netlink_backend_t g_mock_netlink_backend = { NULL, mock_send, mock_receive, NULL };

/* --- mock timer: increments by 5ms every call. Registered so
 *     sapi_dual_channel_send()'s own ACK-wait loop can observe real
 *     wall-clock *progress* between polls (REQ-DUAL-CHANNEL-007's own
 *     "elapsed >= ack_timeout_ms" / "recompute remaining from elapsed"
 *     paths) - with no timer backend registered at all, a fast
 *     in-process mock netlink round trip completes so quickly that the
 *     loop's own stall-poll cap (not wall-clock progress) is what ends
 *     it every time, and those two specific paths are never reached. --- */
static uint64_t g_mock_clock_ms = 0U;

/** Number of upcoming mock_timer_now() calls that must report NO
 *  progress (same value as the previous call) before resuming normal
 *  5ms/call advancement - lets a dedicated test force
 *  sapi_dual_channel_send()'s own ACK-wait loop through its
 *  no-measurable-progress ("now_ms == start_ms") stall_polls++ path on
 *  demand, see that loop's own REQ-DUAL-CHANNEL-007 comment. 0 in every
 *  other test (the common case: the clock always advances). */
static int g_freeze_clock_calls_remaining = 0;

static sapi_status_t mock_timer_now(sapi_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = g_mock_clock_ms;
    if (g_freeze_clock_calls_remaining > 0)
    {
        g_freeze_clock_calls_remaining--;
    }
    else
    {
        g_mock_clock_ms += 5U;
    }
    return SAPI_STATUS_OK;
}

static const sapi_timer_backend_t g_mock_timer_backend = { NULL, NULL, NULL, NULL, mock_timer_now };

/* --- fixture: two redundant links between "A" (sender_id 1) and "B"
 *     (sender_id 2) --- */
typedef struct
{
    mock_mailbox_t mailbox0_a_to_b;
    mock_mailbox_t mailbox0_b_to_a;
    mock_mailbox_t mailbox1_a_to_b;
    mock_mailbox_t mailbox1_b_to_a;
    mock_link_t    a_link0;
    mock_link_t    b_link0;
    mock_link_t    a_link1;
    mock_link_t    b_link1;
} fixture_t;

static void fixture_init(fixture_t *fx)
{
    (void)memset(fx, 0, sizeof(*fx));
    fx->a_link0.inbox  = &fx->mailbox0_b_to_a;
    fx->a_link0.outbox = &fx->mailbox0_a_to_b;
    fx->b_link0.inbox  = &fx->mailbox0_a_to_b;
    fx->b_link0.outbox = &fx->mailbox0_b_to_a;
    fx->a_link1.inbox  = &fx->mailbox1_b_to_a;
    fx->a_link1.outbox = &fx->mailbox1_a_to_b;
    fx->b_link1.inbox  = &fx->mailbox1_a_to_b;
    fx->b_link1.outbox = &fx->mailbox1_b_to_a;
}

static void init_channel_a(fixture_t *fx, sapi_dual_channel_t *channel, sapi_dual_channel_status_callback_t cb)
{
    sapi_dual_channel_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.links[0]         = (sapi_netlink_handle_t)&fx->a_link0;
    cfg.links[1]         = (sapi_netlink_handle_t)&fx->a_link1;
    cfg.link_count       = 2U;
    cfg.sender_id        = 1U;
    cfg.expected_peer_id = 2U;
    cfg.ack_timeout_ms   = 10U;
    cfg.status_callback   = cb;
    assert(sapi_dual_channel_init(channel, &cfg) == SAPI_STATUS_OK);
}

static void init_channel_b(fixture_t *fx, sapi_dual_channel_t *channel)
{
    sapi_dual_channel_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.links[0]         = (sapi_netlink_handle_t)&fx->b_link0;
    cfg.links[1]         = (sapi_netlink_handle_t)&fx->b_link1;
    cfg.link_count       = 2U;
    cfg.sender_id        = 2U;
    cfg.expected_peer_id = 1U;
    cfg.ack_timeout_ms   = 10U;
    assert(sapi_dual_channel_init(channel, &cfg) == SAPI_STATUS_OK);
}

/** Seeds a valid SAPI_DUAL_FRAME_KIND_ACK frame, as "B" would have sent
 *  it, directly into a mailbox - see this file's own header doc on why
 *  pre-seeding rather than a live round trip. */
static void seed_ack(mock_link_t *b_side_link, uint32_t acked_sequence)
{
    sapi_dual_msgchannel_t        seed;
    sapi_dual_msgchannel_config_t seed_cfg;
    sapi_dual_ack_frame_t          ack;

    seed_cfg.link             = (sapi_netlink_handle_t)b_side_link;
    seed_cfg.sender_id        = 2U;
    seed_cfg.expected_peer_id = 1U;
    assert(sapi_dual_msgchannel_init(&seed, &seed_cfg) == SAPI_STATUS_OK);

    ack.header.kind        = (uint8_t)SAPI_DUAL_FRAME_KIND_ACK;
    ack.header.reserved[0] = 0U;
    ack.header.reserved[1] = 0U;
    ack.header.reserved[2] = 0U;
    ack.acked_sequence     = acked_sequence;

    assert(sapi_dual_msgchannel_send(&seed, (const uint8_t *)&ack, (uint8_t)sizeof(ack), 10U, NULL) == SAPI_STATUS_OK);
}

/** Seeds a raw SAPI_DUAL_FRAME_KIND_DATA frame, as "A" would have sent
 *  it, directly into a mailbox. */
static void seed_data(mock_link_t *a_side_link, const uint8_t *payload, uint8_t payload_size)
{
    sapi_dual_msgchannel_t        seed;
    sapi_dual_msgchannel_config_t seed_cfg;
    uint8_t                        frame[SAPI_DUAL_CHANNEL_MAX_PAYLOAD + 4U];
    sapi_dual_frame_header_t       header;

    seed_cfg.link             = (sapi_netlink_handle_t)a_side_link;
    seed_cfg.sender_id        = 1U;
    seed_cfg.expected_peer_id = 2U;
    assert(sapi_dual_msgchannel_init(&seed, &seed_cfg) == SAPI_STATUS_OK);

    header.kind        = (uint8_t)SAPI_DUAL_FRAME_KIND_DATA;
    header.reserved[0] = 0U;
    header.reserved[1] = 0U;
    header.reserved[2] = 0U;
    (void)memcpy(frame, &header, sizeof(header));
    (void)memcpy(&frame[sizeof(header)], payload, payload_size);

    assert(sapi_dual_msgchannel_send(&seed, frame, (uint8_t)(sizeof(header) + payload_size), 10U, NULL)
           == SAPI_STATUS_OK);
}

/** Seeds an arbitrary raw Layer-2 payload directly into a mailbox, as
 *  "B" would have sent it - used to construct malformed/short/unknown-
 *  kind frames that sapi_dual_msgchannel_send()'s own higher-level
 *  helpers (which always build a well-formed frame) cannot produce. */
static void seed_raw(mock_link_t *b_side_link, const uint8_t *raw, uint8_t raw_size)
{
    sapi_dual_msgchannel_t        seed;
    sapi_dual_msgchannel_config_t seed_cfg;

    seed_cfg.link             = (sapi_netlink_handle_t)b_side_link;
    seed_cfg.sender_id        = 2U;
    seed_cfg.expected_peer_id = 1U;
    assert(sapi_dual_msgchannel_init(&seed, &seed_cfg) == SAPI_STATUS_OK);

    assert(sapi_dual_msgchannel_send(&seed, raw, raw_size, 10U, NULL) == SAPI_STATUS_OK);
}

static int                        g_status_callback_calls = 0;
static sapi_dual_channel_status_t g_last_new_status;
static sapi_dual_channel_status_t g_last_old_status;

static void status_callback(sapi_dual_channel_status_t new_status, sapi_dual_channel_status_t old_status,
                             void *user_ctx)
{
    (void)user_ctx;
    g_status_callback_calls++;
    g_last_new_status = new_status;
    g_last_old_status = old_status;
}

static void test_invalid_params(void)
{
    fixture_t                  fx;
    sapi_dual_channel_t         channel_a;
    sapi_dual_channel_config_t  cfg;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.links[0]   = (sapi_netlink_handle_t)&fx.a_link0;
    cfg.link_count = 2U;

    assert(sapi_dual_channel_init(NULL, &cfg) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_init(&channel_a, NULL) == SAPI_STATUS_INVALID_PARAM);

    {
        sapi_dual_channel_config_t zero_links = cfg;

        zero_links.link_count = 0U;
        assert(sapi_dual_channel_init(&channel_a, &zero_links) == SAPI_STATUS_INVALID_PARAM);
    }
    {
        sapi_dual_channel_config_t too_many = cfg;

        too_many.link_count = SAPI_DUAL_CHANNEL_MAX_LINKS + 1U;
        assert(sapi_dual_channel_init(&channel_a, &too_many) == SAPI_STATUS_INVALID_PARAM);
    }
    {
        /* cfg.links[1] is still NULL (fixture only set links[0] above)
         * with link_count claiming 2 populated entries. */
        assert(sapi_dual_channel_init(&channel_a, &cfg) == SAPI_STATUS_INVALID_PARAM);
    }
}

static void test_send_both_links_ack_full(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 0U;

    fixture_init(&fx);
    g_status_callback_calls = 0;
    init_channel_a(&fx, &channel_a, status_callback);

    assert(sapi_dual_channel_get_status(&channel_a) == SAPI_DUAL_CHANNEL_STATUS_DOWN);

    seed_ack(&fx.b_link0, 0U);
    seed_ack(&fx.b_link1, 0U);

    assert(sapi_dual_channel_send(&channel_a, (const uint8_t *)"AB_SAMPLE", 9U, &ack_count) == SAPI_STATUS_OK);
    assert(ack_count == 2U);
    assert(sapi_dual_channel_get_status(&channel_a) == SAPI_DUAL_CHANNEL_STATUS_FULL);
    assert(sapi_dual_channel_is_link_up(&channel_a, 0U) == true);
    assert(sapi_dual_channel_is_link_up(&channel_a, 1U) == true);
    assert(g_status_callback_calls == 1);
    assert(g_last_old_status == SAPI_DUAL_CHANNEL_STATUS_DOWN);
    assert(g_last_new_status == SAPI_DUAL_CHANNEL_STATUS_FULL);
}

static void test_send_one_link_acks_degraded(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 0U;

    fixture_init(&fx);
    g_status_callback_calls = 0;
    init_channel_a(&fx, &channel_a, status_callback);

    seed_ack(&fx.b_link0, 0U);
    /* link1 gets no seeded ACK - its send still "goes out" but nothing
     * ever answers it within this one call. */

    assert(sapi_dual_channel_send(&channel_a, (const uint8_t *)"x", 1U, &ack_count) == SAPI_STATUS_OK);
    assert(ack_count == 1U);
    assert(sapi_dual_channel_get_status(&channel_a) == SAPI_DUAL_CHANNEL_STATUS_DEGRADED);
    assert(sapi_dual_channel_is_link_up(&channel_a, 0U) == true);
    assert(sapi_dual_channel_is_link_up(&channel_a, 1U) == false);
    assert(g_last_new_status == SAPI_DUAL_CHANNEL_STATUS_DEGRADED);
}

static void test_send_no_acks_down(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 99U;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    /* Nothing seeded at all - both links time out. */
    assert(sapi_dual_channel_send(&channel_a, (const uint8_t *)"y", 1U, &ack_count) == SAPI_STATUS_TIMEOUT);
    assert(ack_count == 0U);
    assert(sapi_dual_channel_get_status(&channel_a) == SAPI_DUAL_CHANNEL_STATUS_DOWN);
}

static void test_send_broken_link_never_counts(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 0U;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    fx.a_link1.broken = 1;
    seed_ack(&fx.b_link0, 0U);
    seed_ack(&fx.b_link1, 0U); /* seeded, but link1's own send() never even reaches the wire */

    assert(sapi_dual_channel_send(&channel_a, (const uint8_t *)"z", 1U, &ack_count) == SAPI_STATUS_OK);
    assert(ack_count == 1U);
    assert(sapi_dual_channel_is_link_up(&channel_a, 0U) == true);
    assert(sapi_dual_channel_is_link_up(&channel_a, 1U) == false);
}

/** REQ-DUAL-CHANNEL-008 regression test: a link whose own send genuinely
 *  fails (peer closed/reset the connection - SAPI_STATUS_HARDWARE_FAULT,
 *  not just "hasn't ACKed yet") must not be silently collapsed into the
 *  same SAPI_STATUS_TIMEOUT test_send_no_acks_down() above exercises for
 *  an ordinary unanswered send. This is the exact bug a safeAPIRBC2oo2
 *  container-topology failover test found live: a consumer's own
 *  reconnect logic gates link teardown on "status is neither OK nor
 *  TIMEOUT", so a dead connection reported as a bland TIMEOUT never
 *  triggered a reconnect. */
static void test_send_hard_fault_propagates_status(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 99U;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    fx.a_link0.hard_fault = 1;
    fx.a_link1.hard_fault = 1;

    assert(sapi_dual_channel_send(&channel_a, (const uint8_t *)"y", 1U, &ack_count) == SAPI_STATUS_HARDWARE_FAULT);
    assert(ack_count == 0U);
    assert(sapi_dual_channel_get_status(&channel_a) == SAPI_DUAL_CHANNEL_STATUS_DOWN);
}

/** REQ-DUAL-CHANNEL-008 regression test: same distinction as
 *  test_send_hard_fault_propagates_status() above, for the receive side -
 *  a link reporting a genuinely dead connection while
 *  sapi_dual_channel_receive() sweeps it must not be silently collapsed
 *  into the same SAPI_STATUS_TIMEOUT "nothing staged yet" every other
 *  empty-sweep case returns. */
static void test_receive_hard_fault_propagates_status(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint8_t               out_payload[SAPI_DUAL_CHANNEL_MAX_PAYLOAD];
    uint8_t               out_size = 0U;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    fx.a_link0.hard_fault = 1;
    fx.a_link1.hard_fault = 1;

    assert(sapi_dual_channel_receive(&channel_a, out_payload, sizeof(out_payload), 5U, &out_size)
           == SAPI_STATUS_HARDWARE_FAULT);
}

static void test_receive_auto_acks_and_stages_payload(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_b;
    uint8_t               out_payload[SAPI_DUAL_CHANNEL_MAX_PAYLOAD];
    uint8_t               out_size = 0U;

    fixture_init(&fx);
    init_channel_b(&fx, &channel_b);

    seed_data(&fx.a_link0, (const uint8_t *)"hello", 5U);

    assert(sapi_dual_channel_receive(&channel_b, out_payload, sizeof(out_payload), 5U, &out_size) == SAPI_STATUS_OK);
    assert(out_size == 5U);
    assert(memcmp(out_payload, "hello", 5U) == 0);

    /* Auto-ACK was sent back to A's own inbox as a side effect - verify
     * by decoding it with a throwaway "A-side" msgchannel. */
    {
        sapi_dual_msgchannel_t        checker;
        sapi_dual_msgchannel_config_t checker_cfg;
        uint8_t                       raw[SAPI_DUAL_MSGCHANNEL_MAX_PAYLOAD];
        uint8_t                       raw_size = 0U;
        sapi_dual_ack_frame_t          ack;

        checker_cfg.link             = (sapi_netlink_handle_t)&fx.a_link0;
        checker_cfg.sender_id        = 1U;
        checker_cfg.expected_peer_id = 2U;
        assert(sapi_dual_msgchannel_init(&checker, &checker_cfg) == SAPI_STATUS_OK);
        assert(sapi_dual_msgchannel_receive(&checker, raw, sizeof(raw), 5U, &raw_size, NULL) == SAPI_STATUS_OK);
        assert(raw_size >= (uint8_t)sizeof(ack));
        (void)memcpy(&ack, raw, sizeof(ack));
        assert(ack.header.kind == (uint8_t)SAPI_DUAL_FRAME_KIND_ACK);
        assert(ack.acked_sequence == 0U);
    }

    /* Nothing else pending. */
    assert(sapi_dual_channel_receive(&channel_b, out_payload, sizeof(out_payload), 0U, &out_size)
           == SAPI_STATUS_TIMEOUT);
}

static void test_state_frame_roundtrip(void)
{
    fixture_t                fx;
    sapi_dual_channel_t       channel_a;
    sapi_dual_channel_t       channel_b;
    sapi_dual_state_frame_t   received;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);
    init_channel_b(&fx, &channel_b);

    assert(sapi_dual_channel_send_state_frame(&channel_a, SAPI_DUAL_STATE_ONLINE, false, 12345U) == SAPI_STATUS_OK);
    /* One call sweeps and drains every configured link (both link0 and
     * link1 each received a copy of the same beacon), staging whichever
     * was processed last - see sapi_dual_channel_receive()'s own
     * "sweeps every link" doc, which _receive_state_frame() shares. */
    assert(sapi_dual_channel_receive_state_frame(&channel_b, 5U, &received) == SAPI_STATUS_OK);
    assert(received.state == (uint8_t)SAPI_DUAL_STATE_ONLINE);
    assert(received.channel_degraded == 0U);
    assert(received.timestamp_ms == 12345U);

    /* Both links already drained by the single call above - nothing left. */
    assert(sapi_dual_channel_receive_state_frame(&channel_b, 0U, &received) == SAPI_STATUS_TIMEOUT);
}

static void test_send_param_validation(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint8_t               oversize[SAPI_DUAL_CHANNEL_MAX_PAYLOAD + 1U];

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);
    (void)memset(oversize, 0, sizeof(oversize));

    assert(sapi_dual_channel_send(NULL, (const uint8_t *)"a", 1U, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_send(&channel_a, NULL, 1U, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_send(&channel_a, oversize, (uint8_t)sizeof(oversize), NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_receive_param_validation(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_b;
    uint8_t               out_payload[SAPI_DUAL_CHANNEL_MAX_PAYLOAD];
    uint8_t               out_size = 0U;

    fixture_init(&fx);
    init_channel_b(&fx, &channel_b);

    assert(sapi_dual_channel_receive(NULL, out_payload, sizeof(out_payload), 0U, &out_size)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_receive(&channel_b, NULL, sizeof(out_payload), 0U, &out_size)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_receive(&channel_b, out_payload, sizeof(out_payload), 0U, NULL)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_receive(&channel_b, out_payload, 0U, 0U, &out_size) == SAPI_STATUS_INVALID_PARAM);
}

static void test_receive_staged_frame_too_big_for_caller_buffer(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_b;
    uint8_t               small_buf[3];
    uint8_t               out_size = 0U;

    fixture_init(&fx);
    init_channel_b(&fx, &channel_b);

    /* "hello" (5 bytes) is staged, but the caller's own buffer only
     * holds 3 - reported, not silently truncated. */
    seed_data(&fx.a_link0, (const uint8_t *)"hello", 5U);
    assert(sapi_dual_channel_receive(&channel_b, small_buf, sizeof(small_buf), 5U, &out_size)
           == SAPI_STATUS_INVALID_PARAM);
}

static void test_send_state_frame_and_receive_state_frame_param_validation(void)
{
    fixture_t                fx;
    sapi_dual_channel_t       channel_a;
    sapi_dual_state_frame_t   received;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    assert(sapi_dual_channel_send_state_frame(NULL, SAPI_DUAL_STATE_ONLINE, false, 1U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_receive_state_frame(NULL, 0U, &received) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_channel_receive_state_frame(&channel_a, 0U, NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_get_status_and_is_link_up_defensive_defaults(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    assert(sapi_dual_channel_get_status(NULL) == SAPI_DUAL_CHANNEL_STATUS_DOWN);
    assert(sapi_dual_channel_is_link_up(NULL, 0U) == false);
    assert(sapi_dual_channel_is_link_up(&channel_a, channel_a.link_count) == false);
    assert(sapi_dual_channel_is_link_up(&channel_a, 999U) == false);
}

/** Exercises dual_channel_poll_link_once()'s own defended-integrity
 *  rejections: a frame too short to hold even the common 4-byte
 *  sapi_dual_frame_header_t, a SAPI_DUAL_FRAME_KIND_ACK frame too short
 *  to hold its own acked_sequence field, a SAPI_DUAL_FRAME_KIND_STATE
 *  frame too short to hold its own full fixed size, and an unrecognized
 *  frame kind (defensive-only, ignored rather than failed). Each is
 *  seeded as a raw Layer-2 payload (seed_raw()) since
 *  sapi_dual_msgchannel_send()'s own normal helpers only ever build
 *  well-formed frames. None of these are ever surfaced back through the
 *  public sapi_dual_channel_receive() API (its own sweep loop discards
 *  dual_channel_poll_link_once()'s per-attempt status) - each assertion
 *  below only confirms nothing valid ends up staged as a result. */
static void test_receive_rejects_malformed_and_unknown_frames(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint8_t               out_payload[SAPI_DUAL_CHANNEL_MAX_PAYLOAD];
    uint8_t               out_size = 0U;

    /* Too short to hold even the common 4-byte header. */
    {
        uint8_t too_short[2] = { 0U, 0U };

        fixture_init(&fx);
        init_channel_a(&fx, &channel_a, NULL);
        seed_raw(&fx.b_link0, too_short, (uint8_t)sizeof(too_short));
        assert(sapi_dual_channel_receive(&channel_a, out_payload, sizeof(out_payload), 5U, &out_size)
               == SAPI_STATUS_TIMEOUT);
    }

    /* Kind == ACK but shorter than sizeof(sapi_dual_ack_frame_t) (8 bytes). */
    {
        uint8_t short_ack[5] = { (uint8_t)SAPI_DUAL_FRAME_KIND_ACK, 0U, 0U, 0U, 0U };

        fixture_init(&fx);
        init_channel_a(&fx, &channel_a, NULL);
        seed_raw(&fx.b_link0, short_ack, (uint8_t)sizeof(short_ack));
        assert(sapi_dual_channel_receive(&channel_a, out_payload, sizeof(out_payload), 5U, &out_size)
               == SAPI_STATUS_TIMEOUT);
    }

    /* Kind == STATE but shorter than sizeof(sapi_dual_state_frame_t). */
    {
        uint8_t short_state[6] = { (uint8_t)SAPI_DUAL_FRAME_KIND_STATE, 0U, 0U, 0U, 0U, 0U };

        fixture_init(&fx);
        init_channel_a(&fx, &channel_a, NULL);
        seed_raw(&fx.b_link0, short_state, (uint8_t)sizeof(short_state));
        assert(sapi_dual_channel_receive(&channel_a, out_payload, sizeof(out_payload), 5U, &out_size)
               == SAPI_STATUS_TIMEOUT);
    }

    /* Unrecognized frame kind - defensive default: ignored, not failed. */
    {
        uint8_t unknown_kind[4] = { 99U, 0U, 0U, 0U };

        fixture_init(&fx);
        init_channel_a(&fx, &channel_a, NULL);
        seed_raw(&fx.b_link0, unknown_kind, (uint8_t)sizeof(unknown_kind));
        assert(sapi_dual_channel_receive(&channel_a, out_payload, sizeof(out_payload), 5U, &out_size)
               == SAPI_STATUS_TIMEOUT);
    }
}

/** Exercises sapi_dual_channel_send()'s own ACK-wait loop actually
 *  observing wall-clock progress between polls (REQ-DUAL-CHANNEL-007):
 *  with the mock timer registered (g_mock_clock_ms advances 5ms per
 *  sapi_timer_now() call), a link with no ACK ever seeded runs the
 *  "now_ms > start_ms" branch enough times to both recompute a smaller
 *  `remaining` budget (elapsed < ack_timeout_ms) and then observe
 *  elapsed >= ack_timeout_ms and give up. */
static void test_send_ack_wait_observes_timer_progress(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 99U;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    /* Nothing seeded on either link - both links' ACK-wait loops run to
     * completion via observed timer progress rather than the stall-poll
     * cap. */
    assert(sapi_dual_channel_send(&channel_a, (const uint8_t *)"t", 1U, &ack_count) == SAPI_STATUS_TIMEOUT);
    assert(ack_count == 0U);
    assert(sapi_dual_channel_is_link_up(&channel_a, 0U) == false);
    assert(sapi_dual_channel_is_link_up(&channel_a, 1U) == false);
}

/** Exercises sapi_dual_channel_send()'s own ACK-wait loop's
 *  no-measurable-progress branch (stall_polls++, REQ-DUAL-CHANNEL-007):
 *  freezes the mock clock for the first few post-start_ms reads so
 *  "now_ms == start_ms" is observed at least once before the clock
 *  resumes advancing and the loop concludes normally. */
static void test_send_ack_wait_handles_stalled_clock(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 99U;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    /* Nothing seeded - both links' waits run out. Freeze the clock for
     * the first 3 post-start_ms reads so the loop observes no progress
     * at least once before it resumes and eventually times out. */
    g_freeze_clock_calls_remaining = 3;
    assert(sapi_dual_channel_send(&channel_a, (const uint8_t *)"s", 1U, &ack_count) == SAPI_STATUS_TIMEOUT);
    assert(ack_count == 0U);
    g_freeze_clock_calls_remaining = 0;
}

static void test_send_heartbeat(void)
{
    fixture_t            fx;
    sapi_dual_channel_t   channel_a;
    uint32_t              ack_count = 0U;

    fixture_init(&fx);
    init_channel_a(&fx, &channel_a, NULL);

    assert(sapi_dual_channel_send_heartbeat(NULL, &ack_count) == SAPI_STATUS_INVALID_PARAM);

    /* Both links timeout when nothing seeded */
    assert(sapi_dual_channel_send_heartbeat(&channel_a, &ack_count) == SAPI_STATUS_TIMEOUT);
    assert(ack_count == 0U);
    assert(sapi_dual_channel_get_status(&channel_a) == SAPI_DUAL_CHANNEL_STATUS_DOWN);

    /* Seed ACK on link 0 and link 1 for next sequence (1U) */
    seed_ack(&fx.b_link0, 1U);
    seed_ack(&fx.b_link1, 1U);
    assert(sapi_dual_channel_send_heartbeat(&channel_a, &ack_count) == SAPI_STATUS_OK);
    assert(ack_count == 2U);
    assert(sapi_dual_channel_get_status(&channel_a) == SAPI_DUAL_CHANNEL_STATUS_FULL);
}

int main(void)
{
    assert(sapi_checksum_crc64_init(SAPI_CRC64_ERTMS) == SAPI_STATUS_OK);
    assert(sapi_netlink_register_backend(&g_mock_netlink_backend) == SAPI_STATUS_OK);
    assert(sapi_timer_register_backend(&g_mock_timer_backend) == SAPI_STATUS_OK);

    test_invalid_params();
    test_send_both_links_ack_full();
    test_send_one_link_acks_degraded();
    test_send_no_acks_down();
    test_send_broken_link_never_counts();
    test_send_hard_fault_propagates_status();
    test_receive_hard_fault_propagates_status();
    test_receive_auto_acks_and_stages_payload();
    test_state_frame_roundtrip();
    test_send_param_validation();
    test_receive_param_validation();
    test_receive_staged_frame_too_big_for_caller_buffer();
    test_send_state_frame_and_receive_state_frame_param_validation();
    test_get_status_and_is_link_up_defensive_defaults();
    test_receive_rejects_malformed_and_unknown_frames();
    test_send_ack_wait_observes_timer_progress();
    test_send_ack_wait_handles_stalled_clock();
    test_send_heartbeat();
    return 0;
}
