/* Tests for the site role service (rte_site_role.h, ADR-040 step 2): two instances, WEST and EAST, over a mock
 * netlink OSAdapter with a small queue per direction, driven one call at a time (single thread). A DATA send waits
 * for an acknowledgement the counterpart can only give when it runs next, so sends here usually time out; frames
 * still arrive, which is what the role logic works on (the link teardown threshold is set high where that matters).
 * Covered: frame layout, startup role STANDBY without negotiation, both-STANDBY tie-break (WEST wins), snapshot
 * receipt and adoption on a counterpart's FAULTED notice, silence never promotes, dispatcher-confirmed takeover,
 * FAULTED flush. */
#include <assert.h>
#include <string.h>

#include "rte/redundancy/checksum/rte_checksum.h"
#include "rte/redundancy/site_role/rte_site_role.h"
#include "rte/oal/timer/rte_timer.h"
#include "rte_osadapter/netlink/rte_osadapter_netlink.h"
#include "rte_osadapter/timer/rte_osadapter_timer.h"

#define QUEUE_DEPTH 32U
#define STATE_SIZE 8U
#define FRAME_SIZE (RTE_SITE_ROLE_FRAME_OVERHEAD + STATE_SIZE)
#define PORT_WEST 1U
#define PORT_EAST 2U

/* ---- mock netlink: one queue per direction ---- */

typedef struct
{
    uint8_t buf[QUEUE_DEPTH][sizeof(rte_vital_message_t)];
    size_t  size[QUEUE_DEPTH];
    size_t  head;
    size_t  count;
} mock_queue_t;

typedef struct
{
    mock_queue_t *inbox;
    mock_queue_t *outbox;
    int           open;
    int           blocked; /* simulate a silent counterpart: sends are dropped */
} mock_link_t;

static mock_queue_t g_west_to_east;
static mock_queue_t g_east_to_west;
static mock_link_t  g_west_link;
static mock_link_t  g_east_link;

static rte_status_t mock_open(rte_netlink_storage_t *storage, const rte_netlink_config_t *config,
                              rte_netlink_handle_t *out_handle)
{
    mock_link_t *link = (config->port == PORT_WEST) ? &g_west_link : &g_east_link;

    (void)storage;
    link->open = 1;
    *out_handle = (rte_netlink_handle_t)(void *)link;
    return RTE_STATUS_OK;
}

static rte_status_t mock_send(rte_netlink_handle_t handle, const void *message, size_t message_size,
                              rte_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)(void *)handle;
    mock_queue_t *q = link->outbox;
    size_t slot;

    (void)timeout_ms;
    if (link->blocked != 0)
    {
        return RTE_STATUS_OK; /* lost on the way */
    }
    if ((q->count >= QUEUE_DEPTH) || (message_size > sizeof(q->buf[0])))
    {
        return RTE_STATUS_TIMEOUT;
    }
    slot = (q->head + q->count) % QUEUE_DEPTH;
    (void)memcpy(q->buf[slot], message, message_size);
    q->size[slot] = message_size;
    q->count++;
    return RTE_STATUS_OK;
}

static rte_status_t mock_receive(rte_netlink_handle_t handle, void *out_message, size_t buffer_size,
                                 rte_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)(void *)handle;
    mock_queue_t *q = link->inbox;

    (void)timeout_ms;
    if (q->count == 0U)
    {
        return RTE_STATUS_TIMEOUT;
    }
    if (q->size[q->head] > buffer_size)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    (void)memcpy(out_message, q->buf[q->head], q->size[q->head]);
    q->head = (q->head + 1U) % QUEUE_DEPTH;
    q->count--;
    return RTE_STATUS_OK;
}

static rte_status_t mock_close(rte_netlink_handle_t handle)
{
    ((mock_link_t *)(void *)handle)->open = 0;
    return RTE_STATUS_OK;
}

static const rte_osadapter_netlink_t g_mock_netlink = { mock_open, mock_send, mock_receive, mock_close };

static uint64_t g_clock_ms = 1000U;

static rte_status_t mock_timer_now(rte_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = g_clock_ms;
    g_clock_ms += 5U;
    return RTE_STATUS_OK;
}

static const rte_osadapter_timer_t g_mock_timer = { NULL, NULL, NULL, NULL, mock_timer_now };

/* ---- application side ---- */

typedef struct
{
    uint8_t                live_state[STATE_SIZE];
    uint8_t                held_snapshot[STATE_SIZE];
    int                    receive_calls;
    int                    changes;
    rte_site_role_change_t last_change;
    uint8_t                tx[FRAME_SIZE];
    uint8_t                rx[FRAME_SIZE];
} app_t;

static rte_status_t app_encode(void *user, uint8_t *out, size_t size, bool live)
{
    app_t *app = (app_t *)user;

    assert(size == STATE_SIZE);
    if (live)
    {
        (void)memcpy(out, app->live_state, size);
    }
    return RTE_STATUS_OK;
}

static bool app_receive(void *user, const uint8_t *in, size_t size, bool valid)
{
    app_t *app = (app_t *)user;

    assert(size == STATE_SIZE);
    app->receive_calls++;
    if (valid)
    {
        (void)memcpy(app->held_snapshot, in, size);
    }
    return true;
}

static void app_change(void *user, const rte_site_role_change_t *change)
{
    app_t *app = (app_t *)user;

    app->changes++;
    app->last_change = *change;
}

static void setup(rte_site_role_t *rs, app_t *app, bool west, uint8_t send_miss_threshold)
{
    rte_site_role_config_t cfg;

    (void)memset(app, 0, sizeof(*app));
    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.own_id = west ? 0U : 1U;
    cfg.peer_id = west ? 1U : 0U;
    cfg.is_west = west;
    cfg.listen = west;
    cfg.peer_host = "peer";
    cfg.port = west ? (uint16_t)PORT_WEST : (uint16_t)PORT_EAST;
    cfg.link_message_size = sizeof(rte_vital_message_t);
    cfg.ack_timeout_ms = 20U;
    cfg.peer_lost_timeout_ms = 10000U;
    cfg.reconnect_attempt_ms = 10U;
    cfg.startup_timeout_ms = 100U;
    cfg.link_poll_ms = 10U;
    cfg.connect_retry_ms = 10U;
    cfg.connect_timeout_ms = 10U;
    cfg.send_miss_threshold = send_miss_threshold;
    cfg.standby_sync = false;
    cfg.frame_size = FRAME_SIZE;
    cfg.tx_buf = app->tx;
    cfg.rx_buf = app->rx;
    cfg.role_tag = west ? "A/WEST" : "A/EAST";
    cfg.peer_tag = west ? "A/EAST" : "A/WEST";
    cfg.site_name = west ? "WEST" : "EAST";
    cfg.encode_state = app_encode;
    cfg.receive_state = app_receive;
    cfg.on_change = app_change;
    cfg.user = app;
    assert(rte_site_role_init(rs, &cfg) == RTE_STATUS_OK);
}

static void reset_links(void)
{
    (void)memset(&g_west_to_east, 0, sizeof(g_west_to_east));
    (void)memset(&g_east_to_west, 0, sizeof(g_east_to_west));
    (void)memset(&g_west_link, 0, sizeof(g_west_link));
    (void)memset(&g_east_link, 0, sizeof(g_east_link));
    g_west_link.inbox = &g_east_to_west;
    g_west_link.outbox = &g_west_to_east;
    g_east_link.inbox = &g_west_to_east;
    g_east_link.outbox = &g_east_to_west;
}

static rte_site_role_local_t ready_single_channel(bool faulted)
{
    rte_site_role_local_t local;

    (void)memset(&local, 0, sizeof(local));
    local.faulted = faulted;
    local.own_ready = true;
    local.sibling_reachable = false; /* no A/B sibling in this test: nobody to wait for */
    return local;
}

static rte_site_role_t g_west;
static rte_site_role_t g_east;
static app_t           g_west_app;
static app_t           g_east_app;

static void test_init_rejects_bad_config(void)
{
    rte_site_role_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    assert(rte_site_role_init(NULL, &cfg) == RTE_STATUS_INVALID_PARAM);
    assert(rte_site_role_init(&g_west, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_site_role_init(&g_west, &cfg) == RTE_STATUS_INVALID_PARAM); /* no buffers, no callbacks */
}

/* Both start STANDBY (no startup negotiation in this test); the counterpart answers and is not ONLINE: WEST wins the
 * tie after RTE_SITE_ROLE_BOTH_STANDBY_CYCLES, EAST keeps STANDBY. */
static void test_both_standby_tiebreak_promotes_west(void)
{
    const rte_site_role_local_t local = ready_single_channel(false);
    uint32_t cycle;

    reset_links();
    setup(&g_west, &g_west_app, true, 200U);
    setup(&g_east, &g_east_app, false, 200U);
    for (cycle = 1U; cycle <= 10U; cycle++)
    {
        rte_site_role_execute(&g_west, cycle, &local);
        rte_site_role_execute(&g_east, cycle, &local);
    }
    assert(rte_site_role_is_online(&g_west));
    assert(!rte_site_role_is_online(&g_east));
    assert(g_west_app.changes == 1);
    assert(g_west_app.last_change.kind == RTE_SITE_ROLE_CHANGE_PROMOTED);
    assert(g_west_app.last_change.reason == RTE_SITE_ROLE_REASON_BOTH_STANDBY_WEST);
    assert(g_west_app.last_change.apply_state);
    assert(g_east_app.changes == 0);
    assert(g_east_app.receive_calls > 0);
}

/* WEST is ONLINE and sends its live state as a valid snapshot; EAST holds it. WEST then flushes FAULTED: EAST promotes
 * (PEER_FAULTED) and is told to adopt the snapshot, with WEST's cycle number. */
static void test_snapshot_adopted_on_peer_faulted(void)
{
    const rte_site_role_local_t local = ready_single_channel(false);
    uint32_t cycle;

    test_both_standby_tiebreak_promotes_west();
    (void)memcpy(g_west_app.live_state, "SESSIONS", STATE_SIZE);
    for (cycle = 11U; cycle <= 13U; cycle++)
    {
        rte_site_role_execute(&g_west, cycle, &local);
        rte_site_role_execute(&g_east, cycle, &local);
    }
    assert(memcmp(g_east_app.held_snapshot, "SESSIONS", STATE_SIZE) == 0);
    assert(!rte_site_role_is_online(&g_east));

    rte_site_role_flush_faulted(&g_west, false);
    rte_site_role_execute(&g_east, 14U, &local);
    assert(rte_site_role_is_online(&g_east));
    assert(g_east_app.last_change.kind == RTE_SITE_ROLE_CHANGE_PROMOTED);
    assert(g_east_app.last_change.reason == RTE_SITE_ROLE_REASON_PEER_FAULTED);
    assert(g_east_app.last_change.apply_state);
    assert(g_east_app.last_change.snapshot_valid);
    assert(g_east_app.last_change.snapshot_cycle == 13U);
}

/* A silent counterpart never promotes by itself; a dispatcher confirmation does (REQ-SITEROLE-003, -013). */
static void test_silence_needs_dispatcher_confirmation(void)
{
    const rte_site_role_local_t local = ready_single_channel(false);
    rte_site_role_status_t st;
    uint32_t cycle;

    reset_links();
    setup(&g_east, &g_east_app, false, 3U);
    g_east_link.blocked = 1; /* nothing EAST sends arrives, WEST never answers */
    for (cycle = 1U; cycle <= 20U; cycle++)
    {
        rte_site_role_execute(&g_east, cycle, &local);
    }
    rte_site_role_get_status(&g_east, &st);
    assert(st.peer_unresponsive);
    assert(!rte_site_role_is_online(&g_east));
    assert(g_east_app.changes == 0);

    rte_site_role_on_takeover_confirm(&g_east, 21U);
    rte_site_role_execute(&g_east, 22U, &local);
    assert(rte_site_role_is_online(&g_east));
    assert(g_east_app.last_change.reason == RTE_SITE_ROLE_REASON_DISPATCHER_CONFIRMED);
    assert(!g_east_app.last_change.snapshot_valid); /* nothing ever received: the application starts cold */
}

/* Frame layout: faulted | single | cycle (LE) | state | valid. */
static void test_flush_faulted_frame_layout(void)
{
    const rte_site_role_local_t local = ready_single_channel(false);

    reset_links();
    setup(&g_west, &g_west_app, true, 200U);
    rte_site_role_execute(&g_west, 1U, &local); /* opens the link */
    rte_site_role_flush_faulted(&g_west, true);
    assert(g_west_app.tx[0] == 1U);
    assert(g_west_app.tx[1] == (uint8_t)RTE_SITE_ROLE_SINGLE_MODE_SINGLE);
    assert((g_west_app.tx[2] | g_west_app.tx[3] | g_west_app.tx[4] | g_west_app.tx[5]) == 0U);
    assert(g_west_app.tx[FRAME_SIZE - 1U] == 0U);
}

/* A/B role agreement (REQ-SITEROLE-014): unknown sibling is no verdict; disagreement is a fault on the third
 * consecutive check; one agreeing check resets the count. */
static void test_sibling_agreement(void)
{
    rte_site_role_sibling_t sib;

    test_both_standby_tiebreak_promotes_west(); /* WEST ONLINE, not single */
    (void)memset(&sib, 0, sizeof(sib));
    assert(rte_site_role_check_sibling(&g_west, 11U, false, &sib) == RTE_SITE_ROLE_SIBLING_UNKNOWN);
    assert(rte_site_role_check_sibling(&g_west, 11U, false, NULL) == RTE_SITE_ROLE_SIBLING_UNKNOWN);

    sib.known = true;
    sib.online = true;
    assert(rte_site_role_check_sibling(&g_west, 11U, false, &sib) == RTE_SITE_ROLE_SIBLING_AGREE);

    sib.online = false; /* the sibling thinks the site is STANDBY */
    assert(rte_site_role_check_sibling(&g_west, 12U, false, &sib) == RTE_SITE_ROLE_SIBLING_PENDING);
    assert(rte_site_role_check_sibling(&g_west, 13U, false, &sib) == RTE_SITE_ROLE_SIBLING_PENDING);
    sib.online = true;
    assert(rte_site_role_check_sibling(&g_west, 14U, false, &sib) == RTE_SITE_ROLE_SIBLING_AGREE); /* reset */
    sib.single_mode = true; /* same role, different mode: also a disagreement */
    assert(rte_site_role_check_sibling(&g_west, 15U, false, &sib) == RTE_SITE_ROLE_SIBLING_PENDING);
    assert(rte_site_role_check_sibling(&g_west, 16U, false, &sib) == RTE_SITE_ROLE_SIBLING_PENDING);
    assert(rte_site_role_check_sibling(&g_west, 17U, false, &sib) == RTE_SITE_ROLE_SIBLING_FAULT);
    assert(rte_site_role_check_sibling(&g_west, 18U, true, &sib) == RTE_SITE_ROLE_SIBLING_AGREE);
}

int main(void)
{
    assert(rte_checksum_crc64_init(RTE_CRC64_ERTMS) == RTE_STATUS_OK);
    assert(rte_osadapter_netlink_register(&g_mock_netlink) == RTE_STATUS_OK);
    assert(rte_osadapter_timer_register(&g_mock_timer) == RTE_STATUS_OK);

    test_init_rejects_bad_config();
    test_both_standby_tiebreak_promotes_west();
    test_snapshot_adopted_on_peer_faulted();
    test_silence_needs_dispatcher_confirmation();
    test_flush_faulted_frame_layout();
    test_sibling_agreement();
    return 0;
}
