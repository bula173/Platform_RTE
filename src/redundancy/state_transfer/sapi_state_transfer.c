/** @file sapi_state_transfer.c
 *  @brief See sapi_state_transfer.h.
 */
#include "safeapi/redundancy/state_transfer/sapi_state_transfer.h"

#include "safeapi/oal/memory/sapi_mem_util.h"

sapi_status_t sapi_state_transfer_registry_init(sapi_state_transfer_registry_t *registry)
{
    if (registry == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    sapi_mem_set(registry, 0, sizeof(*registry));
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_state_transfer_register_field(sapi_state_transfer_registry_t *registry,
                                                  const char *name, void *data, size_t size)
{
    return sapi_state_transfer_register_field_ex(registry, name, data, size, NULL, NULL);
}

sapi_status_t sapi_state_transfer_register_field_ex(sapi_state_transfer_registry_t *registry,
                                                     const char *name, void *data, size_t size,
                                                     sapi_state_field_encode_fn encode_fn,
                                                     sapi_state_field_decode_fn decode_fn)
{
    sapi_state_field_t *slot;

    if ((registry == NULL) || (data == NULL) || (size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* Both-or-neither: a field either uses this module's default raw
     * memcpy (both NULL) or a fully custom codec (both non-NULL) - never
     * a mix, which would leave encode/decode asymmetric. */
    if ((encode_fn == NULL) != (decode_fn == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (registry->field_count >= SAPI_STATE_TRANSFER_MAX_FIELDS)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    slot = &registry->fields[registry->field_count];
    slot->name = name;
    slot->data = data;
    slot->size = size;
    slot->encode_fn = encode_fn;
    slot->decode_fn = decode_fn;
    registry->field_count++;
    return SAPI_STATUS_OK;
}

size_t sapi_state_transfer_encoded_size(const sapi_state_transfer_registry_t *registry)
{
    size_t total = 0U;
    uint32_t i;

    if (registry == NULL)
    {
        return 0U;
    }
    for (i = 0U; i < registry->field_count; i++)
    {
        total += registry->fields[i].size;
    }
    return total;
}

sapi_status_t sapi_state_transfer_encode(const sapi_state_transfer_registry_t *registry,
                                          uint8_t *out, size_t out_size, size_t *out_len)
{
    size_t offset = 0U;
    uint32_t i;

    if ((registry == NULL) || (out == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (out_size < sapi_state_transfer_encoded_size(registry))
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    for (i = 0U; i < registry->field_count; i++)
    {
        const sapi_state_field_t *field = &registry->fields[i];
        if (field->encode_fn != NULL)
        {
            sapi_status_t status = field->encode_fn(field->data, &out[offset], field->size);
            if (status != SAPI_STATUS_OK)
            {
                return status;
            }
        }
        else
        {
            sapi_mem_copy(&out[offset], field->data, field->size);
        }
        offset += field->size;
    }
    if (out_len != NULL)
    {
        *out_len = offset;
    }
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_state_transfer_decode(const sapi_state_transfer_registry_t *registry,
                                          const uint8_t *in, size_t in_len)
{
    size_t offset = 0U;
    uint32_t i;

    if ((registry == NULL) || (in == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (in_len < sapi_state_transfer_encoded_size(registry))
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    for (i = 0U; i < registry->field_count; i++)
    {
        const sapi_state_field_t *field = &registry->fields[i];
        if (field->decode_fn != NULL)
        {
            sapi_status_t status = field->decode_fn(field->data, &in[offset], field->size);
            if (status != SAPI_STATUS_OK)
            {
                return status;
            }
        }
        else
        {
            sapi_mem_copy(field->data, &in[offset], field->size);
        }
        offset += field->size;
    }
    return SAPI_STATUS_OK;
}
