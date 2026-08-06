/* Tests for sapi_dual_negotiator_t (ADR-020 section 3): own/peer state
 * tracking, the older-startup-timestamp-wins tie-break, HOTSTANDBY vs
 * COLDSTANDBY from the peer's own reported channel_degraded bit, and
 * degradation to UNKNOWN on lost peer contact (with ONLINE's own
 * exception). Two live negotiators (A and B) share one pair of
 * sapi_dual_channel_t instances over the same mock netlink backend used
 * by the other dual/ tests - unlike DATA's ACK-wait, STATE beacons are
 * fire-and-forget, so a live two-call round trip (A's own
 * sapi_dual_negotiator_execute(), then B's) has no timing race to avoid. */
#include <assert.h>
#include <string.h>

#include "safeapi/checksum/sapi_checksum.h"
#include "safeapi/dual/sapi_dual_negotiator.h"
#include "safeapi/timer/sapi_timer.h"
#include "safeapi_backend/timer/sapi_timer_backend.h"
#include "safeapi_backend/netlink/sapi_netlink_backend.h"

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
} mock_link_t;

static sapi_status_t mock_send(sapi_netlink_handle_t handle, const void *message, size_t message_size,
                                sapi_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)handle;

    (void)timeout_ms;
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
    if (!link->inbox->has_data)
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

/* --- mock timer: increments by 10ms every call, so whichever negotiator
 *     is init()ed first gets the earlier (smaller) startup timestamp -
 *     deterministic and lets the "older timestamp wins" branch of the
 *     tie-break be exercised directly, not just the own_id fallback. --- */
static uint64_t g_mock_clock_ms = 0U;

static sapi_status_t mock_timer_now(sapi_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = g_mock_clock_ms;
    g_mock_clock_ms += 10U;
    return SAPI_STATUS_OK;
}

static const sapi_timer_backend_t g_mock_timer_backend = { NULL, NULL, NULL, NULL, mock_timer_now };

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

static void init_dual_channel(sapi_dual_channel_t *channel, mock_link_t *link, uint32_t sender_id,
                               uint32_t expected_peer_id)
{
    sapi_dual_channel_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.links[0]         = (sapi_netlink_handle_t)link;
    cfg.link_count       = 1U;
    cfg.sender_id        = sender_id;
    cfg.expected_peer_id = expected_peer_id;
    cfg.ack_timeout_ms   = 10U;
    assert(sapi_dual_channel_init(channel, &cfg) == SAPI_STATUS_OK);
}

static void init_negotiator(sapi_dual_negotiator_t *negotiator, sapi_dual_channel_t *channel, uint32_t own_id,
                             uint32_t peer_id, sapi_duration_ms_t peer_lost_timeout_ms)
{
    sapi_dual_negotiator_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.channel               = channel;
    cfg.own_id                = own_id;
    cfg.peer_id                = peer_id;
    cfg.peer_lost_timeout_ms  = peer_lost_timeout_ms;
    assert(sapi_dual_negotiator_init(negotiator, &cfg) == SAPI_STATUS_OK);
}

/** Forces a channel's own sapi_dual_channel_get_status() to FULL for
 *  test setup, without going through a real DATA send/ACK round trip -
 *  deliberately avoided here because that would advance the same
 *  per-link Layer-1 sequence counter sapi_dual_negotiator_t's STATE
 *  beacons travel on, desynchronizing it from the peer's own
 *  expected_sequence before negotiation even starts. sapi_dual_channel_t
 *  has no public "just report FULL" setter (by design - status is
 *  meant to be an observed fact, not something a caller injects), so
 *  this test reaches into the struct directly; acceptable here since
 *  C's structs have no real encapsulation and this is whitebox test
 *  setup, not production code exercising the public API. A freshly-
 *  init()ed sapi_dual_channel_t otherwise starts at DOWN (degraded),
 *  and this test file's negotiation scenarios need to distinguish a
 *  genuinely non-degraded HOTSTANDBY-yielding channel from that
 *  always-degraded-by-default starting condition. */
static void force_channel_to_full(sapi_dual_channel_t *channel)
{
    uint32_t i;

    for (i = 0U; i < channel->link_count; i++)
    {
        channel->link_up[i] = true;
    }
    channel->last_status = SAPI_DUAL_CHANNEL_STATUS_FULL;
}

/** Same whitebox rationale as force_channel_to_full() above, in the
 *  other direction: forces a channel to report DOWN without a real
 *  un-ACKed sapi_dual_channel_send(), which would consume a Layer-1
 *  sequence number on the shared link and desync it from the peer's
 *  expected_sequence (the peer never receives - and so never advances
 *  past - a DATA frame that nobody ACKs), corrupting every STATE
 *  beacon exchanged afterwards. */
static void force_channel_to_down(sapi_dual_channel_t *channel)
{
    uint32_t i;

    for (i = 0U; i < channel->link_count; i++)
    {
        channel->link_up[i] = false;
    }
    channel->last_status = SAPI_DUAL_CHANNEL_STATUS_DOWN;
}

static void test_invalid_params(void)
{
    fixture_t             fx;
    sapi_dual_channel_t    channel_a;
    sapi_dual_negotiator_t negotiator_a;
    sapi_dual_negotiator_config_t cfg;

    fixture_init(&fx);
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);

    (void)memset(&cfg, 0, sizeof(cfg));
    assert(sapi_dual_negotiator_init(NULL, &cfg) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_negotiator_init(&negotiator_a, NULL) == SAPI_STATUS_INVALID_PARAM);
    cfg.channel = NULL;
    assert(sapi_dual_negotiator_init(&negotiator_a, &cfg) == SAPI_STATUS_INVALID_PARAM);

    /* Defensive NULL accessors. */
    assert(sapi_dual_negotiator_get_own_state(NULL) == SAPI_DUAL_STATE_IDLE);
    assert(sapi_dual_negotiator_get_peer_state(NULL) == SAPI_DUAL_STATE_IDLE);
}

static void test_startup_negotiation_decides_online_and_standby(void)
{
    fixture_t              fx;
    sapi_dual_channel_t    channel_a;
    sapi_dual_channel_t    channel_b;
    sapi_dual_negotiator_t negotiator_a;
    sapi_dual_negotiator_t negotiator_b;

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

    assert(sapi_dual_negotiator_get_own_state(&negotiator_a) == SAPI_DUAL_STATE_IDLE);
    assert(sapi_dual_negotiator_get_own_state(&negotiator_b) == SAPI_DUAL_STATE_IDLE);

    /* Round 1: both send their own beacon (own_state still IDLE at send
     * time - that's fine, the *receiver* decides from the timestamp/id
     * carried, not from the sender's self-reported state field). */
    assert(sapi_dual_negotiator_execute(&negotiator_a, 5U) == SAPI_STATUS_OK);
    assert(sapi_dual_negotiator_execute(&negotiator_b, 5U) == SAPI_STATUS_OK);

    /* Round 2: each now receives the other's round-1 beacon and decides. */
    assert(sapi_dual_negotiator_execute(&negotiator_a, 5U) == SAPI_STATUS_OK);
    assert(sapi_dual_negotiator_execute(&negotiator_b, 5U) == SAPI_STATUS_OK);

    assert(sapi_dual_negotiator_get_own_state(&negotiator_a) == SAPI_DUAL_STATE_ONLINE);
    assert(sapi_dual_negotiator_get_peer_state(&negotiator_a) == SAPI_DUAL_STATE_HOTSTANDBY);
    assert(sapi_dual_negotiator_get_own_state(&negotiator_b) == SAPI_DUAL_STATE_HOTSTANDBY);
    assert(sapi_dual_negotiator_get_peer_state(&negotiator_b) == SAPI_DUAL_STATE_ONLINE);
}

static void test_peer_degraded_yields_coldstandby(void)
{
    fixture_t              fx;
    sapi_dual_channel_t    channel_a;
    sapi_dual_channel_t    channel_b;
    sapi_dual_negotiator_t negotiator_a;
    sapi_dual_negotiator_t negotiator_b;
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
        assert(sapi_dual_negotiator_execute(&negotiator_a, 5U) == SAPI_STATUS_OK);
        assert(sapi_dual_negotiator_execute(&negotiator_b, 5U) == SAPI_STATUS_OK);
    }
    assert(sapi_dual_negotiator_get_own_state(&negotiator_a) == SAPI_DUAL_STATE_ONLINE);
    assert(sapi_dual_negotiator_get_own_state(&negotiator_b) == SAPI_DUAL_STATE_HOTSTANDBY);

    /* A's own DualChannel degrades (simulate its one link going down).
     * Forced directly via force_channel_to_down() rather than a real
     * un-ACKed sapi_dual_channel_send(): that would still succeed in
     * transmitting the DATA frame (the mock netlink backend never
     * fails a send, only the wait-for-ACK loop times out), consuming
     * a Layer-1 sequence number on the same link STATE beacons travel
     * on and desyncing it from channel_b's expected_sequence - every
     * STATE frame after that point would then fail Layer-1 sequence
     * verification and never reach the negotiator at all. */
    force_channel_to_down(&channel_a);
    assert(sapi_dual_channel_get_status(&channel_a) != SAPI_DUAL_CHANNEL_STATUS_FULL);

    /* Next negotiation round: A's own beacon now carries channel_degraded=1;
     * once B processes it, B (the standby side) downgrades from
     * HOTSTANDBY to COLDSTANDBY. */
    assert(sapi_dual_negotiator_execute(&negotiator_a, 5U) == SAPI_STATUS_OK);
    assert(sapi_dual_negotiator_execute(&negotiator_b, 5U) == SAPI_STATUS_OK);

    assert(sapi_dual_negotiator_get_own_state(&negotiator_b) == SAPI_DUAL_STATE_COLDSTANDBY);
    assert(sapi_dual_negotiator_get_peer_state(&negotiator_a) == SAPI_DUAL_STATE_COLDSTANDBY);
}

static void test_lost_peer_contact_degrades_to_unknown(void)
{
    fixture_t              fx;
    sapi_dual_channel_t    channel_a;
    sapi_dual_channel_t    channel_b;
    sapi_dual_negotiator_t negotiator_a;
    sapi_dual_negotiator_t negotiator_b;
    uint32_t                i;

    fixture_init(&fx);
    g_mock_clock_ms = 0U;
    init_dual_channel(&channel_a, &fx.a_link, 1U, 2U);
    init_dual_channel(&channel_b, &fx.b_link, 2U, 1U);
    force_channel_to_full(&channel_a);
    /* Short peer_lost_timeout_ms (20ms) - the mock clock advances 10ms
     * per sapi_timer_now() call, so a handful of further execute()
     * calls with no incoming beacon reliably exceeds it. */
    init_negotiator(&negotiator_a, &channel_a, 1U, 2U, 20U);
    init_negotiator(&negotiator_b, &channel_b, 2U, 1U, 20U);

    for (i = 0U; i < 2U; i++)
    {
        assert(sapi_dual_negotiator_execute(&negotiator_a, 5U) == SAPI_STATUS_OK);
        assert(sapi_dual_negotiator_execute(&negotiator_b, 5U) == SAPI_STATUS_OK);
    }
    assert(sapi_dual_negotiator_get_own_state(&negotiator_a) == SAPI_DUAL_STATE_ONLINE);
    assert(sapi_dual_negotiator_get_own_state(&negotiator_b) == SAPI_DUAL_STATE_HOTSTANDBY);

    /* B stops executing entirely (simulates B's process being gone) -
     * only A keeps calling execute(), its own beacon send finding no
     * peer beacon waiting each time, letting last_peer_seen_ms fall far
     * enough behind config.peer_lost_timeout_ms. */
    for (i = 0U; i < 10U; i++)
    {
        assert(sapi_dual_negotiator_execute(&negotiator_a, 0U) == SAPI_STATUS_OK);
    }

    /* A's peer view degrades to UNKNOWN... */
    assert(sapi_dual_negotiator_get_peer_state(&negotiator_a) == SAPI_DUAL_STATE_UNKNOWN);
    /* ...but A's own state, already ONLINE, stays ONLINE (ADR-020
     * section 3 / sapi_dual_negotiator_execute()'s own doc: an active
     * instance keeps acting ONLINE without needing continuous peer
     * confirmation). */
    assert(sapi_dual_negotiator_get_own_state(&negotiator_a) == SAPI_DUAL_STATE_ONLINE);
}

int main(void)
{
    assert(sapi_checksum_crc64_init(SAPI_CRC64_ERTMS) == SAPI_STATUS_OK);
    assert(sapi_netlink_register_backend(&g_mock_netlink_backend) == SAPI_STATUS_OK);
    assert(sapi_timer_register_backend(&g_mock_timer_backend) == SAPI_STATUS_OK);

    test_invalid_params();
    test_startup_negotiation_decides_online_and_standby();
    test_peer_degraded_yields_coldstandby();
    test_lost_peer_contact_degrades_to_unknown();
    return 0;
}
