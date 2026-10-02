/**
 * @file test_rte_site_role_policy.c
 * @brief Decision table and timing predicates of the site-role policy (rte_site_role_policy.h), ported unchanged from
 *        RBC_GP's test_ab_gp_site_role.c and test_ab_gp_standby_policy.c (ADR-040 step 1).
 */
#include "rte/redundancy/site_role/rte_site_role_policy.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            (void)fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                              \
        }                                                                          \
    } while (0)

/** A healthy STANDBY channel of the EAST site, link up, counterpart answering and not ONLINE. */
static rte_site_role_input_t standby_east(void)
{
    rte_site_role_input_t in;

    (void)memset(&in, 0, sizeof(in));
    in.link_up = true;
    in.cycle = 100U;
    in.peer_last_rx_cycle = 100U;
    in.own_ready = true;
    in.sibling_ready = true;
    in.sibling_reachable = true;
    return in;
}

static int expect(const rte_site_role_input_t *in, rte_site_role_action_t action, rte_site_role_reason_t reason)
{
    rte_site_role_decision_t d = rte_site_role_decide(in);

    if ((d.action != action) || (d.reason != reason))
    {
        (void)fprintf(stderr, "decision %d/%d, expected %d/%d\n", (int)d.action, (int)d.reason, (int)action, (int)reason);
        return 1;
    }
    return 0;
}

static int test_faulted_rules(void)
{
    rte_site_role_input_t in = standby_east();

    in.own_faulted = true;
    in.peer_faulted = true;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_RENEGOTIATE, RTE_SITE_ROLE_REASON_BOTH_FAULTED) == 0);

    in = standby_east();
    in.peer_faulted = true; /* the counterpart announced its reboot */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_PEER_FAULTED) == 0);

    in = standby_east();
    in.own_faulted = true;
    in.peer_snapshot_valid = true; /* recovered: the counterpart is healthy and holds ONLINE */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_DEMOTE, RTE_SITE_ROLE_REASON_RECOVERED) == 0);

    in = standby_east();
    in.own_faulted = true; /* FAULTED alone changes nothing */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0);
    return 0;
}

static int test_silence_never_promotes_without_confirmation(void)
{
    rte_site_role_input_t in = standby_east();

    in.peer_unresponsive = true;
    in.peer_last_rx_cycle = 10U; /* silent for 90 cycles */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0);

    in.takeover_confirmed = true;
    in.takeover_confirm_cycle = 95U;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_DISPATCHER_CONFIRMED) == 0);

    in.takeover_confirm_cycle = 100U - RTE_SITE_ROLE_TAKEOVER_CONFIRM_VALID_CYCLES - 1U; /* expired */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0);
    return 0;
}

static int test_dual_online(void)
{
    rte_site_role_input_t in = standby_east();

    in.is_online = true;
    in.peer_online_now = true;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_ALARM, RTE_SITE_ROLE_REASON_DUAL_ONLINE) == 0);

    in.lost_peer_while_online = true;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_DEMOTE, RTE_SITE_ROLE_REASON_YIELD_TO_TAKEOVER) == 0);

    in.peer_last_rx_cycle = 50U; /* the counterpart's ONLINE flag is stale: no reaction */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0);
    return 0;
}

static int test_both_standby_tiebreak(void)
{
    rte_site_role_input_t in = standby_east();
    rte_site_role_decision_t d;
    uint32_t i;

    /* WEST waits the hold time, counting consecutive cycles; EAST never promotes on its own. */
    in.is_west = true;
    for (i = 1U; i < RTE_SITE_ROLE_BOTH_STANDBY_CYCLES; i++)
    {
        d = rte_site_role_decide(&in);
        CHECK(d.action == RTE_SITE_ROLE_ACTION_NONE);
        CHECK(d.both_standby_cycles == i);
        in.both_standby_cycles = d.both_standby_cycles;
    }
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_BOTH_STANDBY_WEST) == 0);

    in = standby_east();
    in.both_standby_cycles = 1000U;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0);

    /* Any channel joins its ONLINE A/B sibling after one cycle. */
    in = standby_east();
    in.sibling_online = true;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_BOTH_STANDBY_SIBLING) == 0);

    /* The counter is not a lifetime count: a cycle without the condition resets it. */
    in = standby_east();
    in.both_standby_cycles = 5U;
    in.link_up = false;
    d = rte_site_role_decide(&in);
    CHECK(d.action == RTE_SITE_ROLE_ACTION_NONE);
    CHECK(d.both_standby_cycles == 0U);

    in = standby_east();
    in.peer_online_now = true; /* the counterpart serves: nothing to tie-break */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0);
    return 0;
}

static int test_readiness_gate(void)
{
    rte_site_role_input_t in = standby_east();

    /* counterpart FAULTED: promotion is due, but only when both A/B channels are ready */
    in.peer_faulted = true;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_PEER_FAULTED) == 0);
    in.own_ready = false;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_WAIT, RTE_SITE_ROLE_REASON_WAITING_READY) == 0);
    in.own_ready = true;
    in.sibling_ready = false; /* reachable sibling not ready */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_WAIT, RTE_SITE_ROLE_REASON_WAITING_READY) == 0);
    in.sibling_online = true; /* sibling already ONLINE: follow it */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_PEER_FAULTED) == 0);
    in.sibling_online = false;
    in.sibling_reachable = false; /* single channel: nobody to wait for */
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_PEER_FAULTED) == 0);
    return 0;
}

static int test_follow_sibling_with_unusable_link(void)
{
    rte_site_role_input_t in = standby_east();

    in.sibling_online = true;
    in.link_up = false;
    in.peer_unresponsive = true;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0); /* first cycle only counts */
    in.both_standby_cycles = 1U;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_PROMOTE, RTE_SITE_ROLE_REASON_BOTH_STANDBY_SIBLING) == 0);
    in.peer_unresponsive = false; /* counterpart verifiably ONLINE: real disagreement, no following */
    in.peer_online_now = true;
    CHECK(expect(&in, RTE_SITE_ROLE_ACTION_NONE, RTE_SITE_ROLE_REASON_NONE) == 0);
    return 0;
}

static int test_reason_texts_exist(void)
{
    rte_site_role_reason_t r;

    for (r = RTE_SITE_ROLE_REASON_NONE; r <= RTE_SITE_ROLE_REASON_WAITING_READY; r = (rte_site_role_reason_t)((int)r + 1))
    {
        CHECK(rte_site_role_reason_text(r)[0] != '\0');
        CHECK(strchr(rte_site_role_reason_token(r), ' ') == NULL);
    }
    return 0;
}

static int test_sync_due_only_when_enabled_online_and_linked(void)
{
    CHECK(rte_site_role_standby_sync_due(true, true, true, true, 10U, 0U));
    CHECK(!rte_site_role_standby_sync_due(false, true, true, true, 10U, 0U)); /* cold */
    CHECK(!rte_site_role_standby_sync_due(true, false, true, true, 10U, 0U)); /* STANDBY never sends */
    CHECK(!rte_site_role_standby_sync_due(true, true, false, true, 10U, 0U)); /* link down */
    return 0;
}

static int test_unhealthy_standby_is_retried_only_periodically(void)
{
    CHECK(!rte_site_role_standby_sync_due(true, true, true, false, 5U, 5U));
    CHECK(!rte_site_role_standby_sync_due(true, true, true, false, 5U + RTE_SITE_ROLE_STANDBY_SYNC_RETRY_CYCLES - 1U, 5U));
    CHECK(rte_site_role_standby_sync_due(true, true, true, false, 5U + RTE_SITE_ROLE_STANDBY_SYNC_RETRY_CYCLES, 5U));
    return 0;
}

static int test_restarted_cycle_counter_retries_at_once(void)
{
    CHECK(rte_site_role_standby_sync_due(true, true, true, false, 1U, 500U));
    return 0;
}

static int test_snapshot_goes_stale_after_timeout_only_on_standby(void)
{
    CHECK(!rte_site_role_standby_data_stale(true, false, true, 10U, 10U));
    CHECK(!rte_site_role_standby_data_stale(true, false, true, 10U + RTE_SITE_ROLE_STANDBY_DATA_TIMEOUT_CYCLES, 10U));
    CHECK(rte_site_role_standby_data_stale(true, false, true, 10U + RTE_SITE_ROLE_STANDBY_DATA_TIMEOUT_CYCLES + 1U, 10U));
    CHECK(!rte_site_role_standby_data_stale(false, false, true, 1000U, 10U)); /* cold: unchanged behaviour */
    CHECK(!rte_site_role_standby_data_stale(true, true, true, 1000U, 10U));   /* ONLINE holds no peer snapshot */
    CHECK(!rte_site_role_standby_data_stale(true, false, false, 1000U, 10U)); /* nothing held */
    return 0;
}

static int test_restarted_cycle_counter_makes_snapshot_stale(void)
{
    CHECK(rte_site_role_standby_data_stale(true, false, true, 2U, 400U));
    return 0;
}

static int test_takeover_needs_silence_and_confirmation(void)
{
    CHECK(rte_site_role_takeover_allowed(false, false, true, true, 100U, 90U));
    CHECK(!rte_site_role_takeover_allowed(false, false, true, false, 100U, 90U)); /* silence alone never promotes */
    CHECK(!rte_site_role_takeover_allowed(false, false, false, true, 100U, 90U)); /* peer is responding */
    CHECK(!rte_site_role_takeover_allowed(true, false, true, true, 100U, 90U));   /* already ONLINE */
    CHECK(!rte_site_role_takeover_allowed(false, true, true, true, 100U, 90U));   /* FAULTED never takes over */
    return 0;
}

static int test_takeover_confirmation_expires(void)
{
    CHECK(rte_site_role_takeover_allowed(false, false, true, true, 90U + RTE_SITE_ROLE_TAKEOVER_CONFIRM_VALID_CYCLES, 90U));
    CHECK(!rte_site_role_takeover_allowed(false, false, true, true, 90U + RTE_SITE_ROLE_TAKEOVER_CONFIRM_VALID_CYCLES + 1U, 90U));
    CHECK(!rte_site_role_takeover_allowed(false, false, true, true, 5U, 90U)); /* restarted counter */
    return 0;
}

static int test_both_standby_tiebreak_promotes_only_west_after_the_hold_time(void)
{
    CHECK(!rte_site_role_both_standby_tiebreak(RTE_SITE_ROLE_BOTH_STANDBY_CYCLES - 1U, true, false));
    CHECK(rte_site_role_both_standby_tiebreak(RTE_SITE_ROLE_BOTH_STANDBY_CYCLES, true, false));
    CHECK(!rte_site_role_both_standby_tiebreak(1000U, false, false)); /* EAST waits for WEST unless its A/B sibling is already ONLINE */
    CHECK(rte_site_role_both_standby_tiebreak(1U, false, true));      /* a channel joins its ONLINE A/B sibling at once */
    CHECK(!rte_site_role_both_standby_tiebreak(0U, false, true));
    return 0;
}

int main(void)
{
    int rc = 0;

    rc |= test_faulted_rules();
    rc |= test_silence_never_promotes_without_confirmation();
    rc |= test_dual_online();
    rc |= test_both_standby_tiebreak();
    rc |= test_readiness_gate();
    rc |= test_follow_sibling_with_unusable_link();
    rc |= test_reason_texts_exist();
    rc |= test_sync_due_only_when_enabled_online_and_linked();
    rc |= test_unhealthy_standby_is_retried_only_periodically();
    rc |= test_restarted_cycle_counter_retries_at_once();
    rc |= test_snapshot_goes_stale_after_timeout_only_on_standby();
    rc |= test_restarted_cycle_counter_makes_snapshot_stale();
    rc |= test_takeover_needs_silence_and_confirmation();
    rc |= test_takeover_confirmation_expires();
    rc |= test_both_standby_tiebreak_promotes_only_west_after_the_hold_time();
    if (rc == 0)
    {
        (void)printf("test_rte_site_role_policy: all passed\n");
    }
    return rc;
}
