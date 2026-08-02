/**
 * @file sapi_safestate.c
 * @brief Implementation of the safe-state transition facility (ADR-004).
 */
#include "safeapi/safestate/sapi_safestate.h"

/** One handler slot per sapi_safestate_level_t value; no dynamic allocation. */
static sapi_safestate_handler_t s_handlers[3] = { NULL, NULL, NULL };

/**
 * @brief Maps a level enumerator to its handler-array index.
 * @return true and sets *out_index if level is a known value; false
 *         (index left unset) otherwise.
 */
static bool sapi_safestate_level_to_index(sapi_safestate_level_t level, size_t *out_index)
{
    bool found;

    switch (level)
    {
        case SAPI_SAFESTATE_LEVEL_DEGRADED:
            *out_index = 0U;
            found = true;
            break;
        case SAPI_SAFESTATE_LEVEL_SAFE:
            *out_index = 1U;
            found = true;
            break;
        case SAPI_SAFESTATE_LEVEL_REBOOT:
            *out_index = 2U;
            found = true;
            break;
        default:
            found = false;
            break;
    }
    return found;
}

sapi_status_t sapi_safestate_register_handler(sapi_safestate_level_t level,
                                               sapi_safestate_handler_t handler)
{
    size_t index;

    if ((handler == NULL) || (!sapi_safestate_level_to_index(level, &index)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_handlers[index] = handler;
    return SAPI_STATUS_OK;
}

void sapi_safestate_enter(sapi_safestate_level_t level,
                           sapi_safestate_reason_t reason,
                           const char *file,
                           int32_t line,
                           const char *message)
{
    size_t index;
    bool valid_level = sapi_safestate_level_to_index(level, &index);
    sapi_safestate_handler_t handler = NULL;

    if (valid_level)
    {
        handler = s_handlers[index];
    }

    if (handler != NULL)
    {
        handler(level, reason, file, line, message);
    }

    /* REQ-COMMON-SAFESTATE-002: SAFE and REBOOT never return control to the
     * caller - not even if the handler above misbehaved and returned, and
     * not even if no handler was registered at all. An unrecognized level
     * value is itself treated as a fault and handled the same defensive
     * way, since a corrupted `level` argument cannot be trusted to mean
     * DEGRADED. */
    if ((!valid_level) || (level == SAPI_SAFESTATE_LEVEL_SAFE) || (level == SAPI_SAFESTATE_LEVEL_REBOOT))
    {
        for (;;)
        {
            /* Defensive halt: intentionally never returns. */
        }
    }
}
