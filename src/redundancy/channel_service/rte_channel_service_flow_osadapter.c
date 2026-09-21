/** @file rte_channel_service_flow_osadapter.c
 *  @brief See rte_channel_service_flow_osadapter.h.
 *
 * **Lazy open, not eager** (found live, not designed up front - see the
 * RCA/OCORA Phase 4 verification history in root TODO.md): rte_flow_open()
 * blocks synchronously for its peer's handshake, up to config->open_timeout_ms
 * (REQ-OAL-FLOW-002) - a real behavioral difference from
 * safeAPIBackendPosix's own rte_posix_osadapter_channel_service, whose
 * osadapter_setup() fires the connect/HELLO (or binds, for LISTEN) and
 * returns immediately, deferring the actual peer handshake to the first
 * osadapter_read() call, retried indefinitely and silently in the background
 * from then on. That difference matters for real: an integrator (e.g.
 * safeAPIRBC2oo2GP) may legitimately set up a channel whose peer is not
 * running yet, or never runs in a given deployment (e.g. an optional
 * service) - the POSIX OSAdapter tolerates that gracefully (setup() never
 * blocks on it), this OSAdapter's naive first version did not (setup() would
 * block for the full open_timeout_ms, then FAIL setup() outright, taking
 * down the whole integrator process even though the missing peer was
 * expected and harmless). Fixed by matching the POSIX OSAdapter's own
 * lazy-connect shape: osadapter_setup() only resolves and stores
 * configuration; the actual rte_flow_open() call happens lazily, on the
 * first osadapter_read()/backend_send(), using a bounded 0ms probe timeout so
 * a still-absent peer returns RTE_STATUS_TIMEOUT immediately rather than
 * blocking that cycle's I/O - the caller's own retry loop (e.g.
 * safeAPIRBC2oo2GP's channel_service_setup_with_retry(), or simply calling
 * read()/send() again next cycle) is what re-attempts the open, exactly the
 * same shape the POSIX OSAdapter's own osadapter_read() reconnect-on-demand
 * logic already has.
 */
#include "safeapi/redundancy/channel_service/rte_channel_service_flow_osadapter.h"

#include <string.h>

#include "safeapi/oal/flow/rte_flow.h"
#include "safeapi/oal/memory/rte_mem_util.h"
#include "safeapi/redundancy/config/rte_redundancy_config.h"

/* Only the fields ensure_open() needs to rebuild a rte_flow_config_t are
 * stored (not the whole struct) - this OSAdapter's storage must fit inside
 * rte_channel_service_storage_t's 128-byte reserved size alongside
 * rte_flow_storage_t (64 bytes) itself. */
typedef struct
{
    rte_flow_storage_t   flow_storage;
    rte_flow_handle_t    flow_handle;
    uint32_t              oflags;
    size_t                message_size;
    char                  name_buf[RTE_CHANNEL_SERVICE_NAME_SIZE];
    rte_duration_ms_t    default_timeout_ms;
    bool                  resolved;   /**< setup() succeeded; the fields above are valid. */
    bool                  opened;     /**< flow_handle is valid (lazy-opened). */
} flow_channel_state_t;

typedef char flow_channel_state_fits_[(sizeof(flow_channel_state_t) <=
                                        sizeof(rte_channel_service_storage_t)) ? 1 : -1];

static rte_channel_service_flow_resolve_fn s_resolver;
static void *s_resolver_context;

static flow_channel_state_t *channel_state(rte_channel_service_storage_t *storage)
{
    return (flow_channel_state_t *)(void *)storage;
}

/** CONNECT->PUBLISHER, LISTEN->SUBSCRIBER, matching safeAPIFlowBackendDDS's
 *  own oflags->netlink-role mapping in reverse (see that project's file
 *  header comment) - kept here, not there, since this module must not
 *  depend on any concrete rte_osadapter_flow_t implementation. */
static uint32_t oflags_for_role(rte_netlink_role_t role)
{
    return (role == RTE_NETLINK_ROLE_CONNECT) ? (uint32_t)RTE_FLOW_O_PUBLISHER
                                                : (uint32_t)RTE_FLOW_O_SUBSCRIBER;
}

static rte_status_t osadapter_setup(rte_channel_service_storage_t *storage, const char *channel_name)
{
    flow_channel_state_t *state;
    rte_netlink_config_t resolved;
    rte_status_t status;

    if ((storage == NULL) || (channel_name == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    state = channel_state(storage);
    rte_mem_set(state, 0, sizeof(*state));

    rte_mem_set(&resolved, 0, sizeof(resolved));
    if (s_resolver != NULL)
    {
        status = s_resolver(channel_name, &resolved, s_resolver_context);
        if (status != RTE_STATUS_OK)
        {
            return status;
        }
    }
    else
    {
        const rte_redundancy_config_t *active = rte_redundancy_config_get_active();
        rte_channel_def_t def;
        if (active == NULL)
        {
            return RTE_STATUS_NOT_INITIALIZED;
        }
        status = rte_redundancy_config_find_channel_by_name(active, channel_name, &def);
        if (status != RTE_STATUS_OK)
        {
            return status;
        }
        resolved.role = (def.role == RTE_CHANNEL_ROLE_CONNECT) ? RTE_NETLINK_ROLE_CONNECT : RTE_NETLINK_ROLE_LISTEN;
        resolved.host = (def.host[0] != '\0') ? def.host : NULL;
        resolved.port = def.port;
        resolved.message_size = def.message_size;
        resolved.connect_timeout_ms = def.connect_timeout_ms;
    }

    (void)strncpy(state->name_buf, channel_name, sizeof(state->name_buf) - 1U);
    state->oflags = oflags_for_role(resolved.role);
    state->message_size = resolved.message_size;
    state->default_timeout_ms = resolved.connect_timeout_ms;
    state->resolved = true;
    state->opened = false;
    return RTE_STATUS_OK;
}

/** Attempts the deferred rte_flow_open() exactly once, bounded by
 *  open_timeout_ms - the SAME timeout_ms the caller passed to THIS
 *  read()/send() call, matching safeAPIBackendPosix's own osadapter_read()
 *  reconnect-on-demand logic exactly (it hands the caller's own read
 *  timeout to its ACK-consumption poll(), not a separate hardcoded value -
 *  see that file's osadapter_read()). A caller sees RTE_STATUS_TIMEOUT (or
 *  another rte_flow_open() failure) as an ordinary per-call read()/send()
 *  failure, not a setup()-time one. Using the caller's own timeout (not a
 *  hardcoded 0) matters for real: a genuinely-present peer (e.g. a
 *  loopback peer that is also just starting up) needs a real, non-zero
 *  window to complete its handshake - 0 would starve it just as
 *  thoroughly as an absent peer, found live against a real GP process
 *  (ab-peer briefly failing the same way ab-relay-util legitimately does)
 *  before this was corrected - see root TODO.md's Phase 4 entry. */
static rte_status_t ensure_open(flow_channel_state_t *state, rte_duration_ms_t open_timeout_ms)
{
    rte_flow_config_t flow_cfg;
    rte_status_t status;

    if (state->opened)
    {
        return RTE_STATUS_OK;
    }
    rte_mem_set(&flow_cfg, 0, sizeof(flow_cfg));
    flow_cfg.name = state->name_buf;
    flow_cfg.oflags = state->oflags;
    flow_cfg.message_size = state->message_size;
    flow_cfg.open_timeout_ms = (open_timeout_ms > 0U) ? open_timeout_ms : state->default_timeout_ms;
    status = rte_flow_open(&state->flow_storage, &flow_cfg, &state->flow_handle);
    if (status == RTE_STATUS_OK)
    {
        state->opened = true;
    }
    return status;
}

static rte_status_t osadapter_read(rte_channel_service_storage_t *storage, void *data, size_t data_size,
                                   rte_duration_ms_t timeout_ms)
{
    flow_channel_state_t *state;
    rte_status_t status;

    if ((storage == NULL) || (data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    state = channel_state(storage);
    if (!state->resolved)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    status = ensure_open(state, timeout_ms);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    return rte_flow_receive(state->flow_handle, data, data_size, NULL, timeout_ms);
}

static rte_status_t backend_send(rte_channel_service_storage_t *storage, const void *data, size_t data_size,
                                   rte_duration_ms_t timeout_ms)
{
    flow_channel_state_t *state;
    rte_status_t status;

    if ((storage == NULL) || (data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    state = channel_state(storage);
    if (!state->resolved)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    status = ensure_open(state, timeout_ms);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    return rte_flow_send(state->flow_handle, data, data_size, RTE_FLOW_CHANNEL_USER, timeout_ms);
}

static rte_status_t osadapter_close(rte_channel_service_storage_t *storage)
{
    flow_channel_state_t *state;
    rte_status_t status;

    if (storage == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    state = channel_state(storage);
    if (!state->opened)
    {
        state->resolved = false;
        return RTE_STATUS_OK;
    }
    status = rte_flow_close(state->flow_handle);
    state->opened = false;
    state->resolved = false;
    return status;
}

static const rte_osadapter_channel_service_t g_flow_osadapter = {
    osadapter_setup,
    osadapter_read,
    backend_send,
    osadapter_close
};

rte_status_t rte_channel_service_flow_osadapter_register_resolver(rte_channel_service_flow_resolve_fn resolver,
                                                                    void *context)
{
    if (resolver == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    s_resolver = resolver;
    s_resolver_context = context;
    /* Mirrors safeAPIBackendPosix's rte_posix_osadapter_channel_service_register_resolver()'s
     * own convention exactly - registering the resolver AND activating this
     * OSAdapter in one call - so an integrator's call site is a one-line swap
     * between the two OSAdapters (see ab_gp_channel.c). */
    return rte_osadapter_channel_service_register(&g_flow_osadapter);
}

const rte_osadapter_channel_service_t *rte_channel_service_flow_osadapter(void)
{
    return &g_flow_osadapter;
}
