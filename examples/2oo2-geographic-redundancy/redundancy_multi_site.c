/**
 * @file redundancy_multi_site.c
 * @brief Multi-Site Redundancy Implementation
 *
 * Implements 2oo2 voting across two geographic sites with active/standby failover.
 * Each site has independent vital channels (A, B) + service channel (C).
 *
 * Safety Model:
 * - ONLINE site: Performs 2oo2 voting, produces commands
 * - STANDBY site: Mirrors voting (for sync), ready to take over
 * - On ONLINE failure: STANDBY automatically becomes ONLINE
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "redundancy_multi_site.h"

/* ============================================================================
 * CRC32 (reuse from single-site example)
 * ========================================================================== */

static uint32_t crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xffffffff;
    uint32_t i;

    if (data == NULL) return 0;

    for (i = 0; i < len; i++) {
        uint8_t byte = data[i];
        crc = (crc >> 8) ^ (uint32_t)(crc ^ byte);
    }

    return crc ^ 0xffffffff;
}

/* ============================================================================
 * Safe State Generation
 * ========================================================================== */

static train_command_t safe_command(uint32_t train_id)
{
    train_command_t safe = {
        .train_id = train_id,
        .position = 0,
        .signal_state = 0,
        .emergency_stop = 1,
        .speed_limit = 0,
        .timestamp_ms = 0,
        .checksum = 0,
        .sequence_number = 0
    };
    return safe;
}

/* ============================================================================
 * Per-Site Voting Logic
 * ========================================================================== */

static int site_vote(site_context_t *site,
                     const train_command_t *cmd_a,
                     const train_command_t *cmd_b)
{
    if (site == NULL || cmd_a == NULL || cmd_b == NULL) {
        return -1;
    }

    site->stats.total_votes++;

    /* Verify checksums */
    uint32_t checksum_a = crc32((const uint8_t *)cmd_a,
                                 sizeof(*cmd_a) - sizeof(cmd_a->checksum));
    uint32_t checksum_b = crc32((const uint8_t *)cmd_b,
                                 sizeof(*cmd_b) - sizeof(cmd_b->checksum));

    if (checksum_a != cmd_a->checksum) {
        site->stats.channel_a_errors++;
        snprintf(site->channels.service_output, sizeof(site->channels.service_output),
                 "[SITE %s] Channel A corruption (CRC fail), using safe state",
                 site->site_id == SITE_WEST ? "WEST" : "EAST");
        site->channels.voting_result = 2;
        site->channels.voted_command = safe_command(cmd_a->train_id);
        return 0;
    }

    if (checksum_b != cmd_b->checksum) {
        site->stats.channel_b_errors++;
        snprintf(site->channels.service_output, sizeof(site->channels.service_output),
                 "[SITE %s] Channel B corruption (CRC fail), using safe state",
                 site->site_id == SITE_WEST ? "WEST" : "EAST");
        site->channels.voting_result = 2;
        site->channels.voted_command = safe_command(cmd_b->train_id);
        return 0;
    }

    /* Compare commands */
    if (cmd_a->train_id != cmd_b->train_id ||
        cmd_a->position != cmd_b->position ||
        cmd_a->signal_state != cmd_b->signal_state ||
        cmd_a->speed_limit != cmd_b->speed_limit) {

        /* MISMATCH */
        site->stats.total_mismatches++;
        site->channels.voting_result = 1;
        site->channels.voted_command = safe_command(cmd_a->train_id);

        snprintf(site->channels.service_output, sizeof(site->channels.service_output),
                 "[SITE %s] MISMATCH: A(train=%u,sig=%u,spd=%u) vs "
                 "B(train=%u,sig=%u,spd=%u) → SAFE_STATE",
                 site->site_id == SITE_WEST ? "WEST" : "EAST",
                 cmd_a->train_id, cmd_a->signal_state, cmd_a->speed_limit,
                 cmd_b->train_id, cmd_b->signal_state, cmd_b->speed_limit);
        return 0;
    }

    /* MATCH */
    site->stats.total_matches++;
    site->channels.voting_result = 0;
    site->channels.voted_command = *cmd_a;
    site->channels.channel_a = *cmd_a;
    site->channels.channel_b = *cmd_b;

    snprintf(site->channels.service_output, sizeof(site->channels.service_output),
             "[SITE %s] MATCH: train=%u, pos=%u, sig=%u, speed=%u km/h",
             site->site_id == SITE_WEST ? "WEST" : "EAST",
             cmd_a->train_id, cmd_a->position,
             cmd_a->signal_state, cmd_a->speed_limit);
    return 0;
}

/* ============================================================================
 * Initialization
 * ========================================================================== */

int multi_site_voter_init(multi_site_voter_t *voter,
                          const multi_site_config_t *config)
{
    if (voter == NULL || config == NULL) {
        return -1;
    }

    memset(voter, 0, sizeof(*voter));
    voter->config = *config;

    /* Initialize West (ONLINE) */
    voter->west.site_id = SITE_WEST;
    voter->west.state = SITE_STATE_ONLINE;
    voter->west.heartbeat_timestamp_ms = (uint32_t)(time(NULL) * 1000);

    /* Initialize East (STANDBY) */
    voter->east.site_id = SITE_EAST;
    voter->east.state = SITE_STATE_STANDBY;
    voter->east.heartbeat_timestamp_ms = (uint32_t)(time(NULL) * 1000);

    /* West is initially active */
    voter->active_site = SITE_WEST;

    fprintf(stderr,
            "[MULTI-SITE] Initialized: %s (ONLINE) vs %s (STANDBY)\n",
            config->west_site_name,
            config->east_site_name);

    return 0;
}

/* ============================================================================
 * Voting Cycle
 * ========================================================================== */

int multi_site_vote_cycle(multi_site_voter_t *voter,
                          site_id_t site,
                          const train_command_t *cmd_a,
                          const train_command_t *cmd_b)
{
    if (voter == NULL || cmd_a == NULL || cmd_b == NULL) {
        return -1;
    }

    site_context_t *site_ctx = (site == SITE_WEST) ? &voter->west : &voter->east;

    /* Update sequence number */
    site_ctx->stats.sequence_number++;

    /* Perform 2oo2 voting */
    return site_vote(site_ctx, cmd_a, cmd_b);
}

/* ============================================================================
 * Heartbeat & Failover
 * ========================================================================== */

site_id_t multi_site_heartbeat(multi_site_voter_t *voter)
{
    if (voter == NULL) {
        return voter->active_site;
    }

    uint32_t now = (uint32_t)(time(NULL) * 1000);

    /* Check if active site is still responding */
    site_context_t *active_site_ctx =
        (voter->active_site == SITE_WEST) ? &voter->west : &voter->east;
    site_context_t *standby_site_ctx =
        (voter->active_site == SITE_WEST) ? &voter->east : &voter->west;

    uint32_t time_since_active = now - active_site_ctx->heartbeat_timestamp_ms;

    if (time_since_active > voter->config.heartbeat_timeout_ms) {
        active_site_ctx->heartbeat_miss_count++;

        if (active_site_ctx->heartbeat_miss_count >=
            voter->config.max_heartbeat_misses) {

            /* FAILOVER TRIGGERED */
            fprintf(stderr,
                    "\n[MULTI-SITE FAILOVER] %s lost heartbeat (%u ms), "
                    "switching to %s\n\n",
                    active_site_ctx->site_id == SITE_WEST ? "WEST" : "EAST",
                    time_since_active,
                    standby_site_ctx->site_id == SITE_WEST ? "WEST" : "EAST");

            /* Mark old active as faulty */
            active_site_ctx->state = SITE_STATE_FAULTY;

            /* Promote standby to online */
            standby_site_ctx->state = SITE_STATE_ONLINE;
            voter->active_site =
                (voter->active_site == SITE_WEST) ? SITE_EAST : SITE_WEST;

            /* Reset heartbeat timer */
            standby_site_ctx->heartbeat_timestamp_ms = now;
            standby_site_ctx->heartbeat_miss_count = 0;

            return voter->active_site;
        }
    } else {
        /* Heartbeat restored, reset counter */
        active_site_ctx->heartbeat_miss_count = 0;
    }

    /* Update active site heartbeat */
    active_site_ctx->heartbeat_timestamp_ms = now;

    return voter->active_site;
}

/* ============================================================================
 * Manual Failover
 * ========================================================================== */

int multi_site_failover_request(multi_site_voter_t *voter,
                                 site_id_t new_active)
{
    if (voter == NULL) {
        return -1;
    }

    if (new_active == voter->active_site) {
        return 0;  /* Already active */
    }

    site_context_t *old_active =
        (voter->active_site == SITE_WEST) ? &voter->west : &voter->east;
    site_context_t *new_active_ctx = (new_active == SITE_WEST) ? &voter->west : &voter->east;

    fprintf(stderr,
            "[MULTI-SITE FAILOVER] Manual request: switching from %s to %s\n",
            old_active->site_id == SITE_WEST ? "WEST" : "EAST",
            new_active == SITE_WEST ? "WEST" : "EAST");

    old_active->state = SITE_STATE_STANDBY;
    new_active_ctx->state = SITE_STATE_ONLINE;
    voter->active_site = new_active;

    return 0;
}

/* ============================================================================
 * Query Active Command
 * ========================================================================== */

int multi_site_get_active_command(multi_site_voter_t *voter,
                                   train_command_t *cmd_out)
{
    if (voter == NULL || cmd_out == NULL) {
        return -1;
    }

    site_context_t *active = (voter->active_site == SITE_WEST) ?
                               &voter->west : &voter->east;

    *cmd_out = active->channels.voted_command;
    return 0;
}

/* ============================================================================
 * Query Service Output (C Channel)
 * ========================================================================== */

int multi_site_get_service_output(multi_site_voter_t *voter,
                                   char *output_buf,
                                   uint32_t buf_size)
{
    if (voter == NULL || output_buf == NULL || buf_size == 0) {
        return -1;
    }

    site_context_t *active = (voter->active_site == SITE_WEST) ?
                               &voter->west : &voter->east;

    strncpy(output_buf, active->channels.service_output, buf_size - 1);
    output_buf[buf_size - 1] = '\0';

    return (int)strlen(output_buf);
}

/* ============================================================================
 * Statistics
 * ========================================================================== */

void multi_site_print_stats(const multi_site_voter_t *voter)
{
    if (voter == NULL) {
        return;
    }

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Multi-Site Voting Statistics\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\nWEST Site (%s):\n", voter->west.state == SITE_STATE_ONLINE ? "ONLINE" :
           voter->west.state == SITE_STATE_STANDBY ? "STANDBY" : "FAULTY");
    printf("  Total votes:      %u\n", voter->west.stats.total_votes);
    printf("  Matches:          %u (%.1f%%)\n", voter->west.stats.total_matches,
           voter->west.stats.total_votes > 0 ?
           (100.0 * voter->west.stats.total_matches / voter->west.stats.total_votes) : 0.0);
    printf("  Mismatches:       %u\n", voter->west.stats.total_mismatches);
    printf("  Ch-A errors:      %u\n", voter->west.stats.channel_a_errors);
    printf("  Ch-B errors:      %u\n", voter->west.stats.channel_b_errors);

    printf("\nEAST Site (%s):\n", voter->east.state == SITE_STATE_ONLINE ? "ONLINE" :
           voter->east.state == SITE_STATE_STANDBY ? "STANDBY" : "FAULTY");
    printf("  Total votes:      %u\n", voter->east.stats.total_votes);
    printf("  Matches:          %u (%.1f%%)\n", voter->east.stats.total_matches,
           voter->east.stats.total_votes > 0 ?
           (100.0 * voter->east.stats.total_matches / voter->east.stats.total_votes) : 0.0);
    printf("  Mismatches:       %u\n", voter->east.stats.total_mismatches);
    printf("  Ch-A errors:      %u\n", voter->east.stats.channel_a_errors);
    printf("  Ch-B errors:      %u\n", voter->east.stats.channel_b_errors);

    printf("\nActive Site: %s\n", voter->active_site == SITE_WEST ? "WEST" : "EAST");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
}

/* ============================================================================
 * Shutdown
 * ========================================================================== */

int multi_site_voter_shutdown(multi_site_voter_t *voter)
{
    if (voter == NULL) {
        return -1;
    }

    fprintf(stderr, "[MULTI-SITE] Shutting down voter\n");
    voter->west.state = SITE_STATE_FAULTY;
    voter->east.state = SITE_STATE_FAULTY;

    return 0;
}
