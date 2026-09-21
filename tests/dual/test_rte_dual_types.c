/* Tests for rte_dual_state_to_string()/rte_dual_channel_status_to_string()
 * (REQ-DUAL-TYPES-001): every defined enum value maps to its documented
 * string, and an out-of-range value (defensive only - not reachable
 * through the public enum, but C does not enforce enum range at the type
 * level) maps to the "UNKNOWN_STATE"/"UNKNOWN_STATUS" default branch.
 *
 * Style follows tests/status/test_rte_status.c / tests/log/test_rte_log.c:
 * plain assert()-based main(), no framework, no dynamic memory.
 */
#include <assert.h>
#include <string.h>
#include "safeapi/redundancy/dual/rte_dual_types.h"

int main(void)
{
    /* rte_dual_state_to_string(): every defined value. */
    assert(strcmp(rte_dual_state_to_string(RTE_DUAL_STATE_IDLE), "IDLE") == 0);
    assert(strcmp(rte_dual_state_to_string(RTE_DUAL_STATE_UNKNOWN), "UNKNOWN") == 0);
    assert(strcmp(rte_dual_state_to_string(RTE_DUAL_STATE_ONLINE), "ONLINE") == 0);
    assert(strcmp(rte_dual_state_to_string(RTE_DUAL_STATE_HOTSTANDBY), "HOTSTANDBY") == 0);
    assert(strcmp(rte_dual_state_to_string(RTE_DUAL_STATE_COLDSTANDBY), "COLDSTANDBY") == 0);
    /* Defensive default: out-of-range value via explicit cast. */
    assert(strcmp(rte_dual_state_to_string((rte_dual_state_t)99), "UNKNOWN_STATE") == 0);

    /* rte_dual_channel_status_to_string(): every defined value. */
    assert(strcmp(rte_dual_channel_status_to_string(RTE_DUAL_CHANNEL_STATUS_DOWN), "DOWN") == 0);
    assert(strcmp(rte_dual_channel_status_to_string(RTE_DUAL_CHANNEL_STATUS_DEGRADED), "DEGRADED") == 0);
    assert(strcmp(rte_dual_channel_status_to_string(RTE_DUAL_CHANNEL_STATUS_FULL), "FULL") == 0);
    /* Defensive default: out-of-range value via explicit cast. */
    assert(strcmp(rte_dual_channel_status_to_string((rte_dual_channel_status_t)99), "UNKNOWN_STATUS") == 0);

    return 0;
}
