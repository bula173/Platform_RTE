/**
 * @file rte_state_transfer.h
 * @brief Declarative registry of application state that must survive a
 *        STANDBY -> ONLINE promotion (RCA/OCORA compatibility initiative).
 *
 * Added per direct request as a generalization of what
 * safeAPIRBC2oo2GP/src/application/AB/GP/com/ab_gp_channel_negotiate.c
 * hand-rolls today: a fixed `negotiate_extra_payload_t` struct plus manual
 * byte-offset encode/decode functions naming GP's own `sessions[]`/`db`
 * fields directly. This module is the Platform-owned equivalent - GP/GA
 * REGISTER which of their own data fields must be transferred (name,
 * pointer, size) instead of the framework (or GP's own negotiation code)
 * hardcoding that list. See docs/rca/ at the workspace root and TODO.md's
 * "RCA/OCORA compatibility" section.
 *
 * This module does NOT decide WHEN a transfer happens, or over what
 * transport - it only knows how to pack/unpack whatever has been
 * registered into/from one bounds-checked buffer. Cadence (hot/warm/cold)
 * is rte_redundancy_config_t::standby_mode's job; carrying the encoded
 * buffer across the wire remains the caller's own negotiation-link
 * transport (e.g. ab_gp_channel_negotiate.c's own extra-payload slot).
 *
 * REQ-STATE-TRANSFER-001: no dynamic allocation; a fixed-size static field
 *                         table, caller-owned buffers for encode/decode.
 * REQ-STATE-TRANSFER-002: fields are packed/unpacked in registration order,
 *                         byte-for-byte, no padding inserted between them.
 *
 * @defgroup state_transfer State transfer registry (hot/warm/cold standby)
 * @{
 */
#ifndef RTE_STATE_TRANSFER_H
#define RTE_STATE_TRANSFER_H

#include <stdint.h>
#include <stddef.h>
#include "safeapi/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Largest number of fields one registry can hold. */
#define RTE_STATE_TRANSFER_MAX_FIELDS 8U

/**
 * @brief Optional custom wire encoder for one field, registered via
 *        rte_state_transfer_register_field_ex() - e.g. a field whose live
 *        in-memory struct has compiler-inserted padding this module's
 *        default raw memcpy would transfer verbatim (uninitialized bytes),
 *        or whose wire form is genuinely smaller than its live struct
 *        (index/count fields the wire form omits). Must write EXACTLY the
 *        field's registered `size` bytes to out - the same fixed-size
 *        contract rte_state_transfer_encode()'s own offset bookkeeping
 *        already assumes for a plain (non-callback) field.
 * @param[in]  data  The field's own live data pointer, as registered.
 * @param[out] out   Destination for exactly `size` encoded bytes.
 * @param[in]  size  The field's own registered size (the exact byte count
 *                   to write - NOT necessarily sizeof(*data)).
 * @return RTE_STATUS_OK on success; any other value aborts the whole
 *         rte_state_transfer_encode() call with that status.
 */
typedef rte_status_t (*rte_state_field_encode_fn)(const void *data, uint8_t *out, size_t size);

/**
 * @brief Inverse of rte_state_field_encode_fn - must consume EXACTLY
 *        `size` bytes from in and write the result into *data.
 * @return RTE_STATUS_OK on success; any other value aborts the whole
 *         rte_state_transfer_decode() call with that status.
 */
typedef rte_status_t (*rte_state_field_decode_fn)(void *data, const uint8_t *in, size_t size);

/** One piece of application state to carry across a promotion. */
typedef struct {
    /** Diagnostic/logging name only (e.g. "sessions", "db") - not part of
     *  the encoded wire form, never compared on decode. Caller-owned
     *  pointer (a string literal is the common case); not copied. */
    const char *name;
    /** Pointer to the live data. Caller-owned; must remain valid for the
     *  registry's whole lifetime (this module never allocates its own
     *  copy). Passed to encode_fn/decode_fn as-is when either is set -
     *  this module never dereferences it directly in that case. */
    void       *data;
    /** The exact number of bytes this field occupies in the encoded form.
     *  For a plain field (encode_fn/decode_fn both NULL) this is also
     *  sizeof(*data), and encode/decode is a raw memcpy. For a custom
     *  field it is whatever encode_fn/decode_fn's own wire format needs -
     *  may differ from sizeof(*data) (e.g. a struct with padding, or a
     *  wire form narrower than its live representation). Must be > 0. */
    size_t      size;
    /** NULL for a plain (raw memcpy) field. See rte_state_field_encode_fn. */
    rte_state_field_encode_fn encode_fn;
    /** NULL for a plain (raw memcpy) field. See rte_state_field_decode_fn.
     *  Must be NULL iff encode_fn is NULL (both-or-neither, checked by
     *  rte_state_transfer_register_field_ex()). */
    rte_state_field_decode_fn decode_fn;
} rte_state_field_t;

/** Caller-owned storage for one registry instance. */
typedef struct {
    rte_state_field_t fields[RTE_STATE_TRANSFER_MAX_FIELDS];
    uint32_t            field_count;
} rte_state_transfer_registry_t;

/**
 * @brief Initializes an empty registry.
 * @param[out] registry  Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if registry is NULL.
 */
rte_status_t rte_state_transfer_registry_init(rte_state_transfer_registry_t *registry);

/**
 * @brief Registers one field. Order matters - encode()/decode() pack/unpack
 *        in registration order.
 * @param[in,out] registry  Must not be NULL.
 * @param[in]     name      May be NULL (diagnostic only).
 * @param[in]     data      Must not be NULL.
 * @param[in]     size      Must be > 0.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM (NULL registry/data, or
 *         size 0); RTE_STATUS_RESOURCE_EXHAUSTED if the registry already
 *         holds RTE_STATE_TRANSFER_MAX_FIELDS entries.
 */
rte_status_t rte_state_transfer_register_field(rte_state_transfer_registry_t *registry,
                                                  const char *name, void *data, size_t size);

/**
 * @brief Registers one field with a custom wire encoder/decoder pair
 *        instead of this module's default raw memcpy - see
 *        rte_state_field_t's own doc for when this is needed (e.g. a
 *        field whose live struct has padding, or a wire form narrower
 *        than its live representation).
 * @param[in,out] registry   Must not be NULL.
 * @param[in]     name       May be NULL (diagnostic only).
 * @param[in]     data       Must not be NULL.
 * @param[in]     size       The field's exact encoded byte count. Must be > 0.
 * @param[in]     encode_fn  Must not be NULL.
 * @param[in]     decode_fn  Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM (any required pointer
 *         NULL, or size 0); RTE_STATUS_RESOURCE_EXHAUSTED as in
 *         rte_state_transfer_register_field().
 */
rte_status_t rte_state_transfer_register_field_ex(rte_state_transfer_registry_t *registry,
                                                     const char *name, void *data, size_t size,
                                                     rte_state_field_encode_fn encode_fn,
                                                     rte_state_field_decode_fn decode_fn);

/**
 * @brief Total encoded size (sum of every registered field's size) - the
 *        minimum buffer size encode()/decode() need.
 * @param[in] registry  Must not be NULL.
 * @return The total size, or 0 if registry is NULL or empty.
 */
size_t rte_state_transfer_encoded_size(const rte_state_transfer_registry_t *registry);

/**
 * @brief Packs every registered field's current live value into out, in
 *        registration order, back-to-back with no padding.
 * @param[in]  registry  Must not be NULL.
 * @param[out] out       Destination buffer. Must not be NULL.
 * @param[in]  out_size  Usable size of out; must be >=
 *                       rte_state_transfer_encoded_size(registry).
 * @param[out] out_len   Receives the number of bytes actually written
 *                       (== rte_state_transfer_encoded_size(registry) on
 *                       success). May be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM (NULL registry/out);
 *         RTE_STATUS_RESOURCE_EXHAUSTED if out_size is too small.
 */
rte_status_t rte_state_transfer_encode(const rte_state_transfer_registry_t *registry,
                                          uint8_t *out, size_t out_size, size_t *out_len);

/**
 * @brief Inverse of encode(): overwrites every registered field's live
 *        value from in, in registration order.
 * @param[in] registry  Must not be NULL.
 * @param[in] in        Source buffer. Must not be NULL.
 * @param[in] in_len    Usable size of in; must be >=
 *                      rte_state_transfer_encoded_size(registry).
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM (NULL registry/in);
 *         RTE_STATUS_RESOURCE_EXHAUSTED if in_len is too small.
 * @post On success, every registered field's live data equals the bytes
 *       encode() would have produced from in at that same offset - i.e.
 *       the live application state now matches whatever was encoded.
 */
rte_status_t rte_state_transfer_decode(const rte_state_transfer_registry_t *registry,
                                          const uint8_t *in, size_t in_len);

#ifdef __cplusplus
}
#endif

#endif /* RTE_STATE_TRANSFER_H */
/** @} */
