/* Tests for rte_dual_msgchannel_t (ADR-020 Layer 1): send/receive
 * roundtrip over a mock rte_netlink OSAdapter (two in-memory mailboxes,
 * following tests/netlink/test_rte_netlink.c's own mock-osadapter
 * pattern), sequence continuity rejection, and sender_id masquerade
 * rejection. */
#include <assert.h>
#include <string.h>

#include "rte/redundancy/checksum/rte_checksum.h"
#include "rte/redundancy/dual/rte_dual_msgchannel.h"
#include "rte_osadapter/netlink/rte_osadapter_netlink.h"

/* --- mock netlink OSAdapter: two single-slot mailboxes, wired A<->B --- */
typedef struct
{
    uint8_t buf[sizeof(rte_vital_message_t)];
    size_t  size;
    int     has_data;
} mock_mailbox_t;

typedef struct
{
    mock_mailbox_t *inbox;  /* where this handle's receive() reads from */
    mock_mailbox_t *outbox; /* where this handle's send() writes to */
    int              broken; /* if set, mock_send() fails as if the link were down */
} mock_link_t;

static rte_status_t mock_send(rte_netlink_handle_t handle, const void *message, size_t message_size,
                                rte_duration_ms_t timeout_ms)
{
    mock_link_t *link = (mock_link_t *)handle;

    (void)timeout_ms;
    if (link->broken)
    {
        return RTE_STATUS_TIMEOUT;
    }
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

int main(void)
{
    mock_mailbox_t mailbox_a_to_b = { { 0 }, 0U, 0 };
    mock_mailbox_t mailbox_b_to_a = { { 0 }, 0U, 0 };
    mock_link_t    link_a         = { &mailbox_b_to_a, &mailbox_a_to_b, 0 };
    mock_link_t    link_b         = { &mailbox_a_to_b, &mailbox_b_to_a, 0 };

    rte_dual_msgchannel_t channel_a;
    rte_dual_msgchannel_t channel_b;
    rte_dual_msgchannel_config_t cfg_a;
    rte_dual_msgchannel_config_t cfg_b;

    uint8_t  out_payload[32];
    uint8_t  out_payload_size = 0U;
    uint32_t out_sequence     = 0U;

    assert(rte_checksum_crc64_init(RTE_CRC64_ERTMS) == RTE_STATUS_OK);
    assert(rte_osadapter_netlink_register(&g_mock_netlink_osadapter) == RTE_STATUS_OK);

    cfg_a.link             = (rte_netlink_handle_t)&link_a;
    cfg_a.sender_id        = 1U;
    cfg_a.expected_peer_id = 2U;
    assert(rte_dual_msgchannel_init(&channel_a, &cfg_a) == RTE_STATUS_OK);

    cfg_b.link             = (rte_netlink_handle_t)&link_b;
    cfg_b.sender_id        = 2U;
    cfg_b.expected_peer_id = 1U;
    assert(rte_dual_msgchannel_init(&channel_b, &cfg_b) == RTE_STATUS_OK);

    /* NULL-argument rejection. */
    assert(rte_dual_msgchannel_init(NULL, &cfg_a) == RTE_STATUS_INVALID_PARAM);
    assert(rte_dual_msgchannel_send(NULL, (const uint8_t *)"x", 1U, 10U, NULL) == RTE_STATUS_INVALID_PARAM);

    /* Basic roundtrip: A sends, B receives the same payload, sequence 0. */
    assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"hello", 5U, 10U, &out_sequence) == RTE_STATUS_OK);
    assert(out_sequence == 0U);
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == RTE_STATUS_OK);
    assert(out_payload_size == 5U);
    assert(memcmp(out_payload, "hello", 5U) == 0);
    assert(out_sequence == 0U);

    /* No frame pending: TIMEOUT, not a hang. */
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == RTE_STATUS_TIMEOUT);

    /* Second send advances the sequence; continuity check on B's side
     * (expected_sequence now 1) accepts it. */
    assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"again", 5U, 10U, &out_sequence) == RTE_STATUS_OK);
    assert(out_sequence == 1U);
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == RTE_STATUS_OK);
    assert(out_sequence == 1U);

    /* Masquerade defense: a frame from an unexpected sender_id is
     * rejected as RTE_STATUS_HARDWARE_FAULT, not silently delivered.
     * channel_a's own sender_id (1) does not match what channel_b
     * would need to see from a *different* peer - simulate by pointing
     * a fresh channel at B's inbox but expecting a peer_id B never uses. */
    {
        rte_dual_msgchannel_t         wrong_expect;
        rte_dual_msgchannel_config_t  wrong_cfg;

        wrong_cfg.link             = (rte_netlink_handle_t)&link_b;
        wrong_cfg.sender_id        = 2U;
        wrong_cfg.expected_peer_id = 99U; /* A's real sender_id is 1, not 99 */
        assert(rte_dual_msgchannel_init(&wrong_expect, &wrong_cfg) == RTE_STATUS_OK);

        assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"z", 1U, 10U, NULL) == RTE_STATUS_OK);
        assert(rte_dual_msgchannel_receive(&wrong_expect, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                             &out_sequence)
               == RTE_STATUS_HARDWARE_FAULT);
    }

    /* Sequence continuity: reset both sides to a known-clean state first
     * (the masquerade test above consumed one frame off the wire via a
     * *different* rte_dual_msgchannel_t instance, so channel_a/channel_b's
     * own counters are not both usefully known here without this). */
    assert(rte_dual_msgchannel_reset_sequence(&channel_a) == RTE_STATUS_OK);
    assert(rte_dual_msgchannel_reset_sequence(&channel_b) == RTE_STATUS_OK);

    /* One clean roundtrip: both counters now at 1. */
    assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"v", 1U, 10U, &out_sequence) == RTE_STATUS_OK);
    assert(out_sequence == 0U);
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == RTE_STATUS_OK);
    assert(out_sequence == 0U);

    /* Resetting only channel_a's counter (back to 0) desynchronizes it
     * from channel_b's (still expecting 1) - the next receive is
     * reported as RTE_STATUS_DATA_CORRUPTION rather than silently
     * accepted out of order. */
    assert(rte_dual_msgchannel_reset_sequence(&channel_a) == RTE_STATUS_OK);
    assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"y", 1U, 10U, &out_sequence) == RTE_STATUS_OK);
    assert(out_sequence == 0U);
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == RTE_STATUS_DATA_CORRUPTION);

    /* Resetting BOTH sides together (mirrors channel_ab.c's own
     * cycle-resync precedent: both ends reset after a fresh link
     * (re)establishment, not just one) recovers synchronization. */
    assert(rte_dual_msgchannel_reset_sequence(&channel_a) == RTE_STATUS_OK);
    assert(rte_dual_msgchannel_reset_sequence(&channel_b) == RTE_STATUS_OK);
    assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"w", 1U, 10U, &out_sequence) == RTE_STATUS_OK);
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == RTE_STATUS_OK);

    /* send(): payload NULL with a non-zero payload_size is rejected
     * without ever reaching rte_checksum_vital_message_create() or
     * rte_netlink_send(). */
    assert(rte_dual_msgchannel_send(&channel_a, NULL, 1U, 10U, NULL) == RTE_STATUS_INVALID_PARAM);

    /* send(): rte_checksum_vital_message_create() itself rejects a
     * payload larger than sizeof(rte_vital_message_t::payload) (248
     * bytes) - this layer does not pre-check payload_size itself, that
     * validation is rte_dual_channel_t's job (RTE_DUAL_CHANNEL_MAX_PAYLOAD),
     * this layer just propagates whatever the checksum layer reports. */
    {
        uint8_t oversize_payload[255];

        (void)memset(oversize_payload, 0, sizeof(oversize_payload));
        assert(rte_dual_msgchannel_send(&channel_a, oversize_payload, (uint8_t)sizeof(oversize_payload), 10U, NULL)
               == RTE_STATUS_INVALID_PARAM);
    }

    /* send(): rte_netlink_send() itself failing (link down) is
     * propagated as-is, and next_sequence is left unadvanced (a retry
     * would reuse the same sequence_number). */
    {
        uint32_t seq_before_failed_send = channel_a.next_sequence;

        link_a.broken = 1;
        assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"q", 1U, 10U, NULL) == RTE_STATUS_TIMEOUT);
        assert(channel_a.next_sequence == seq_before_failed_send);
        link_a.broken = 0;
    }

    /* receive(): NULL/zero-size argument rejection. */
    assert(rte_dual_msgchannel_receive(NULL, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == RTE_STATUS_INVALID_PARAM);
    assert(rte_dual_msgchannel_receive(&channel_b, NULL, sizeof(out_payload), 10U, &out_payload_size, &out_sequence)
           == RTE_STATUS_INVALID_PARAM);
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, NULL, &out_sequence)
           == RTE_STATUS_INVALID_PARAM);
    assert(rte_dual_msgchannel_receive(&channel_b, out_payload, 0U, 10U, &out_payload_size, &out_sequence)
           == RTE_STATUS_INVALID_PARAM);

    /* reset_sequence(): NULL argument rejection. */
    assert(rte_dual_msgchannel_reset_sequence(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_dual_msgchannel_set_resync_on_sequence_error(NULL, true) == RTE_STATUS_INVALID_PARAM);

    /* Sequence resync: A restarts (sequence back to 0) while B keeps expecting the old number. Without the resync B
     * rejects A for good; with it B rejects one frame, then follows A's numbering. Off by default. */
    {
        int i;

        assert(rte_dual_msgchannel_reset_sequence(&channel_a) == RTE_STATUS_OK); /* A restarted: next_sequence 0 */
        channel_b.expected_sequence = 100U; /* B has been running for a long time */
        for (i = 0; i < 3; i++)
        {
            assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"r", 1U, 10U, NULL) == RTE_STATUS_OK);
            assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                                 &out_sequence) == RTE_STATUS_DATA_CORRUPTION);
        }
        assert(rte_dual_msgchannel_set_resync_on_sequence_error(&channel_b, true) == RTE_STATUS_OK);
        assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"r", 1U, 10U, NULL) == RTE_STATUS_OK);
        assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                             &out_sequence) == RTE_STATUS_DATA_CORRUPTION); /* still rejected once */
        assert(rte_dual_msgchannel_send(&channel_a, (const uint8_t *)"s", 1U, 10U, NULL) == RTE_STATUS_OK);
        assert(rte_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                             &out_sequence) == RTE_STATUS_OK);
        assert(out_payload[0] == (uint8_t)'s');
    }

    return 0;
}
