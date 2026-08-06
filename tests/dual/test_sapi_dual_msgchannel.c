/* Tests for sapi_dual_msgchannel_t (ADR-020 Layer 1): send/receive
 * roundtrip over a mock sapi_netlink backend (two in-memory mailboxes,
 * following tests/netlink/test_sapi_netlink.c's own mock-backend
 * pattern), sequence continuity rejection, and sender_id masquerade
 * rejection. */
#include <assert.h>
#include <string.h>

#include "safeapi/checksum/sapi_checksum.h"
#include "safeapi/dual/sapi_dual_msgchannel.h"
#include "safeapi_backend/netlink/sapi_netlink_backend.h"

/* --- mock netlink backend: two single-slot mailboxes, wired A<->B --- */
typedef struct
{
    uint8_t buf[sizeof(sapi_vital_message_t)];
    size_t  size;
    int     has_data;
} mock_mailbox_t;

typedef struct
{
    mock_mailbox_t *inbox;  /* where this handle's receive() reads from */
    mock_mailbox_t *outbox; /* where this handle's send() writes to */
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

int main(void)
{
    mock_mailbox_t mailbox_a_to_b = { { 0 }, 0U, 0 };
    mock_mailbox_t mailbox_b_to_a = { { 0 }, 0U, 0 };
    mock_link_t    link_a         = { &mailbox_b_to_a, &mailbox_a_to_b };
    mock_link_t    link_b         = { &mailbox_a_to_b, &mailbox_b_to_a };

    sapi_dual_msgchannel_t channel_a;
    sapi_dual_msgchannel_t channel_b;
    sapi_dual_msgchannel_config_t cfg_a;
    sapi_dual_msgchannel_config_t cfg_b;

    uint8_t  out_payload[32];
    uint8_t  out_payload_size = 0U;
    uint32_t out_sequence     = 0U;

    assert(sapi_checksum_crc64_init(SAPI_CRC64_ERTMS) == SAPI_STATUS_OK);
    assert(sapi_netlink_register_backend(&g_mock_netlink_backend) == SAPI_STATUS_OK);

    cfg_a.link             = (sapi_netlink_handle_t)&link_a;
    cfg_a.sender_id        = 1U;
    cfg_a.expected_peer_id = 2U;
    assert(sapi_dual_msgchannel_init(&channel_a, &cfg_a) == SAPI_STATUS_OK);

    cfg_b.link             = (sapi_netlink_handle_t)&link_b;
    cfg_b.sender_id        = 2U;
    cfg_b.expected_peer_id = 1U;
    assert(sapi_dual_msgchannel_init(&channel_b, &cfg_b) == SAPI_STATUS_OK);

    /* NULL-argument rejection. */
    assert(sapi_dual_msgchannel_init(NULL, &cfg_a) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_dual_msgchannel_send(NULL, (const uint8_t *)"x", 1U, 10U, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* Basic roundtrip: A sends, B receives the same payload, sequence 0. */
    assert(sapi_dual_msgchannel_send(&channel_a, (const uint8_t *)"hello", 5U, 10U, &out_sequence) == SAPI_STATUS_OK);
    assert(out_sequence == 0U);
    assert(sapi_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == SAPI_STATUS_OK);
    assert(out_payload_size == 5U);
    assert(memcmp(out_payload, "hello", 5U) == 0);
    assert(out_sequence == 0U);

    /* No frame pending: TIMEOUT, not a hang. */
    assert(sapi_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == SAPI_STATUS_TIMEOUT);

    /* Second send advances the sequence; continuity check on B's side
     * (expected_sequence now 1) accepts it. */
    assert(sapi_dual_msgchannel_send(&channel_a, (const uint8_t *)"again", 5U, 10U, &out_sequence) == SAPI_STATUS_OK);
    assert(out_sequence == 1U);
    assert(sapi_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == SAPI_STATUS_OK);
    assert(out_sequence == 1U);

    /* Masquerade defense: a frame from an unexpected sender_id is
     * rejected as SAPI_STATUS_HARDWARE_FAULT, not silently delivered.
     * channel_a's own sender_id (1) does not match what channel_b
     * would need to see from a *different* peer - simulate by pointing
     * a fresh channel at B's inbox but expecting a peer_id B never uses. */
    {
        sapi_dual_msgchannel_t         wrong_expect;
        sapi_dual_msgchannel_config_t  wrong_cfg;

        wrong_cfg.link             = (sapi_netlink_handle_t)&link_b;
        wrong_cfg.sender_id        = 2U;
        wrong_cfg.expected_peer_id = 99U; /* A's real sender_id is 1, not 99 */
        assert(sapi_dual_msgchannel_init(&wrong_expect, &wrong_cfg) == SAPI_STATUS_OK);

        assert(sapi_dual_msgchannel_send(&channel_a, (const uint8_t *)"z", 1U, 10U, NULL) == SAPI_STATUS_OK);
        assert(sapi_dual_msgchannel_receive(&wrong_expect, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                             &out_sequence)
               == SAPI_STATUS_HARDWARE_FAULT);
    }

    /* Sequence continuity: reset both sides to a known-clean state first
     * (the masquerade test above consumed one frame off the wire via a
     * *different* sapi_dual_msgchannel_t instance, so channel_a/channel_b's
     * own counters are not both usefully known here without this). */
    assert(sapi_dual_msgchannel_reset_sequence(&channel_a) == SAPI_STATUS_OK);
    assert(sapi_dual_msgchannel_reset_sequence(&channel_b) == SAPI_STATUS_OK);

    /* One clean roundtrip: both counters now at 1. */
    assert(sapi_dual_msgchannel_send(&channel_a, (const uint8_t *)"v", 1U, 10U, &out_sequence) == SAPI_STATUS_OK);
    assert(out_sequence == 0U);
    assert(sapi_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == SAPI_STATUS_OK);
    assert(out_sequence == 0U);

    /* Resetting only channel_a's counter (back to 0) desynchronizes it
     * from channel_b's (still expecting 1) - the next receive is
     * reported as SAPI_STATUS_DATA_CORRUPTION rather than silently
     * accepted out of order. */
    assert(sapi_dual_msgchannel_reset_sequence(&channel_a) == SAPI_STATUS_OK);
    assert(sapi_dual_msgchannel_send(&channel_a, (const uint8_t *)"y", 1U, 10U, &out_sequence) == SAPI_STATUS_OK);
    assert(out_sequence == 0U);
    assert(sapi_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == SAPI_STATUS_DATA_CORRUPTION);

    /* Resetting BOTH sides together (mirrors channel_ab.c's own
     * cycle-resync precedent: both ends reset after a fresh link
     * (re)establishment, not just one) recovers synchronization. */
    assert(sapi_dual_msgchannel_reset_sequence(&channel_a) == SAPI_STATUS_OK);
    assert(sapi_dual_msgchannel_reset_sequence(&channel_b) == SAPI_STATUS_OK);
    assert(sapi_dual_msgchannel_send(&channel_a, (const uint8_t *)"w", 1U, 10U, &out_sequence) == SAPI_STATUS_OK);
    assert(sapi_dual_msgchannel_receive(&channel_b, out_payload, sizeof(out_payload), 10U, &out_payload_size,
                                         &out_sequence)
           == SAPI_STATUS_OK);

    return 0;
}
