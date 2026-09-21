/**
 * @file geo_distributed_checkpoint_sync.c
 * @brief Example: two rte_channel_t links (one per "machine"), registered
 *        into a rte_voter_t and synchronized via rte_checkpoint
 *        (ADR-017, rewired onto rte_voter_t by ADR-025) instead of
 *        wall-clock agreement.
 *
 * Demonstrates the answer to "how do channels on different hardware, in
 * different locations, get compared as if they were the same silicon":
 * not by making their clocks agree (rte_clocksync is diagnostic only -
 * see its header), but by having both sites confirm the same
 * checkpoint_id to each other within a bounded timeout before the real
 * vote happens. If a site doesn't confirm in time, that's a fault and
 * the framework enters safe-state itself - it is not left to the
 * application to notice.
 *
 * This example simulates the network with in-memory mailboxes (one per
 * site) instead of a real socket/serial link, so it runs standalone with
 * no external dependencies - swap mock_backend_send/recv for a real
 * transport (e.g. built on rte_ipc, TCP, or a serial link) to deploy
 * this for real, per ADR-017 section 2.2 ("no new OSAdapter of its own -
 * reuses whatever transport is already registered on each channel").
 *
 * Scenario:
 *   Cycle 1: both sites confirm checkpoint 1 in time -> vote succeeds.
 *   Cycle 2: WEST's confirmation is deliberately dropped (simulating a
 *            lost/delayed message) -> checkpoint fails -> safe-state.
 *
 * Compile:
 *   gcc -std=c99 -Wall -Wextra -o geo_checkpoint_sync \
 *       geo_distributed_checkpoint_sync.c \
 *       -I../include -L../build \
 *       -lrte_channels -lrte_oal -lrte_core
 */

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "rte/checkpoint/rte_checkpoint.h"
#include "rte/checksum/rte_checksum.h"
#include "rte/clocksync/rte_clocksync.h"
#include "rte_osadapter/clocksync/rte_osadapter_clocksync.h"
#include "rte/safestate/rte_safestate.h"
#include "rte/voter/rte_voter.h"

/* ============================================================================
 * Simulated transport: one in-memory mailbox per site (WEST=0, EAST=1)
 * ========================================================================== */

#define SITE_WEST 0
#define SITE_EAST 1
#define SITE_COUNT 2U

static int g_site_index[SITE_COUNT] = { SITE_WEST, SITE_EAST };
static void *g_channel_handles[SITE_COUNT] = { &g_site_index[SITE_WEST], &g_site_index[SITE_EAST] };

static rte_vital_message_t g_mailbox[SITE_COUNT];
static bool g_mail_ready[SITE_COUNT];
/* Simulates a dead link to a given site: its backend_recv() times out
 * regardless of what was sent, standing in for a message that was sent
 * but never arrived (dropped/delayed network, not a local logic error -
 * a real transport's recv() would time out the same way). */
static bool g_link_down[SITE_COUNT];

static rte_status_t mock_send(void *channel, const void *data, size_t data_size)
{
    int idx = *(const int *)channel;

    if (data_size != sizeof(rte_vital_message_t))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* Loopback echo for this single-process demo: in a real deployment
     * this send() would travel over rte_ipc/a socket/a serial link to
     * the actual peer process, which would send its own confirmation
     * back independently - this mock just short-circuits that round
     * trip for sites whose link is up. */
    (void)memcpy(&g_mailbox[idx], data, data_size);
    g_mail_ready[idx] = true;
    return RTE_STATUS_OK;
}

static rte_status_t mock_recv(void *channel, void *data, size_t data_size, uint32_t timeout_ms)
{
    int idx = *(const int *)channel;

    (void)timeout_ms; /* in-memory mailbox: either the mail is there or it isn't */
    if (data_size != sizeof(rte_vital_message_t))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (g_link_down[idx] || !g_mail_ready[idx])
    {
        return RTE_STATUS_TIMEOUT;
    }
    (void)memcpy(data, &g_mailbox[idx], data_size);
    return RTE_STATUS_OK;
}

/* ============================================================================
 * A minimal rte_clocksync OSAdapter - diagnostic only, per its own header's
 * warning: never the basis for deciding whether results are comparable.
 * ========================================================================== */

static rte_status_t clocksync_get_offset_ms(int64_t *out_offset_ms)
{
    *out_offset_ms = 3; /* pretend WEST's clock is 3ms ahead of EAST's */
    return RTE_STATUS_OK;
}

static rte_status_t clocksync_get_quality(rte_clocksync_quality_t *out_quality)
{
    *out_quality = RTE_CLOCKSYNC_SYNCHRONIZED;
    return RTE_STATUS_OK;
}

/* ============================================================================
 * Safe-state handler for this demo: print and unwind via longjmp instead
 * of the production infinite loop, so the example can report the outcome
 * and exit cleanly. A real integrator's SAFE handler drives outputs to
 * their fail-safe state instead.
 * ========================================================================== */

static jmp_buf g_safestate_jmp;

static void demo_safestate_handler(rte_safestate_level_t level, rte_safestate_reason_t reason, const char *file,
                                    int32_t line, const char *message)
{
    (void)file;
    (void)message;
    printf("  [SAFE-STATE] level=%d reason=%u at line %d - withdrawing movement authority\n", (int)level,
           (unsigned)reason, (int)line);
    longjmp(g_safestate_jmp, 1);
}

int main(void)
{
    rte_channel_t sites[SITE_COUNT];
    rte_voter_t voter;
    rte_voter_config_t voter_config;
    rte_osadapter_clocksync_t clock_osadapter;
    int64_t offset_ms = 0;
    rte_clocksync_quality_t quality = RTE_CLOCKSYNC_UNSYNCHRONIZED;
    uint32_t i;

    (void)rte_checksum_crc64_init(RTE_CRC64_ERTMS);
    (void)rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_SAFE, demo_safestate_handler);

    clock_osadapter.get_offset_ms = clocksync_get_offset_ms;
    clock_osadapter.get_quality = clocksync_get_quality;
    (void)rte_osadapter_clocksync_register(&clock_osadapter);
    (void)rte_clocksync_get_offset_ms(&offset_ms);
    (void)rte_clocksync_get_quality(&quality);
    printf("Diagnostic clock offset WEST-vs-EAST: %lldms (quality=%d) - NOT used for the vote below\n",
           (long long)offset_ms, (int)quality);

    memset(&voter_config, 0, sizeof(voter_config));
    voter_config.voting_strategy = RTE_VOTING_2OO2;
    voter_config.channel_timeout_ms = 100U;
    (void)rte_voter_init(&voter, &voter_config);

    for (i = 0U; i < SITE_COUNT; i++)
    {
        rte_channel_config_t chan_config;

        memset(&chan_config, 0, sizeof(chan_config));
        chan_config.channel_handle = g_channel_handles[i];
        chan_config.send = mock_send;
        chan_config.recv = mock_recv;
        (void)rte_channel_init(&sites[i], &chan_config);
        (void)rte_voter_register_channel(&voter, &sites[i]);
    }

    /* ---- Cycle 1: both sites confirm in time ---- */
    {
        rte_checkpoint_config_t ckpt;
        rte_status_t status;

        g_mail_ready[SITE_WEST] = false;
        g_mail_ready[SITE_EAST] = false;

        ckpt.checkpoint_id = 1U;
        ckpt.max_delay_ms = 200U;
        ckpt.expected_node_count = SITE_COUNT;
        ckpt.watchdog = NULL;

        printf("\nCycle 1: WEST and EAST both reach checkpoint 1...\n");
        status = rte_channel_checkpoint(&voter, &ckpt);
        printf("  rte_channel_checkpoint() -> %s\n", rte_status_to_string(status));
    }

    /* ---- Cycle 2: EAST's confirmation never arrives (dropped/delayed) ---- */
    if (setjmp(g_safestate_jmp) == 0)
    {
        rte_checkpoint_config_t ckpt;
        rte_status_t status;

        g_mail_ready[SITE_WEST] = false;
        g_mail_ready[SITE_EAST] = false;
        g_link_down[SITE_EAST] = true; /* EAST's confirmation will never arrive */

        ckpt.checkpoint_id = 2U;
        ckpt.max_delay_ms = 50U;
        ckpt.expected_node_count = SITE_COUNT;
        ckpt.watchdog = NULL;

        printf("\nCycle 2: EAST's link is down - its confirmation never arrives...\n");
        status = rte_channel_checkpoint(&voter, &ckpt);
        /* Not reached on a real timeout: rte_channel_checkpoint() enters
         * safe-state directly (REQ-CHECKPOINT-003) before returning. */
        printf("  rte_channel_checkpoint() -> %s (unexpected - should have diverted)\n",
               rte_status_to_string(status));
    }
    else
    {
        printf("  Cycle 2 correctly forced safe-state instead of voting on an unconfirmed cycle.\n");
    }

    (void)rte_voter_destroy(&voter);
    for (i = 0U; i < SITE_COUNT; i++)
    {
        (void)rte_channel_destroy(&sites[i]);
    }
    return 0;
}
