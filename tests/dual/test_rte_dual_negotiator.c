/* Tests for rte_dual_negotiator_t (ADR-020 section 3): own/peer state
 * tracking, the older-startup-timestamp-wins tie-break, HOTSTANDBY vs
 * COLDSTANDBY from the peer's own reported channel_degraded bit, and
 * degradation to UNKNOWN on lost peer contact (with ONLINE's own
 * exception). Two live negotiators (A and B) share one pair of
 * rte_dual_channel_t instances over the same mock netlink OSAdapter used
 * by the other dual/ tests - unlike DATA's ACK-wait, STATE beacons are
 * fire-and-forget, so a live two-call round trip (A's own
 * rte_dual_negotiator_execute(), then B's) has no timing race to avoid. */
#include <assert.h>
#include <string.h>

#include "safeapi/redundancy/checksum/rte_checksum.h"
#include "safeapi/redundancy/dual/rte_dual_negotiator.h"
#include "safeapi/oal/timer/rte_timer.h"
#include "safeapi_osadapter/timer/rte_osadapter_timer.h"
#include "safeapi_osadapter/netlink/rte_osadapter_netlink.h"

typedef struct
{
    uint8_t buf[sizeof(rte_vital_message_t)];
    size_t  size;
    int     has_data;
} mock_mailbox_t;

typedef struct
{
    mock_mailbox_t *inbox;
    mock_mailbox_t *outbox;
} mock_link_t;

static rte_status_t mock_send(rte_netlink_handle_t handle, const void *message, size_t message_size,
                                rte_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)handle;

    (void)timeout_ms;
    if (message_size > sizeof(link->outbox->buf))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    (void)memcpy(link->outbox->buf, message, message_size);
    link->outbox->size     = message_size;
    link->outbox->has_data = 1;
    return RTE_STATUS_OK;
}

static rte_status_t mock_receive(rte_netlink_handle_t handle, void *out_message, size_t buffer_size,
                                   rte_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)handle;

    (void)timeout_ms;
    if (!link->inbox->has_data)
    {
        return RTE_STATUS_TIMEOUT;
    }
    if (link->inbox->size > buffer_size)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    (void)memcpy(out_message, link->inbox->buf, link->inbox->size);
    link->inbox->has_data = 0;
    return RTE_STATUS_OK;
}

static const rte_osadapter_netlink_t g_mock_netlink_osadapter = { NULL, mock_send, mock_receive, NULL };

/* --- mock timer: increments by 10ms every call, so whichever negotiator
 *     is init()ed first gets the earlier (smaller) startup timestamp -
 *     deterministic and lets the "older timestamp wins" branch of the
 *     tie-break be exercised directly, not just the own_id fallback. --- */
static uint64_t g_mock_clock_ms = 0U;

static rte_status_t mock_timer_now(rte_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = g_mock_clock_ms;
    g_mock_clock_ms += 10U;
    return RTE_STATUS_OK;
}

static const rte_osadapter_timer_t g_mock_timer_osadapter = { NULL, NULL, NULL, NULL, mock_timer_now };

/* --- fixture: one single link each way between "A" and "B" --- */
typedef struct
{
    mock_mailbox_t mailbox_a_to_b;
    mock_mailbox_t mailbox_b_to_a;
    mock_link_t    a_link;
    mock_link_t    b_link;
} fixture_t;

static void fixture_init(fixture_t *fx)
{
    (void)memset(fx, 0, sizeof(*fx));
    fx->a_link.inbox  = &fx->mailbox_b_to_a;
    fx->a_link.outbox = &fx->mailbox_a_to_b;
    fx->b_link.inbox  = &fx->mailbox_a_to_b;
    fx->b_link.outbox = &fx->mailbox_b_to_a;
}

static void init_dual_channel(rte_dual_channel_t *channel, mock_link_t *link, uint32_t sender_id,
                               uint32_t expected_peer_id)
{
    rte_dual_channel_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.links[0]         = (rte_netlink_handle_t)link;
    cfg.link_count       = 1U;
    cfg.sender_id        = sender_id;
    cfg.expected_peer_id = expected_peer_id;
    cfg.ack_timeout_ms   = 10U;
    assert(rte_dual_channel_init(channel, &cfg) == RTE_STATUS_OK);
}

static void init_negotiator(rte_dual_negotiator_t *negotiator, rte_dual_channel_t *channel, uint32_t own_id,
                             uint32_t peer_id, rte_duration_ms_t peer_lost_timeout_ms)
{
    rte_dual_negotiator_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.channel               = channel;
    cfg.own_id                = own_id;
    cfg.peer_id                = peer_id;
    cfg.peer_lost_timeout_ms  = peer_lost_timeout_ms;
    assert(rte_dual_negotiator_init(negotiator, &cfg) == RTE_STATUS_OK);
}

/** Forces a channel's own rte_dual_channel_get_status() to FULL for
 *  test setup, without going through a real DATA send/ACK round trip -
 *  deliberately avoided here because that would advance the same
 *  per-link Layer-1 sequence counter rte_dual_negotiator_t's STATE
 *  beacons travel on, desynchronizing it from the peer's own
 *  expected_sequence before negotiation even starts. rte_dual_channel_t
 *  has no public "just report FULL" setter (by design - status is
 *  meant to be an observed fact, not something a caller injects), so
 *  this test reaches into the struct directly; acceptable here since
 *  C's structs have no real encapsulation and this is whitebox test
 *  setup, not production code exercising the public API. A freshly-
 *  init()ed rte_dual_channel_t otherwise starts at DOWN (degraded),
 *  and this test file's negotiation scenarios need to distinguish a
 *  genuinely non-degraded HOTSTANDBY-yielding channel from that
 *  always-degraded-by-default starting condition. */
static void force_channel_to_full(rte_dual_channel_t *channel)
{
    uint32_t i;

    for (i = 0U; i < channel->link_count; i++)
    {
        channel->link_up[i] = true;
    }
    channel->last_status = RTE_DUAL_CHANNEL_STATUS_FULL;
}

/** Same whitebox rationale as force_channel_to_full() above, in the
 *  other direction: forces a channel to report DOWN without a real
 *  un-ACKed rte_dual_channel_send(), which would consume a Layer-1
 *  sequence number on the shared link and desync it from the peer's
 *  expected_sequence (the peer never receives - and so never advances
 *  past - a DATA frame that nobody ACKs), corrupting every STATE
 *  beacon exchanged afterwards. */
static void force_channel_to_down(rte_dual_channel_t *channel)
{
    uint32_t i;

    for (i = 0U; i < channel->link_count; i++)
    {
        channel->link_up[i] = false;
    }
    channel->last_status = RTE_DUAL_CHANNEL_STATUS_DOWN;
}

static void test_invalid_params(void)
{
    fixture_t             fx;
    rte_dual_channel_t    channel_a;
    rte_dual_negotiator_t negotiator_a;
    rte_dual_negotiator_config_t cfg;

    fixture_init(&fx);
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);

    (void)memset(&cfg, 0, sizeof(cfg));
    assert(rte_dual_negotiator_init(NULL, &cfg) == RTE_STATUS_INVALID_PARAM);
    assert(rte_dual_negotiator_init(&negotiator_a, NULL) == RTE_STATUS_INVALID_PARAM);
    cfg.channel = NULL;
    assert(rte_dual_negotiator_init(&negotiator_a, &cfg) == RTE_STATUS_INVALID_PARAM);

    /* Defensive NULL accessors. */
    assert(rte_dual_negotiator_get_own_state(NULL) == RTE_DUAL_STATE_IDLE);
    assert(rte_dual_negotiator_get_peer_state(NULL) == RTE_DUAL_STATE_IDLE);

    /* execute() itself rejects a NULL negotiator too. */
    assert(rte_dual_negotiator_execute(NULL, 5U) == RTE_STATUS_INVALID_PARAM);
}

static int                g_state_change_calls = 0;
static rte_dual_state_t  g_cb_new_own;
static rte_dual_state_t  g_cb_old_own;
static rte_dual_state_t  g_cb_new_peer;
static rte_dual_state_t  g_cb_old_peer;

static void state_change_callback(rte_dual_state_t new_own_state, rte_dual_state_t old_own_state,
                                   rte_dual_state_t new_peer_state, rte_dual_state_t old_peer_state,
                                   void *user_ctx)
{
    (void)user_ctx;
    g_state_change_calls++;
    g_cb_new_own  = new_own_state;
    g_cb_old_own  = old_own_state;
    g_cb_new_peer = new_peer_state;
    g_cb_old_peer = old_peer_state;
}

static void test_state_change_callback_fires_on_change(void)
{
    fixture_t                     fx;
    rte_dual_channel_t            channel_a;
    rte_dual_channel_t            channel_b;
    rte_dual_negotiator_t         negotiator_a;
    rte_dual_negotiator_t         negotiator_b;
    rte_dual_negotiator_config_t  cfg_a;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    force_channel_to_full(&channel_a);

    (void)memset(&cfg_a, 0, sizeof(cfg_a));
    cfg_a.channel               = &channel_a;
    cfg_a.own_id                = 1U;
    cfg_a.peer_id               = 2U;
    cfg_a.peer_lost_timeout_ms  = 1000U;
    cfg_a.state_change_callback = state_change_callback;
    assert(rte_dual_negotiator_init(&negotiator_a, &cfg_a) == RTE_STATUS_OK);
    init_negotiator(&negotiator_b, &channel_b, 2U, 1U, 1000U);

    g_state_change_calls = 0;

    assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
    assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);
    /* Round 2: negotiator_a receives B's round-1 beacon and decides
     * ONLINE - both own_state and peer_state change, callback fires. */
    assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);

    assert(g_state_change_calls >= 1);
    assert(g_cb_old_own == RTE_DUAL_STATE_IDLE);
    assert(g_cb_new_own == RTE_DUAL_STATE_ONLINE);
    assert(g_cb_old_peer == RTE_DUAL_STATE_IDLE);
    assert(g_cb_new_peer == RTE_DUAL_STATE_HOTSTANDBY);
}

/** Exercises negotiator_decide_online()'s own_id/peer_id fallback branch
 *  (rte_dual_negotiator.h REQ-DUAL-NEGOTIATOR-003): reached only on an
 *  exact startup-timestamp tie, which the mock timer (incrementing every
 *  call) never produces on its own - forced here by directly overwriting
 *  negotiator_b's own_startup_timestamp_ms to match negotiator_a's after
 *  both are already init()ed (same whitebox rationale as
 *  force_channel_to_full()/force_channel_to_down() above). */
static void test_tie_break_falls_back_to_id_on_exact_timestamp_tie(void)
{
    fixture_t              fx;
    rte_dual_channel_t    channel_a;
    rte_dual_channel_t    channel_b;
    rte_dual_negotiator_t negotiator_a;
    rte_dual_negotiator_t negotiator_b;
    uint32_t                i;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    force_channel_to_full(&channel_a);
    force_channel_to_full(&channel_b);

    /* own_id 9 (A) vs own_id 3 (B, as B's own peer_id=9's counterpart) -
     * B's own_id (3) is smaller, so B must win the tie-break once
     * timestamps are forced equal. */
    init_negotiator(&negotiator_a, &channel_a, 9U, 3U, 1000U);
    init_negotiator(&negotiator_b, &channel_b, 3U, 9U, 1000U);
    negotiator_b.own_startup_timestamp_ms = negotiator_a.own_startup_timestamp_ms;

    for (i = 0U; i < 2U; i++)
    {
        assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
        assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);
    }

    /* B's own_id (3) < A's own_id (9) -> B wins -> B is ONLINE, A is
     * STANDBY, despite A having executed (and so sent) first. */
    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_ONLINE);
    assert(rte_dual_negotiator_get_own_state(&negotiator_a) != RTE_DUAL_STATE_ONLINE);
}

/** Companion to test_lost_peer_contact_degrades_to_unknown(): that test
 *  covers the ONLINE-stays-ONLINE exception; this one covers the general
 *  case (own_state HOTSTANDBY/COLDSTANDBY/IDLE) actually degrading to
 *  RTE_DUAL_STATE_UNKNOWN on lost peer contact. */
static void test_lost_peer_contact_degrades_non_online_own_state_too(void)
{
    fixture_t              fx;
    rte_dual_channel_t    channel_a;
    rte_dual_channel_t    channel_b;
    rte_dual_negotiator_t negotiator_a;
    rte_dual_negotiator_t negotiator_b;
    uint32_t                i;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    force_channel_to_full(&channel_a);
    init_negotiator(&negotiator_a, &channel_a, 1U, 2U, 20U);
    init_negotiator(&negotiator_b, &channel_b, 2U, 1U, 20U);

    for (i = 0U; i < 2U; i++)
    {
        assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
        assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);
    }
    assert(rte_dual_negotiator_get_own_state(&negotiator_a) == RTE_DUAL_STATE_ONLINE);
    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_HOTSTANDBY);

    /* A stops executing entirely - only B keeps calling execute(), never
     * seeing a fresh beacon from A, letting B's own last_peer_seen_ms
     * fall behind config.peer_lost_timeout_ms. B's own_state
     * (HOTSTANDBY, not ONLINE) must degrade to UNKNOWN too, unlike A's
     * own case in test_lost_peer_contact_degrades_to_unknown(). */
    for (i = 0U; i < 10U; i++)
    {
        assert(rte_dual_negotiator_execute(&negotiator_b, 0U) == RTE_STATUS_OK);
    }

    assert(rte_dual_negotiator_get_peer_state(&negotiator_b) == RTE_DUAL_STATE_UNKNOWN);
    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_UNKNOWN);
}

/** Exercises rte_dual_negotiator_execute()'s own "drain any additional
 *  already-buffered frames" while() loop needing more than one
 *  iteration: a frame already staged as channel_a->pending_state (so the
 *  first rte_dual_channel_receive_state_frame() call inside execute()
 *  returns it immediately without touching the links) plus a second,
 *  still-unread frame sitting in the link's own mailbox (so the while()
 *  loop's own follow-up call finds and processes it too). Reaches into
 *  rte_dual_channel_t's own pending_state/pending_state_valid fields
 *  directly - same whitebox rationale as force_channel_to_full() above:
 *  this specific interleaving (one frame already staged, a second still
 *  on the wire) cannot be produced through the public API alone with
 *  this test file's single-slot-per-link mock OSAdapter, since a normal
 *  sweep always collapses every currently-mailboxed frame down to just
 *  the last one processed. */
static void test_execute_drains_multiple_buffered_state_frames(void)
{
    fixture_t              fx;
    rte_dual_channel_t    channel_a;
    rte_dual_channel_t    channel_b;
    rte_dual_negotiator_t negotiator_a;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    force_channel_to_full(&channel_a);
    init_negotiator(&negotiator_a, &channel_a, 1U, 2U, 1000U);

    /* Frame 1: pre-staged directly as already-pending, standing in for a
     * frame this same channel already picked up as a side effect of an
     * earlier DATA-path poll this round, before execute() was called. */
    (void)memset(&channel_a.pending_state, 0, sizeof(channel_a.pending_state));
    channel_a.pending_state.state            = (uint8_t)RTE_DUAL_STATE_ONLINE;
    channel_a.pending_state.channel_degraded = 0U;
    channel_a.pending_state.timestamp_ms     = 111U;
    channel_a.pending_state_valid            = true;

    /* Frame 2: a real, still-unread frame sent from B straight onto the
     * wire (channel_b need not itself be negotiator-driven for this). */
    assert(rte_dual_channel_send_state_frame(&channel_b, RTE_DUAL_STATE_HOTSTANDBY, true, 222U) == RTE_STATUS_OK);

    assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);

    /* The second (real, wire) frame is the one negotiator_a ends up
     * having last processed - its own peer_channel_degraded reflects
     * frame 2, not frame 1. */
    assert(negotiator_a.peer_channel_degraded == true);
    assert(negotiator_a.peer_startup_timestamp_ms == 222U);
}

static void test_startup_negotiation_decides_online_and_standby(void)
{
    fixture_t              fx;
    rte_dual_channel_t    channel_a;
    rte_dual_channel_t    channel_b;
    rte_dual_negotiator_t negotiator_a;
    rte_dual_negotiator_t negotiator_b;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    /* A's own redundancy is full - B (the standby side, once decided)
     * should come out HOTSTANDBY, not COLDSTANDBY, from this. */
    force_channel_to_full(&channel_a);

    /* A is init()ed (and so gets its fixed startup timestamp) before B -
     * A's timestamp is strictly smaller -> A wins the tie-break -> A
     * becomes ONLINE, B becomes STANDBY. */
    init_negotiator(&negotiator_a, &channel_a, 1U, 2U, 1000U);
    init_negotiator(&negotiator_b, &channel_b, 2U, 1U, 1000U);

    assert(rte_dual_negotiator_get_own_state(&negotiator_a) == RTE_DUAL_STATE_IDLE);
    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_IDLE);

    /* Round 1: both send their own beacon (own_state still IDLE at send
     * time - that's fine, the *receiver* decides from the timestamp/id
     * carried, not from the sender's self-reported state field). */
    assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
    assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);

    /* Round 2: each now receives the other's round-1 beacon and decides. */
    assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
    assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);

    assert(rte_dual_negotiator_get_own_state(&negotiator_a) == RTE_DUAL_STATE_ONLINE);
    assert(rte_dual_negotiator_get_peer_state(&negotiator_a) == RTE_DUAL_STATE_HOTSTANDBY);
    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_HOTSTANDBY);
    assert(rte_dual_negotiator_get_peer_state(&negotiator_b) == RTE_DUAL_STATE_ONLINE);
}

static void test_peer_degraded_yields_coldstandby(void)
{
    fixture_t              fx;
    rte_dual_channel_t    channel_a;
    rte_dual_channel_t    channel_b;
    rte_dual_negotiator_t negotiator_a;
    rte_dual_negotiator_t negotiator_b;
    uint32_t                i;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    force_channel_to_full(&channel_a);
    init_negotiator(&negotiator_a, &channel_a, 1U, 2U, 1000U);
    init_negotiator(&negotiator_b, &channel_b, 2U, 1U, 1000U);

    /* Negotiate to a decided state first (A ONLINE, B HOTSTANDBY). */
    for (i = 0U; i < 2U; i++)
    {
        assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
        assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);
    }
    assert(rte_dual_negotiator_get_own_state(&negotiator_a) == RTE_DUAL_STATE_ONLINE);
    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_HOTSTANDBY);

    /* A's own DualChannel degrades (simulate its one link going down).
     * Forced directly via force_channel_to_down() rather than a real
     * un-ACKed rte_dual_channel_send(): that would still succeed in
     * transmitting the DATA frame (the mock netlink OSAdapter never
     * fails a send, only the wait-for-ACK loop times out), consuming
     * a Layer-1 sequence number on the same link STATE beacons travel
     * on and desyncing it from channel_b's expected_sequence - every
     * STATE frame after that point would then fail Layer-1 sequence
     * verification and never reach the negotiator at all. */
    force_channel_to_down(&channel_a);
    assert(rte_dual_channel_get_status(&channel_a) != RTE_DUAL_CHANNEL_STATUS_FULL);

    /* Next negotiation round: A's own beacon now carries channel_degraded=1;
     * once B processes it, B (the standby side) downgrades from
     * HOTSTANDBY to COLDSTANDBY. */
    assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
    assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);

    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_COLDSTANDBY);
    assert(rte_dual_negotiator_get_peer_state(&negotiator_a) == RTE_DUAL_STATE_COLDSTANDBY);
}

static void test_lost_peer_contact_degrades_to_unknown(void)
{
    fixture_t              fx;
    rte_dual_channel_t    channel_a;
    rte_dual_channel_t    channel_b;
    rte_dual_negotiator_t negotiator_a;
    rte_dual_negotiator_t negotiator_b;
    uint32_t                i;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    force_channel_to_full(&channel_a);
    /* Short peer_lost_timeout_ms (20ms) - the mock clock advances 10ms
     * per rte_timer_now() call, so a handful of further execute()
     * calls with no incoming beacon reliably exceeds it. */
    init_negotiator(&negotiator_a, &channel_a, 1U, 2U, 20U);
    init_negotiator(&negotiator_b, &channel_b, 2U, 1U, 20U);

    for (i = 0U; i < 2U; i++)
    {
        assert(rte_dual_negotiator_execute(&negotiator_a, 5U) == RTE_STATUS_OK);
        assert(rte_dual_negotiator_execute(&negotiator_b, 5U) == RTE_STATUS_OK);
    }
    assert(rte_dual_negotiator_get_own_state(&negotiator_a) == RTE_DUAL_STATE_ONLINE);
    assert(rte_dual_negotiator_get_own_state(&negotiator_b) == RTE_DUAL_STATE_HOTSTANDBY);

    /* B stops executing entirely (simulates B's process being gone) -
     * only A keeps calling execute(), its own beacon send finding no
     * peer beacon waiting each time, letting last_peer_seen_ms fall far
     * enough behind config.peer_lost_timeout_ms. */
    for (i = 0U; i < 10U; i++)
    {
        assert(rte_dual_negotiator_execute(&negotiator_a, 0U) == RTE_STATUS_OK);
    }

    /* A's peer view degrades to UNKNOWN... */
    assert(rte_dual_negotiator_get_peer_state(&negotiator_a) == RTE_DUAL_STATE_UNKNOWN);
    /* ...but A's own state, already ONLINE, stays ONLINE (ADR-020
     * section 3 / rte_dual_negotiator_execute()'s own doc: an active
     * instance keeps acting ONLINE without needing continuous peer
     * confirmation). */
    assert(rte_dual_negotiator_get_own_state(&negotiator_a) == RTE_DUAL_STATE_ONLINE);
}

int main(void)
{
    assert(rte_checksum_crc64_init(RTE_CRC64_ERTMS) == RTE_STATUS_OK);
    assert(rte_osadapter_netlink_register(&g_mock_netlink_osadapter) == RTE_STATUS_OK);
    assert(rte_osadapter_timer_register(&g_mock_timer_osadapter) == RTE_STATUS_OK);

    test_invalid_params();
    test_startup_negotiation_decides_online_and_standby();
    test_peer_degraded_yields_coldstandby();
    test_lost_peer_contact_degrades_to_unknown();
    test_state_change_callback_fires_on_change();
    test_tie_break_falls_back_to_id_on_exact_timestamp_tie();
    test_lost_peer_contact_degrades_non_online_own_state_too();
    test_execute_drains_multiple_buffered_state_frames();
    return 0;
}
