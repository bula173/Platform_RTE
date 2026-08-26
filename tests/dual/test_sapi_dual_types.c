/* Tests for sapi_dual_state_to_string()/sapi_dual_channel_status_to_string()
 * (REQ-DUAL-TYPES-001): every defined enum value maps to its documented
 * string, and an out-of-range value (defensive only - not reachable
 * through the public enum, but C does not enforce enum range at the type
 * level) maps to the "UNKNOWN_STATE"/"UNKNOWN_STATUS" default branch.
 *
 * Style follows tests/status/test_sapi_status.c / tests/log/test_sapi_log.c:
 * plain assert()-based main(), no framework, no dynamic memory.
 */
#include <assert.h>
#include <string.h>
#include "safeapi/redundancy/dual/sapi_dual_types.h"

int main(void)
{
    /* sapi_dual_state_to_string(): every defined value. */
    assert(strcmp(sapi_dual_state_to_string(SAPI_DUAL_STATE_IDLE), "IDLE") == 0);
    assert(strcmp(sapi_dual_state_to_string(SAPI_DUAL_STATE_UNKNOWN), "UNKNOWN") == 0);
    assert(strcmp(sapi_dual_state_to_string(SAPI_DUAL_STATE_ONLINE), "ONLINE") == 0);
    assert(strcmp(sapi_dual_state_to_string(SAPI_DUAL_STATE_HOTSTANDBY), "HOTSTANDBY") == 0);
    assert(strcmp(sapi_dual_state_to_string(SAPI_DUAL_STATE_COLDSTANDBY), "COLDSTANDBY") == 0);
    /* Defensive default: out-of-range value via explicit cast. */
    assert(strcmp(sapi_dual_state_to_string((sapi_dual_state_t)99), "UNKNOWN_STATE") == 0);

    /* sapi_dual_channel_status_to_string(): every defined value. */
    assert(strcmp(sapi_dual_channel_status_to_string(SAPI_DUAL_CHANNEL_STATUS_DOWN), "DOWN") == 0);
    assert(strcmp(sapi_dual_channel_status_to_string(SAPI_DUAL_CHANNEL_STATUS_DEGRADED), "DEGRADED") == 0);
    assert(strcmp(sapi_dual_channel_status_to_string(SAPI_DUAL_CHANNEL_STATUS_FULL), "FULL") == 0);
    /* Defensive default: out-of-range value via explicit cast. */
    assert(strcmp(sapi_dual_channel_status_to_string((sapi_dual_channel_status_t)99), "UNKNOWN_STATUS") == 0);

    return 0;
}
