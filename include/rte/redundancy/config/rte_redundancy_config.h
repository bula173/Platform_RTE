/**
 * @file rte_redundancy_config.h
 * @brief Application-level redundancy topology configuration (2oo2 / 2oo2 with
 *        redundancy / 2oo3 / NMR), loaded from a small JSON file at startup.
 *
 * Added per direct request as part of the RCA/OCORA compatibility initiative
 * (see docs/rca/ at the workspace root, and TODO.md's "RCA/OCORA compatibility"
 * section). OCORA's own Safe Computing Platform model treats voting topology
 * (how many Replicas, what quorum) as a Platform-level concern the Functional
 * Application does not hardcode - this module is that Platform-level piece:
 * an integrator (e.g. safeAPIRBC2oo2GP) reads its intended topology from a
 * deployment-time JSON file instead of a compiled-in constant, then hands the
 * resolved quorum to rte_voter's own config (rte_redundancy_config_apply_to_voter()).
 *
 * This module does NOT create channels or replicas itself - it only resolves
 * "what topology, how many replicas, what quorum" from a file. Registering the
 * actual rte_channel_t links for each replica remains the integrator's job
 * (transport/addressing is deployment-specific, not something a generic
 * redundancy-topology file can express).
 *
 * JSON format (flat, fixed shape - this is a hand-rolled scanner for exactly
 * this schema, not a general JSON parser; a SIL4/MISRA C99 static library
 * avoids pulling in a third-party JSON library the same way
 * safecomm_config_load avoids one for its own "key = value" files):
 * @code
 * {
 *   "topology": "2oo3",
 *   "replicas": 3,
 *   "quorum": 2,
 *   "roles": ["A", "B", "C"]
 * }
 * @endcode
 *
 * "quorum" and "roles" are optional. When "quorum" is absent, a topology-based
 * default is used (see rte_redundancy_config_load()'s own doc). Key order in
 * the file does not matter; unrecognized keys are ignored.
 *
 * @defgroup redundancy_config Redundancy topology config
 * @{
 */
#ifndef RTE_REDUNDANCY_CONFIG_H
#define RTE_REDUNDANCY_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "rte/utils/status/rte_status.h"
#include "rte/redundancy/voter/rte_voter.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Longest role name (including terminator) accepted in the "roles" array. */
#define RTE_REDUNDANCY_CONFIG_MAX_ROLE_NAME_LEN 16U

/** Largest replica count (and "roles" array length) this module accepts. */
#define RTE_REDUNDANCY_CONFIG_MAX_REPLICAS 5U

/** Largest config file this loader will read - a bounded, static buffer,
 *  no dynamic allocation; a larger file is rejected as invalid. */
#define RTE_REDUNDANCY_CONFIG_MAX_FILE_SIZE 8192U

/** Largest channel count accepted in the "channels" array. */
#define RTE_CHANNEL_CONFIG_MAX_CHANNELS 32U

/** Longest channel name (including terminator). */
#define RTE_CHANNEL_CONFIG_MAX_NAME_LEN 32U

/** Longest host name (including terminator). */
#define RTE_CHANNEL_CONFIG_MAX_HOST_LEN 64U

/** Channel transport type. */
typedef enum {
    RTE_CHANNEL_TRANSPORT_FLOW = 0,
    RTE_CHANNEL_TRANSPORT_POSIX_NETLINK = 1,
    RTE_CHANNEL_TRANSPORT_DDS = 2
} rte_channel_transport_t;

/** Channel endpoint role. */
typedef enum {
    RTE_CHANNEL_ROLE_LISTEN = 0,
    RTE_CHANNEL_ROLE_CONNECT = 1
} rte_channel_role_t;

/** Definition of a single communication channel. */
typedef struct {
    uint32_t                 id;
    char                     name[RTE_CHANNEL_CONFIG_MAX_NAME_LEN];
    rte_channel_transport_t transport;
    rte_channel_role_t      role;
    char                     host[RTE_CHANNEL_CONFIG_MAX_HOST_LEN];
    uint16_t                 port;
    uint32_t                 message_size;
    uint32_t                 connect_timeout_ms;
} rte_channel_def_t;

/** Voting/replication topology, matching the OCORA/RCA-discussed options. */
typedef enum {
    /** Two replicas, pairwise 2-way comparison (rte_cross_comparator). */
    RTE_REDUNDANCY_TOPOLOGY_2OO2 = 0,
    /** Two active replicas plus one or more hot-standby/redundant replicas
     *  not in the voting quorum today - replica_count may exceed 2, but
     *  quorum_size stays 2 unless the file overrides it. */
    RTE_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT = 1,
    /** Three replicas, 2-out-of-3 quorum voting (rte_voter). OCORA's own
     *  recommended topology for RBC use-cases over plain 2oo2. */
    RTE_REDUNDANCY_TOPOLOGY_2OO3 = 2,
    /** N-modular redundancy: replica_count is N, quorum_size is whatever the
     *  file specifies (no implicit majority default - see load()'s doc). */
    RTE_REDUNDANCY_TOPOLOGY_NMR = 3
} rte_redundancy_topology_t;

/**
 * @brief How a non-active replica ("standby") keeps itself ready to take
 *        over, and how rte_state_transfer (redundancy/state_transfer/
 *        rte_state_transfer.h) gets used, if at all. This is a CADENCE
 *        policy only - it says nothing about WHAT data is registered
 *        (that stays the integrator's own rte_state_transfer_register_field()
 *        calls) or how a promotion is detected (that stays e.g.
 *        rte_dual_negotiator_t's own job).
 */
typedef enum {
    /** Standby computes independently every cycle from the same live
     *  inputs as the active replica, so it is never behind - no transfer
     *  is needed at all, ever. The integrator simply never calls
     *  rte_state_transfer_encode()/_decode() in this mode. */
    RTE_STANDBY_MODE_HOT = 0,
    /** Standby is periodically refreshed (an integrator-chosen interval,
     *  not specified by this module) so a promotion only needs a short
     *  catch-up, not a full cold transfer. */
    RTE_STANDBY_MODE_WARM = 1,
    /** Standby is not refreshed until the moment of promotion - the
     *  default, and the ONLY mode this workspace's current
     *  safeAPIRBC2oo2GP actually implements today (see that project's
     *  ab_gp_channel_negotiate.c apply_state_transfer()). */
    RTE_STANDBY_MODE_COLD = 2
} rte_standby_mode_t;

/** Resolved redundancy configuration for one Functional Actor deployment. */
typedef struct {
    /** Selected topology. */
    rte_redundancy_topology_t topology;
    /** Number of replicas (channels) the integrator is expected to register. */
    uint32_t replica_count;
    /** Quorum size to pass into rte_voter_config_t::quorum_size. */
    uint32_t quorum_size;
    /** Standby cadence policy. Defaults to RTE_STANDBY_MODE_COLD when
     *  "standby_mode" is absent from the file (matches today's only
     *  actually-implemented behavior - see the enum's own doc). */
    rte_standby_mode_t standby_mode;
    /** Number of entries populated in roles[] below (0 if "roles" was absent
     *  from the file - role naming is optional, purely diagnostic). */
    uint32_t role_count;
    /** Optional per-replica role labels (e.g. "A", "B", "C"), diagnostic only -
     *  not consumed by rte_voter itself. */
    char roles[RTE_REDUNDANCY_CONFIG_MAX_REPLICAS][RTE_REDUNDANCY_CONFIG_MAX_ROLE_NAME_LEN];
    /** Number of entries populated in channels[] below. */
    uint32_t channel_count;
    /** Resolved channel definitions from configuration. */
    rte_channel_def_t channels[RTE_CHANNEL_CONFIG_MAX_CHANNELS];
} rte_redundancy_config_t;

/**
 * @brief Parse a standby-mode name string ("hot", "warm", "cold") into its
 *        enum value.
 * @param[in]  name      NUL-terminated; case-sensitive.
 * @param[out] out_mode  Set on success; untouched on failure.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM (NULL pointer or
 *         unrecognized name).
 */
rte_status_t rte_redundancy_config_standby_mode_from_string(const char *name, rte_standby_mode_t *out_mode);

/**
 * @brief Inverse of rte_redundancy_config_standby_mode_from_string() - for
 *        logging/diagnostics.
 * @param[in] mode  Any value of rte_standby_mode_t.
 * @return A static, NUL-terminated string; "unknown" for an out-of-range value.
 */
const char *rte_redundancy_config_standby_mode_to_string(rte_standby_mode_t mode);

/**
 * @brief Parse a topology name string ("2oo2", "2oo2_redundant", "2oo3", "nmr")
 *        into its enum value.
 * @param[in]  name          NUL-terminated topology name; case-sensitive,
 *                            exactly one of the four strings above.
 * @param[out] out_topology  Set on success; untouched on failure.
 * @return RTE_STATUS_OK, RTE_STATUS_INVALID_PARAM (NULL pointer or
 *         unrecognized name).
 */
rte_status_t rte_redundancy_config_topology_from_string(const char *name,
                                                           rte_redundancy_topology_t *out_topology);

/**
 * @brief Inverse of rte_redundancy_config_topology_from_string() - for
 *        logging/diagnostics.
 * @param[in] topology  Any value of rte_redundancy_topology_t.
 * @return A static, NUL-terminated string; "unknown" for an out-of-range value.
 */
const char *rte_redundancy_config_topology_to_string(rte_redundancy_topology_t topology);

/**
 * @brief Integrator-supplied capability query: can THIS application
 *        actually run the given topology/replica_count combination? This
 *        is how an integrator (e.g. safeAPIRBC2oo2GP) declares what it
 *        supports to the Platform, instead of the Platform (or the
 *        integrator's own call site) hardcoding a topology allowlist -
 *        matches OCORA's own "Platform decides" posture already used
 *        throughout this module and rte_voter/rte_cross_comparator's
 *        own safestate-transition ownership.
 * @param[in] topology       The loaded config's topology.
 * @param[in] replica_count  The loaded config's replica_count.
 * @param[in] context        Whatever rte_redundancy_config_register_capability()
 *                           was given.
 * @return true if this application can run this combination; false
 *         otherwise (rte_redundancy_config_load() then fails with
 *         RTE_STATUS_NOT_SUPPORTED instead of returning RTE_STATUS_OK).
 */
typedef bool (*rte_redundancy_capability_fn)(rte_redundancy_topology_t topology, uint32_t replica_count,
                                               void *context);

/**
 * @brief Registers the capability callback rte_redundancy_config_load()
 *        consults after successfully parsing a file, before returning
 *        RTE_STATUS_OK. Call once at startup, before load(). No callback
 *        registered (the default) means load() accepts any
 *        successfully-parsed, internally-consistent config - the same
 *        behavior this module had before this function existed.
 * @param[in] fn       May be NULL to clear a previously registered callback
 *                     (load() then accepts any config again).
 * @param[in] context  Passed verbatim to fn on every call; may be NULL.
 * @return RTE_STATUS_OK always.
 */
rte_status_t rte_redundancy_config_register_capability(rte_redundancy_capability_fn fn, void *context);

/**
 * @brief Load a redundancy topology config from a JSON file.
 *
 * @param[in]  path        NUL-terminated filesystem path.
 * @param[out] out_config  Filled on success; untouched on failure.
 * @return RTE_STATUS_OK on success.
 *         RTE_STATUS_INVALID_PARAM - path or out_config is NULL.
 *         RTE_STATUS_INTERNAL_ERROR - file could not be opened/read, or exceeds
 *         RTE_REDUNDANCY_CONFIG_MAX_FILE_SIZE.
 *         RTE_STATUS_INVALID_STATE - file content does not match the
 *         expected schema (missing/malformed "topology" or "replicas", a
 *         "roles" array longer than RTE_REDUNDANCY_CONFIG_MAX_REPLICAS, a
 *         role name longer than RTE_REDUNDANCY_CONFIG_MAX_ROLE_NAME_LEN-1,
 *         or replica_count/quorum_size that fail sanity checks below).
 *         RTE_STATUS_NOT_SUPPORTED - the file parsed and validated fine,
 *         but a registered rte_redundancy_capability_fn returned false for
 *         it (see rte_redundancy_config_register_capability()).
 *
 * @post On success: 1 <= replica_count <= RTE_REDUNDANCY_CONFIG_MAX_REPLICAS,
 *       1 <= quorum_size <= replica_count.
 *
 * Default quorum_size when "quorum" is absent from the file:
 * - RTE_REDUNDANCY_TOPOLOGY_2OO2 / _2OO2_REDUNDANT: 2 (rejected if
 *   replica_count < 2).
 * - RTE_REDUNDANCY_TOPOLOGY_2OO3: 2 (rejected if replica_count != 3).
 * - RTE_REDUNDANCY_TOPOLOGY_NMR: no default - "quorum" is REQUIRED in the
 *   file for this topology (RTE_STATUS_INVALID_STATE if absent), since NMR's
 *   whole point is a deployment-chosen quorum, not an implicit majority.
 *
 * "standby_mode" ("hot"/"warm"/"cold") is optional; defaults to
 * RTE_STANDBY_MODE_COLD when absent (see that enum's own doc).
 */
rte_status_t rte_redundancy_config_load(const char *path, rte_redundancy_config_t *out_config);

/**
 * @brief Copy the resolved quorum from a loaded redundancy config into a
 *        rte_voter_config_t, leaving every other field of voter_cfg
 *        untouched (compare()/context/timeouts/safestate fields remain the
 *        caller's own responsibility to set).
 * @param[in]     config     A config previously filled by
 *                            rte_redundancy_config_load(), non-NULL.
 * @param[in,out] voter_cfg  Non-NULL; only ::quorum_size is written.
 * @return RTE_STATUS_OK, RTE_STATUS_INVALID_PARAM (either pointer is NULL).
 */
rte_status_t rte_redundancy_config_apply_to_voter(const rte_redundancy_config_t *config,
                                                     rte_voter_config_t *voter_cfg);

/**
 * @brief Parse transport name string ("flow", "netlink", "posix", "dds") into enum.
 */
rte_status_t rte_channel_config_transport_from_string(const char *name, rte_channel_transport_t *out_transport);

/**
 * @brief Parse channel role name string ("listen", "connect", "publisher", "subscriber") into enum.
 */
rte_status_t rte_channel_config_role_from_string(const char *name, rte_channel_role_t *out_role);

/**
 * @brief Find channel definition by name in the given configuration.
 */
rte_status_t rte_redundancy_config_find_channel_by_name(const rte_redundancy_config_t *config,
                                                          const char *name,
                                                          rte_channel_def_t *out_channel);

/**
 * @brief Find channel definition by ID in the given configuration.
 */
rte_status_t rte_redundancy_config_find_channel_by_id(const rte_redundancy_config_t *config,
                                                        uint32_t id,
                                                        rte_channel_def_t *out_channel);

/**
 * @brief Get the currently active loaded redundancy/channel configuration (if any).
 */
const rte_redundancy_config_t *rte_redundancy_config_get_active(void);

/**
 * @brief Set the active configuration explicitly (or automatically by rte_redundancy_config_load).
 */
void rte_redundancy_config_set_active(const rte_redundancy_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* RTE_REDUNDANCY_CONFIG_H */
/** @} */
