/* Tests for rte_safechannel_t (ADR-022): the factory itself, not a
 * re-test of rte_dual_channel/rte_channel's own wire semantics
 * (already covered by tests/dual/ and would-be tests/channel_link/).
 * Covers: parameter validation (rejected before the netlink OSAdapter is
 * ever touched), DUAL_REDUNDANT open/send/receive/close wiring (using
 * the same pre-seed-then-one-call technique tests/dual/test_rte_dual_channel.c
 * established, since a live two-sided round trip needs two real threads),
 * and VITAL_VOTED open/send/receive/close wiring (plain self-loopback,
 * since vital_channel imposes no framing at this layer).
 */
#include <assert.h>
#include <string.h>

#include "safeapi/redundancy/safechannel/rte_safechannel.h"
#include "safeapi_osadapter/netlink/rte_osadapter_netlink.h"

typedef struct
{
    uint8_t buf[sizeof(rte_vital_message_t)];
    size_t  size;
    int     has_data;
} mock_mailbox_t;

/** inbox/outbox, not a single shared slot: rte_safechannel_send()'s own
 *  transmission must not overwrite whatever a test pre-seeded as "the
 *  peer's" reply - same reason tests/dual/test_rte_dual_channel.c's
 *  mock_link_t keeps the two directions separate. g_a_side[i] is the
 *  handle rte_safechannel_open() actually gets (via mock_open());
 *  g_b_side[i] is the cross-wired "peer" handle test helpers use to
 *  seed/inspect traffic. */
typedef struct
{
    mock_mailbox_t *inbox;
    mock_mailbox_t *outbox;
} mock_link_t;

#define MOCK_LINK_POOL_SIZE 4U
static mock_mailbox_t g_mailbox_a_to_b[MOCK_LINK_POOL_SIZE];
static mock_mailbox_t g_mailbox_b_to_a[MOCK_LINK_POOL_SIZE];
static mock_link_t    g_a_side[MOCK_LINK_POOL_SIZE];
static mock_link_t    g_b_side[MOCK_LINK_POOL_SIZE];
static uint32_t       g_mock_links_used = 0U;

/** 0-based endpoint index at which mock_open() should fail with
 *  RTE_STATUS_TIMEOUT instead of succeeding - UINT32_MAX (never) unless
 *  a test overrides it, to exercise safechannel_open_dual()'s/
 *  safechannel_open_vital()'s own partial-open unwind path (a later
 *  endpoint failing after earlier ones already succeeded). */
static uint32_t g_mock_open_fail_at_index = 0xFFFFFFFFU;

/** 0-based endpoint index at which mock_open() should report success but
 *  hand back a NULL handle - UINT32_MAX (never) unless a test overrides
 *  it. A conforming OSAdapter never does this; it exists purely to force
 *  rte_dual_channel_init()/rte_channel_init() to see a NULL
 *  link after every endpoint has otherwise "opened successfully", the
 *  only way to reach safeApi_safechannel.c's own post-open init-failure
 *  unwind path without a real protocol-level init failure available at
 *  this layer. */
static uint32_t g_mock_open_null_handle_at_index = 0xFFFFFFFFU;

static void mock_links_reset(void)
{
    uint32_t i;

    (void)memset(g_mailbox_a_to_b, 0, sizeof(g_mailbox_a_to_b));
    (void)memset(g_mailbox_b_to_a, 0, sizeof(g_mailbox_b_to_a));
    for (i = 0U; i < MOCK_LINK_POOL_SIZE; i++)
    {
        g_a_side[i].outbox = &g_mailbox_a_to_b[i];
        g_a_side[i].inbox  = &g_mailbox_b_to_a[i];
        g_b_side[i].outbox = &g_mailbox_b_to_a[i];
        g_b_side[i].inbox  = &g_mailbox_a_to_b[i];
    }
    g_mock_links_used              = 0U;
    g_mock_open_fail_at_index      = 0xFFFFFFFFU;
    g_mock_open_null_handle_at_index = 0xFFFFFFFFU;
}

static rte_status_t mock_open(rte_netlink_storage_t *storage, const rte_netlink_config_t *config,
                                rte_netlink_handle_t *out_handle)
{
    (void)storage;
    (void)config;
    if (g_mock_links_used >= MOCK_LINK_POOL_SIZE)
    {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }
    if (g_mock_links_used == g_mock_open_fail_at_index)
    {
        return RTE_STATUS_TIMEOUT;
    }
    if (g_mock_links_used == g_mock_open_null_handle_at_index)
    {
        *out_handle = NULL;
        g_mock_links_used++;
        return RTE_STATUS_OK;
    }
    *out_handle = (rte_netlink_handle_t)&g_a_side[g_mock_links_used];
    g_mock_links_used++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_close(rte_netlink_handle_t handle)
{
    (void)handle;
    return RTE_STATUS_OK;
}

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

static const rte_osadapter_netlink_t g_mock_osadapter = { mock_open, mock_send, mock_receive, mock_close };

/** Moves whatever the channel-under-test's own send() just transmitted
 *  into its own inbox, for a pure self-loopback test (VITAL_VOTED has no
 *  framing/ACK protocol at this layer, so a real "peer" stand-in isn't
 *  needed - just prove the backend_send/backend_recv bridge works). */
static void mock_loopback_copy(uint32_t index)
{
    g_mailbox_b_to_a[index] = g_mailbox_a_to_b[index];
}

/** Initializes a "peer stand-in" msgchannel over the b-side link. Must be
 *  reused (not re-initialized) across consecutive seed_ack()/seed_data()
 *  calls on the same link within one test, since the real receiver's own
 *  expected_sequence keeps advancing across every frame it accepts
 *  (ACK or DATA alike, rte_dual_msgchannel_receive() does not
 *  distinguish frame kind) - a fresh seed instance would restart at
 *  sequence 0 and fail that continuity check. */
static void seed_init(rte_dual_msgchannel_t *seed, mock_link_t *link)
{
    rte_dual_msgchannel_config_t seed_cfg;

    seed_cfg.link             = (rte_netlink_handle_t)link;
    seed_cfg.sender_id        = 2U;
    seed_cfg.expected_peer_id = 1U;
    assert(rte_dual_msgchannel_init(seed, &seed_cfg) == RTE_STATUS_OK);
}

/** Seeds an ACK frame (as "the peer" would have sent it) directly into
 *  the b-side link's mailbox - same technique as
 *  tests/dual/test_rte_dual_channel.c's seed_ack(). */
static void seed_ack(rte_dual_msgchannel_t *seed, uint32_t acked_sequence)
{
    rte_dual_ack_frame_t ack;

    ack.header.kind        = (uint8_t)RTE_DUAL_FRAME_KIND_ACK;
    ack.header.reserved[0] = 0U;
    ack.header.reserved[1] = 0U;
    ack.header.reserved[2] = 0U;
    ack.acked_sequence     = acked_sequence;
    assert(rte_dual_msgchannel_send(seed, (const uint8_t *)&ack, (uint8_t)sizeof(ack), 10U, NULL) == RTE_STATUS_OK);
}

/** Seeds a raw DATA frame (as "the peer" would have sent it). */
static void seed_data(rte_dual_msgchannel_t *seed, const uint8_t *payload, uint8_t payload_size)
{
    rte_dual_frame_header_t header;
    uint8_t                  frame[sizeof(header) + 255U];

    header.kind        = (uint8_t)RTE_DUAL_FRAME_KIND_DATA;
    header.reserved[0] = 0U;
    header.reserved[1] = 0U;
    header.reserved[2] = 0U;
    (void)memcpy(frame, &header, sizeof(header));
    (void)memcpy(&frame[sizeof(header)], payload, payload_size);
    assert(rte_dual_msgchannel_send(seed, frame, (uint8_t)(sizeof(header) + payload_size), 10U, NULL)
           == RTE_STATUS_OK);
}

static void test_open_rejects_bad_params(void)
{
    rte_safechannel_t       channel;
    rte_safechannel_config_t cfg;

    mock_links_reset();
    memset(&cfg, 0, sizeof(cfg));

    assert(rte_safechannel_open(NULL, &cfg) == RTE_STATUS_INVALID_PARAM);
    assert(rte_safechannel_open(&channel, NULL) == RTE_STATUS_INVALID_PARAM);

    /* Unrecognized type. */
    cfg.type = (rte_safechannel_type_t)99;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);

    /* DUAL_REDUNDANT with link_count == 0. */
    cfg.type              = RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT;
    cfg.as.dual.link_count = 0U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);

    /* DUAL_REDUNDANT with too many links. */
    cfg.as.dual.link_count = RTE_SAFECHANNEL_MAX_LINKS + 1U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);

    /* CONNECT-role endpoint with NULL host. */
    cfg.as.dual.link_count           = 1U;
    cfg.as.dual.endpoints[0].role    = RTE_NETLINK_ROLE_CONNECT;
    cfg.as.dual.endpoints[0].host    = NULL;
    cfg.as.dual.endpoints[0].port    = 9000U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);

    /* None of the above should have ever reached the netlink OSAdapter. */
    assert(g_mock_links_used == 0U);
}

static void test_dual_open_send_receive_close(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;
    rte_dual_msgchannel_t     seed;
    size_t                     out_size = 0U;
    uint8_t                    out_payload[32];
    uint32_t                   status;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);
    seed_init(&seed, &g_b_side[0]);

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                       = RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT;
    cfg.as.dual.link_count          = 1U;
    cfg.as.dual.endpoints[0].role   = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[0].host   = NULL;
    cfg.as.dual.endpoints[0].port   = 9000U;
    cfg.as.dual.sender_id           = 1U;
    cfg.as.dual.expected_peer_id    = 2U;
    cfg.as.dual.connect_timeout_ms  = 100U;
    cfg.as.dual.ack_timeout_ms      = 10U;

    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_OK);
    assert(g_mock_links_used == 1U);
    assert(rte_safechannel_get_status(&channel) == RTE_SAFECHANNEL_LINK_DOWN);

    /* send(): pre-seed the ACK the "peer" would reply with (transport
     * sequence 0), then send. */
    seed_ack(&seed, 0U);
    assert(rte_safechannel_send(&channel, (const uint8_t *)"AB_SAMPLE", 9U) == RTE_STATUS_OK);
    assert(rte_safechannel_get_status(&channel) == RTE_SAFECHANNEL_LINK_FULL);

    /* receive(): pre-seed a DATA frame "from the peer" using the SAME
     * seed instance (now at transport sequence 1) so it satisfies the
     * real receiver's expected_sequence continuity check. */
    seed_data(&seed, (const uint8_t *)"hello", 5U);
    assert(rte_safechannel_receive(&channel, out_payload, sizeof(out_payload), 5U, &out_size) == RTE_STATUS_OK);
    assert(out_size == 5U);
    assert(memcmp(out_payload, "hello", 5U) == 0);

    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
    /* Idempotent second close. */
    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
    (void)status;
}

/** safechannel_open_dual()'s own partial-open unwind path: the first
 *  endpoint opens fine, the second (of two) fails - every endpoint
 *  opened so far must be closed again (all-or-nothing) and the failure
 *  status propagated as-is. */
static void test_dual_open_partial_endpoint_failure_unwinds(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);
    g_mock_open_fail_at_index = 1U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                      = RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT;
    cfg.as.dual.link_count         = 2U;
    cfg.as.dual.endpoints[0].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[0].port  = 9010U;
    cfg.as.dual.endpoints[1].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[1].port  = 9011U;
    cfg.as.dual.connect_timeout_ms = 100U;
    cfg.as.dual.ack_timeout_ms     = 10U;

    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_TIMEOUT);
    assert(channel.is_open == false);
}

/** Forces rte_dual_channel_init() itself to fail after every configured
 *  endpoint has already "opened successfully" (a misbehaving OSAdapter
 *  handing back a NULL handle - see g_mock_open_null_handle_at_index's
 *  own doc), exercising safechannel_open_dual()'s own post-open unwind
 *  path (distinct from test_dual_open_partial_endpoint_failure_unwinds()'s
 *  own earlier, open()-itself-fails path). */
static void test_dual_open_dual_channel_init_failure_unwinds(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);
    g_mock_open_null_handle_at_index = 0U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                      = RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT;
    cfg.as.dual.link_count         = 1U;
    cfg.as.dual.endpoints[0].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[0].port  = 9012U;
    cfg.as.dual.connect_timeout_ms = 100U;
    cfg.as.dual.ack_timeout_ms     = 10U;

    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);
    assert(channel.is_open == false);
}

static void test_vital_open_rejects_bad_params(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;

    mock_links_reset();
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = RTE_SAFECHANNEL_TYPE_VITAL_VOTED;

    /* message_size == 0. */
    cfg.as.vital.link_count   = 1U;
    cfg.as.vital.message_size = 0U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);

    /* CONNECT-role endpoint with NULL host. */
    cfg.as.vital.message_size      = 16U;
    cfg.as.vital.endpoints[0].role = RTE_NETLINK_ROLE_CONNECT;
    cfg.as.vital.endpoints[0].host = NULL;
    cfg.as.vital.endpoints[0].port = 9020U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);

    assert(g_mock_links_used == 0U);
}

/** Mirrors test_dual_open_partial_endpoint_failure_unwinds() for the
 *  VITAL_VOTED path: the first (of two) endpoints opens fine, the second
 *  fails - every endpoint opened so far must be closed again. */
static void test_vital_open_partial_endpoint_failure_unwinds(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);
    g_mock_open_fail_at_index = 1U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                       = RTE_SAFECHANNEL_TYPE_VITAL_VOTED;
    cfg.as.vital.link_count         = 2U;
    cfg.as.vital.message_size       = 16U;
    cfg.as.vital.voting_strategy    = RTE_VOTING_2OO2;
    cfg.as.vital.endpoints[0].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[0].port  = 9021U;
    cfg.as.vital.endpoints[1].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[1].port  = 9022U;
    cfg.as.vital.connect_timeout_ms = 100U;
    cfg.as.vital.channel_timeout_ms = 100U;

    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_TIMEOUT);
    assert(channel.is_open == false);
}

/** Forces rte_voter_init() itself to fail after every endpoint has
 *  already opened successfully: RTE_VOTING_NMR with quorum_size == 0
 *  is a shape rte_voter_init() itself rejects (ADR-025), exercising
 *  safechannel_open_vital()'s own post-link-open unwind path. (Unlike
 *  the pre-redesign rte_vital_channel_init(), the current
 *  rte_voter_init()/rte_channel_init() no longer validate a
 *  link_count-vs-strategy shape mismatch up front - that only surfaces
 *  later, at vote time, as INSUFFICIENT_QUORUM - so this test now
 *  targets the one remaining way to fail after the links are open.) */
static void test_vital_open_voter_init_failure_unwinds(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                       = RTE_SAFECHANNEL_TYPE_VITAL_VOTED;
    cfg.as.vital.link_count         = 2U;
    cfg.as.vital.message_size       = 16U;
    cfg.as.vital.voting_strategy    = RTE_VOTING_NMR;
    cfg.as.vital.quorum_size        = 0U; /* invalid for NMR - rejected by rte_voter_init() */
    cfg.as.vital.endpoints[0].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[0].port  = 9023U;
    cfg.as.vital.endpoints[1].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[1].port  = 9024U;
    cfg.as.vital.connect_timeout_ms = 100U;
    cfg.as.vital.channel_timeout_ms = 100U;

    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_INVALID_PARAM);
    assert(channel.is_open == false);
    assert(g_mock_links_used == 2U);
}

static void test_send_receive_reject_null_or_unopened_channel(void)
{
    rte_safechannel_t channel;
    uint8_t             out_payload[8];
    size_t              out_size = 0U;

    memset(&channel, 0, sizeof(channel));

    assert(rte_safechannel_send(NULL, (const uint8_t *)"a", 1U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_safechannel_send(&channel, (const uint8_t *)"a", 1U) == RTE_STATUS_INVALID_PARAM);

    assert(rte_safechannel_receive(&channel, out_payload, sizeof(out_payload), 0U, &out_size)
           == RTE_STATUS_INVALID_PARAM);
}

static void test_dual_send_rejects_oversized_payload(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;
    uint8_t                    oversize_payload[300];

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);
    (void)memset(oversize_payload, 0, sizeof(oversize_payload));

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                      = RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT;
    cfg.as.dual.link_count         = 1U;
    cfg.as.dual.endpoints[0].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[0].port  = 9013U;
    cfg.as.dual.sender_id          = 1U;
    cfg.as.dual.expected_peer_id   = 2U;
    cfg.as.dual.connect_timeout_ms = 100U;
    cfg.as.dual.ack_timeout_ms     = 10U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_OK);

    /* payload_size (300) exceeds UINT8_MAX, the hard cap this facade can
     * ever forward to rte_dual_channel_send(). */
    assert(rte_safechannel_send(&channel, oversize_payload, sizeof(oversize_payload)) == RTE_STATUS_INVALID_PARAM);

    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
}

static void test_dual_receive_clamps_oversized_max_size(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;
    rte_dual_msgchannel_t     seed;
    uint8_t                    out_payload[300];
    size_t                     out_size = 0U;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);
    seed_init(&seed, &g_b_side[0]);

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                      = RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT;
    cfg.as.dual.link_count         = 1U;
    cfg.as.dual.endpoints[0].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[0].port  = 9014U;
    cfg.as.dual.sender_id          = 1U;
    cfg.as.dual.expected_peer_id   = 2U;
    cfg.as.dual.connect_timeout_ms = 100U;
    cfg.as.dual.ack_timeout_ms     = 10U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_OK);

    seed_data(&seed, (const uint8_t *)"hi", 2U);
    /* max_size (300) exceeds UINT8_MAX - internally clamped down to 255
     * rather than passed straight through to rte_dual_channel_receive(),
     * which takes its own max_size as a uint8_t. */
    assert(rte_safechannel_receive(&channel, out_payload, sizeof(out_payload), 5U, &out_size) == RTE_STATUS_OK);
    assert(out_size == 2U);
    assert(memcmp(out_payload, "hi", 2U) == 0);

    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
}

/** Drives a real rte_safechannel_t (DUAL_REDUNDANT, 2 links) to
 *  RTE_SAFECHANNEL_LINK_DEGRADED: only one of the two links' own ACK is
 *  seeded before send(), so the underlying rte_dual_channel_t's own
 *  aggregate status comes out DEGRADED, not FULL/DOWN - same technique
 *  tests/dual/test_rte_dual_channel.c's own
 *  test_send_one_link_acks_degraded() uses, just through the facade. */
static void test_dual_status_degraded(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;
    rte_dual_msgchannel_t     seed0;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                      = RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT;
    cfg.as.dual.link_count         = 2U;
    cfg.as.dual.endpoints[0].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[0].port  = 9015U;
    cfg.as.dual.endpoints[1].role  = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.dual.endpoints[1].port  = 9016U;
    cfg.as.dual.sender_id          = 1U;
    cfg.as.dual.expected_peer_id   = 2U;
    cfg.as.dual.connect_timeout_ms = 100U;
    cfg.as.dual.ack_timeout_ms     = 10U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_OK);
    assert(g_mock_links_used == 2U);

    /* Only link0's own ACK is seeded - link1's send still "goes out" but
     * nothing answers it within this one call. */
    seed_init(&seed0, &g_b_side[0]);
    seed_ack(&seed0, 0U);

    assert(rte_safechannel_send(&channel, (const uint8_t *)"x", 1U) == RTE_STATUS_OK);
    assert(rte_safechannel_get_status(&channel) == RTE_SAFECHANNEL_LINK_DEGRADED);

    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
}

/** Exercises rte_safechannel_get_status()'s own VITAL_VOTED DOWN/
 *  DEGRADED branches, driving each individual rte_channel_t's health
 *  through the real public rte_channel_set_healthy() API (ADR-025) -
 *  unlike the pre-redesign vital_channel, each registered channel is
 *  now independently reachable and has its own public health setter,
 *  so no whitebox struct-poking is needed here anymore. */
static void test_vital_status_down_and_degraded(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);

    memset(&cfg, 0, sizeof(cfg));
    cfg.type                        = RTE_SAFECHANNEL_TYPE_VITAL_VOTED;
    cfg.as.vital.link_count          = 2U;
    cfg.as.vital.message_size        = 16U;
    cfg.as.vital.voting_strategy     = RTE_VOTING_2OO2;
    cfg.as.vital.endpoints[0].role   = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[0].port   = 9025U;
    cfg.as.vital.endpoints[1].role   = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[1].port   = 9026U;
    cfg.as.vital.connect_timeout_ms  = 100U;
    cfg.as.vital.channel_timeout_ms  = 100U;
    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_OK);

    /* Both channels unhealthy -> DOWN. */
    assert(rte_channel_set_healthy(&channel.impl.vital.channels[0], false) == RTE_STATUS_OK);
    assert(rte_channel_set_healthy(&channel.impl.vital.channels[1], false) == RTE_STATUS_OK);
    assert(rte_safechannel_get_status(&channel) == RTE_SAFECHANNEL_LINK_DOWN);

    /* One of two unhealthy -> DEGRADED. */
    assert(rte_channel_set_healthy(&channel.impl.vital.channels[0], true) == RTE_STATUS_OK);
    assert(rte_safechannel_get_status(&channel) == RTE_SAFECHANNEL_LINK_DEGRADED);

    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
}

static void test_vital_open_send_receive_close(void)
{
    rte_safechannel_t        channel;
    rte_safechannel_config_t cfg;
    size_t                     out_size = 0U;
    uint8_t                    out_payload[16];

    mock_links_reset();
    (void)rte_osadapter_netlink_register(&g_mock_osadapter);

    /* rte_channel_receive() itself hard-requires at least 2
     * successful per-channel receives to ever return RTE_STATUS_OK
     * (voting needs something to compare) - channel_count == 1 is only
     * a legitimate shape for rte_channel_checkpoint()'s direct
     * backend_send/backend_recv use (rte_channel_init()'s own
     * doc), not for going through _receive() itself. Use 2 links with
     * 2oo2 voting here so this test exercises the real receive path. */
    memset(&cfg, 0, sizeof(cfg));
    cfg.type                        = RTE_SAFECHANNEL_TYPE_VITAL_VOTED;
    cfg.as.vital.link_count          = 2U;
    cfg.as.vital.message_size        = 16U;
    cfg.as.vital.voting_strategy     = RTE_VOTING_2OO2;
    cfg.as.vital.endpoints[0].role   = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[0].host   = NULL;
    cfg.as.vital.endpoints[0].port   = 9001U;
    cfg.as.vital.endpoints[1].role   = RTE_NETLINK_ROLE_LISTEN;
    cfg.as.vital.endpoints[1].host   = NULL;
    cfg.as.vital.endpoints[1].port   = 9002U;
    cfg.as.vital.connect_timeout_ms  = 100U;
    cfg.as.vital.channel_timeout_ms  = 100U;

    assert(rte_safechannel_open(&channel, &cfg) == RTE_STATUS_OK);
    assert(g_mock_links_used == 2U);

    assert(rte_safechannel_send(&channel, (const uint8_t *)"0123456789ABCDEF", 16U) == RTE_STATUS_OK);
    mock_loopback_copy(0U);
    mock_loopback_copy(1U);
    assert(rte_safechannel_receive(&channel, out_payload, sizeof(out_payload), 0U, &out_size) == RTE_STATUS_OK);
    assert(out_size == 16U);
    assert(memcmp(out_payload, "0123456789ABCDEF", 16U) == 0);
    assert(rte_safechannel_get_status(&channel) == RTE_SAFECHANNEL_LINK_FULL);

    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
}

static void test_close_on_unopened_is_safe(void)
{
    rte_safechannel_t channel;

    memset(&channel, 0, sizeof(channel));
    assert(rte_safechannel_close(&channel) == RTE_STATUS_OK);
    assert(rte_safechannel_close(NULL) == RTE_STATUS_OK);
    assert(rte_safechannel_get_status(NULL) == RTE_SAFECHANNEL_LINK_DOWN);
    assert(rte_safechannel_get_status(&channel) == RTE_SAFECHANNEL_LINK_DOWN);
}

int main(void)
{
    test_open_rejects_bad_params();
    test_dual_open_send_receive_close();
    test_dual_open_partial_endpoint_failure_unwinds();
    test_dual_open_dual_channel_init_failure_unwinds();
    test_vital_open_rejects_bad_params();
    test_vital_open_partial_endpoint_failure_unwinds();
    test_vital_open_voter_init_failure_unwinds();
    test_send_receive_reject_null_or_unopened_channel();
    test_dual_send_rejects_oversized_payload();
    test_dual_receive_clamps_oversized_max_size();
    test_dual_status_degraded();
    test_vital_status_down_and_degraded();
    test_vital_open_send_receive_close();
    test_close_on_unopened_is_safe();
    return 0;
}
