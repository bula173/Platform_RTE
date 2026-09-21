/**
 * @file rte_redundancy_config.c
 * @brief See rte_redundancy_config.h.
 *
 * The parser below is deliberately NOT a general JSON parser - it is a small,
 * bounded scanner for exactly the flat, fixed-key schema documented in the
 * header. stdio (fopen/fread/fclose) is used only at load time, matching the
 * OCORA PI-API's own "stdio.h is init-only" allowance (see
 * docs/rca/RCA-OCORA-PI-API.md Section 3a) - this is deployment-configuration
 * loading, not a cyclic safety operation.
 */
#include "safeapi/redundancy/config/rte_redundancy_config.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static rte_redundancy_capability_fn s_capability_fn;
static void *s_capability_context;

rte_status_t rte_redundancy_config_register_capability(rte_redundancy_capability_fn fn, void *context)
{
    s_capability_fn = fn;
    s_capability_context = context;
    return RTE_STATUS_OK;
}

/** Skip ASCII whitespace starting at buf[pos], bounded by len. */
static size_t skip_ws(const char *buf, size_t len, size_t pos)
{
    while ((pos < len) &&
           ((buf[pos] == ' ') || (buf[pos] == '\t') || (buf[pos] == '\n') || (buf[pos] == '\r'))) {
        pos++;
    }
    return pos;
}

/**
 * @brief Find `"key"` followed by a colon, return the index just past the
 *        colon (and any whitespace), i.e. where the value begins.
 * @return RTE_STATUS_OK with *out_value_pos set, or RTE_STATUS_INVALID_STATE
 *         if the key is not present.
 */
static rte_status_t find_key_value_pos_bounded(const char *buf, size_t start_pos, size_t end_pos,
                                                const char *key, size_t *out_value_pos)
{
    char needle[40];
    size_t needle_len;
    size_t i;

    needle[0] = '"';
    (void)strncpy(&needle[1], key, sizeof(needle) - 2U);
    needle[sizeof(needle) - 1U] = '\0';
    needle_len = strlen(needle);
    if ((needle_len + 1U) >= sizeof(needle)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    needle[needle_len] = '"';
    needle_len++;
    needle[needle_len] = '\0';

    if ((start_pos + needle_len) > end_pos) {
        return RTE_STATUS_INVALID_STATE;
    }

    for (i = start_pos; (i + needle_len) <= end_pos; i++) {
        if (memcmp(&buf[i], needle, needle_len) == 0) {
            size_t pos = i + needle_len;
            pos = skip_ws(buf, end_pos, pos);
            if ((pos >= end_pos) || (buf[pos] != ':')) {
                continue;
            }
            pos++;
            pos = skip_ws(buf, end_pos, pos);
            *out_value_pos = pos;
            return RTE_STATUS_OK;
        }
    }
    return RTE_STATUS_INVALID_STATE;
}

static rte_status_t find_key_value_pos(const char *buf, size_t len, const char *key, size_t *out_value_pos)
{
    return find_key_value_pos_bounded(buf, 0U, len, key, out_value_pos);
}

/** Parse a quoted string value starting at buf[pos] (must be '"'). */
static rte_status_t parse_string_value(const char *buf, size_t len, size_t pos,
                                         char *out, size_t out_size, size_t *out_end_pos)
{
    size_t start;
    size_t n;

    if ((pos >= len) || (buf[pos] != '"')) {
        return RTE_STATUS_INVALID_STATE;
    }
    pos++;
    start = pos;
    while ((pos < len) && (buf[pos] != '"')) {
        pos++;
    }
    if (pos >= len) {
        return RTE_STATUS_INVALID_STATE;
    }
    n = pos - start;
    if (n >= out_size) {
        return RTE_STATUS_INVALID_STATE;
    }
    (void)memcpy(out, &buf[start], n);
    out[n] = '\0';
    *out_end_pos = pos + 1U;
    return RTE_STATUS_OK;
}

/** Parse an unsigned decimal integer value starting at buf[pos]. */
static rte_status_t parse_uint_value(const char *buf, size_t len, size_t pos, uint32_t *out)
{
    size_t start = pos;
    uint32_t value = 0U;

    while ((pos < len) && (buf[pos] >= '0') && (buf[pos] <= '9')) {
        value = (value * 10U) + (uint32_t)(buf[pos] - '0');
        pos++;
    }
    if (pos == start) {
        return RTE_STATUS_INVALID_STATE;
    }
    *out = value;
    return RTE_STATUS_OK;
}

/** Parse a `["a", "b", ...]` array of strings starting at buf[pos] (must be '['). */
static rte_status_t parse_string_array(const char *buf, size_t len, size_t pos,
                                         char out[][RTE_REDUNDANCY_CONFIG_MAX_ROLE_NAME_LEN],
                                         uint32_t max_items, uint32_t *out_count)
{
    uint32_t count = 0U;

    if ((pos >= len) || (buf[pos] != '[')) {
        return RTE_STATUS_INVALID_STATE;
    }
    pos++;
    pos = skip_ws(buf, len, pos);

    while ((pos < len) && (buf[pos] != ']')) {
        size_t end_pos;
        rte_status_t status;

        if (count >= max_items) {
            return RTE_STATUS_INVALID_STATE;
        }
        status = parse_string_value(buf, len, pos, out[count], RTE_REDUNDANCY_CONFIG_MAX_ROLE_NAME_LEN, &end_pos);
        if (status != RTE_STATUS_OK) {
            return status;
        }
        count++;
        pos = skip_ws(buf, len, end_pos);
        if ((pos < len) && (buf[pos] == ',')) {
            pos++;
            pos = skip_ws(buf, len, pos);
        }
    }
    if ((pos >= len) || (buf[pos] != ']')) {
        return RTE_STATUS_INVALID_STATE;
    }
    *out_count = count;
    return RTE_STATUS_OK;
}

rte_status_t rte_channel_config_transport_from_string(const char *name, rte_channel_transport_t *out_transport)
{
    if ((name == NULL) || (out_transport == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((strcmp(name, "flow") == 0) || (strcmp(name, "FLOW") == 0)) {
        *out_transport = RTE_CHANNEL_TRANSPORT_FLOW;
        return RTE_STATUS_OK;
    }
    if ((strcmp(name, "netlink") == 0) || (strcmp(name, "posix") == 0) || (strcmp(name, "POSIX") == 0)) {
        *out_transport = RTE_CHANNEL_TRANSPORT_POSIX_NETLINK;
        return RTE_STATUS_OK;
    }
    if ((strcmp(name, "dds") == 0) || (strcmp(name, "DDS") == 0)) {
        *out_transport = RTE_CHANNEL_TRANSPORT_DDS;
        return RTE_STATUS_OK;
    }
    return RTE_STATUS_INVALID_PARAM;
}

rte_status_t rte_channel_config_role_from_string(const char *name, rte_channel_role_t *out_role)
{
    if ((name == NULL) || (out_role == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((strcmp(name, "listen") == 0) || (strcmp(name, "LISTEN") == 0) ||
        (strcmp(name, "subscriber") == 0) || (strcmp(name, "SUBSCRIBER") == 0)) {
        *out_role = RTE_CHANNEL_ROLE_LISTEN;
        return RTE_STATUS_OK;
    }
    if ((strcmp(name, "connect") == 0) || (strcmp(name, "CONNECT") == 0) ||
        (strcmp(name, "publisher") == 0) || (strcmp(name, "PUBLISHER") == 0)) {
        *out_role = RTE_CHANNEL_ROLE_CONNECT;
        return RTE_STATUS_OK;
    }
    return RTE_STATUS_INVALID_PARAM;
}

static rte_status_t parse_channel_object(const char *buf, size_t obj_start, size_t obj_end,
                                          rte_channel_def_t *out_channel)
{
    size_t val_pos;
    size_t end_pos;
    char str_buf[64];
    rte_status_t status;

    (void)memset(out_channel, 0, sizeof(*out_channel));

    /* id (optional) */
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "id", &val_pos);
    if (status == RTE_STATUS_OK) {
        status = parse_uint_value(buf, obj_end, val_pos, &out_channel->id);
        if (status != RTE_STATUS_OK) {
            return RTE_STATUS_INVALID_STATE;
        }
    }

    /* name (required) */
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "name", &val_pos);
    if (status != RTE_STATUS_OK) {
        return RTE_STATUS_INVALID_STATE;
    }
    status = parse_string_value(buf, obj_end, val_pos, out_channel->name,
                                sizeof(out_channel->name), &end_pos);
    if (status != RTE_STATUS_OK) {
        return RTE_STATUS_INVALID_STATE;
    }

    /* transport (optional, defaults to FLOW) */
    out_channel->transport = RTE_CHANNEL_TRANSPORT_FLOW;
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "transport", &val_pos);
    if (status == RTE_STATUS_OK) {
        status = parse_string_value(buf, obj_end, val_pos, str_buf, sizeof(str_buf), &end_pos);
        if (status == RTE_STATUS_OK) {
            (void)rte_channel_config_transport_from_string(str_buf, &out_channel->transport);
        }
    }

    /* role (optional, defaults to CONNECT) */
    out_channel->role = RTE_CHANNEL_ROLE_CONNECT;
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "role", &val_pos);
    if (status == RTE_STATUS_OK) {
        status = parse_string_value(buf, obj_end, val_pos, str_buf, sizeof(str_buf), &end_pos);
        if (status == RTE_STATUS_OK) {
            (void)rte_channel_config_role_from_string(str_buf, &out_channel->role);
        }
    }

    /* host (optional) */
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "host", &val_pos);
    if (status == RTE_STATUS_OK) {
        (void)parse_string_value(buf, obj_end, val_pos, out_channel->host,
                                 sizeof(out_channel->host), &end_pos);
    }

    /* port (optional) */
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "port", &val_pos);
    if (status == RTE_STATUS_OK) {
        uint32_t port_val = 0U;
        if (parse_uint_value(buf, obj_end, val_pos, &port_val) == RTE_STATUS_OK) {
            out_channel->port = (uint16_t)port_val;
        }
    }

    /* message_size (optional) */
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "message_size", &val_pos);
    if (status == RTE_STATUS_OK) {
        (void)parse_uint_value(buf, obj_end, val_pos, &out_channel->message_size);
    }

    /* connect_timeout_ms (optional) */
    status = find_key_value_pos_bounded(buf, obj_start, obj_end, "connect_timeout_ms", &val_pos);
    if (status == RTE_STATUS_OK) {
        (void)parse_uint_value(buf, obj_end, val_pos, &out_channel->connect_timeout_ms);
    }

    return RTE_STATUS_OK;
}

static rte_status_t parse_channels_array(const char *buf, size_t len, size_t pos,
                                          rte_channel_def_t *out_channels,
                                          uint32_t max_channels, uint32_t *out_count)
{
    uint32_t count = 0U;

    if ((pos >= len) || (buf[pos] != '[')) {
        return RTE_STATUS_INVALID_STATE;
    }
    pos++;
    pos = skip_ws(buf, len, pos);

    while ((pos < len) && (buf[pos] != ']')) {
        size_t obj_start;
        size_t obj_end;
        rte_status_t status;

        if (count >= max_channels) {
            return RTE_STATUS_INVALID_STATE;
        }
        if (buf[pos] != '{') {
            return RTE_STATUS_INVALID_STATE;
        }
        obj_start = pos;
        pos++;
        while ((pos < len) && (buf[pos] != '}')) {
            pos++;
        }
        if (pos >= len) {
            return RTE_STATUS_INVALID_STATE;
        }
        obj_end = pos;
        pos++; /* past '}' */

        status = parse_channel_object(buf, obj_start, obj_end, &out_channels[count]);
        if (status != RTE_STATUS_OK) {
            return status;
        }
        count++;

        pos = skip_ws(buf, len, pos);
        if ((pos < len) && (buf[pos] == ',')) {
            pos++;
            pos = skip_ws(buf, len, pos);
        }
    }
    if ((pos >= len) || (buf[pos] != ']')) {
        return RTE_STATUS_INVALID_STATE;
    }
    *out_count = count;
    return RTE_STATUS_OK;
}

rte_status_t rte_redundancy_config_topology_from_string(const char *name, rte_redundancy_topology_t *out_topology)
{
    if ((name == NULL) || (out_topology == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (strcmp(name, "2oo2") == 0) {
        *out_topology = RTE_REDUNDANCY_TOPOLOGY_2OO2;
        return RTE_STATUS_OK;
    }
    if (strcmp(name, "2oo2_redundant") == 0) {
        *out_topology = RTE_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT;
        return RTE_STATUS_OK;
    }
    if (strcmp(name, "2oo3") == 0) {
        *out_topology = RTE_REDUNDANCY_TOPOLOGY_2OO3;
        return RTE_STATUS_OK;
    }
    if (strcmp(name, "nmr") == 0) {
        *out_topology = RTE_REDUNDANCY_TOPOLOGY_NMR;
        return RTE_STATUS_OK;
    }
    return RTE_STATUS_INVALID_PARAM;
}

const char *rte_redundancy_config_topology_to_string(rte_redundancy_topology_t topology)
{
    const char *result;

    switch (topology) {
        case RTE_REDUNDANCY_TOPOLOGY_2OO2:
            result = "2oo2";
            break;
        case RTE_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT:
            result = "2oo2_redundant";
            break;
        case RTE_REDUNDANCY_TOPOLOGY_2OO3:
            result = "2oo3";
            break;
        case RTE_REDUNDANCY_TOPOLOGY_NMR:
            result = "nmr";
            break;
        default:
            result = "unknown";
            break;
    }
    return result;
}

rte_status_t rte_redundancy_config_standby_mode_from_string(const char *name, rte_standby_mode_t *out_mode)
{
    if ((name == NULL) || (out_mode == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (strcmp(name, "hot") == 0) {
        *out_mode = RTE_STANDBY_MODE_HOT;
        return RTE_STATUS_OK;
    }
    if (strcmp(name, "warm") == 0) {
        *out_mode = RTE_STANDBY_MODE_WARM;
        return RTE_STATUS_OK;
    }
    if (strcmp(name, "cold") == 0) {
        *out_mode = RTE_STANDBY_MODE_COLD;
        return RTE_STATUS_OK;
    }
    return RTE_STATUS_INVALID_PARAM;
}

const char *rte_redundancy_config_standby_mode_to_string(rte_standby_mode_t mode)
{
    const char *result;

    switch (mode) {
        case RTE_STANDBY_MODE_HOT:
            result = "hot";
            break;
        case RTE_STANDBY_MODE_WARM:
            result = "warm";
            break;
        case RTE_STANDBY_MODE_COLD:
            result = "cold";
            break;
        default:
            result = "unknown";
            break;
    }
    return result;
}

rte_status_t rte_redundancy_config_load(const char *path, rte_redundancy_config_t *out_config)
{
    FILE *fp;
    static char buf[RTE_REDUNDANCY_CONFIG_MAX_FILE_SIZE];
    size_t len;
    size_t value_pos;
    rte_status_t status;
    char topology_name[24];
    rte_redundancy_config_t result;
    bool has_quorum;

    if ((path == NULL) || (out_config == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return RTE_STATUS_INTERNAL_ERROR;
    }
    len = fread(buf, 1U, sizeof(buf), fp);
    if ((ferror(fp) != 0) || (feof(fp) == 0)) {
        /* Either a read error, or the file was too large to fit (no EOF
         * reached within RTE_REDUNDANCY_CONFIG_MAX_FILE_SIZE bytes). */
        (void)fclose(fp);
        return RTE_STATUS_INTERNAL_ERROR;
    }
    (void)fclose(fp);

    (void)memset(&result, 0, sizeof(result));

    status = find_key_value_pos(buf, len, "topology", &value_pos);
    if (status != RTE_STATUS_OK) {
        return RTE_STATUS_INVALID_STATE;
    }
    {
        size_t end_pos;
        status = parse_string_value(buf, len, value_pos, topology_name, sizeof(topology_name), &end_pos);
        if (status != RTE_STATUS_OK) {
            return RTE_STATUS_INVALID_STATE;
        }
    }
    status = rte_redundancy_config_topology_from_string(topology_name, &result.topology);
    if (status != RTE_STATUS_OK) {
        return RTE_STATUS_INVALID_STATE;
    }

    status = find_key_value_pos(buf, len, "replicas", &value_pos);
    if (status != RTE_STATUS_OK) {
        return RTE_STATUS_INVALID_STATE;
    }
    status = parse_uint_value(buf, len, value_pos, &result.replica_count);
    if (status != RTE_STATUS_OK) {
        return RTE_STATUS_INVALID_STATE;
    }
    if ((result.replica_count < 1U) || (result.replica_count > RTE_REDUNDANCY_CONFIG_MAX_REPLICAS)) {
        return RTE_STATUS_INVALID_STATE;
    }

    has_quorum = false;
    status = find_key_value_pos(buf, len, "quorum", &value_pos);
    if (status == RTE_STATUS_OK) {
        status = parse_uint_value(buf, len, value_pos, &result.quorum_size);
        if (status != RTE_STATUS_OK) {
            return RTE_STATUS_INVALID_STATE;
        }
        has_quorum = true;
    }

    if (!has_quorum) {
        switch (result.topology) {
            case RTE_REDUNDANCY_TOPOLOGY_2OO2:
            case RTE_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT:
                if (result.replica_count < 2U) {
                    return RTE_STATUS_INVALID_STATE;
                }
                result.quorum_size = 2U;
                break;
            case RTE_REDUNDANCY_TOPOLOGY_2OO3:
                if (result.replica_count != 3U) {
                    return RTE_STATUS_INVALID_STATE;
                }
                result.quorum_size = 2U;
                break;
            case RTE_REDUNDANCY_TOPOLOGY_NMR:
            default:
                /* No implicit majority default for NMR - "quorum" is
                 * required in the file for this topology (see header doc). */
                return RTE_STATUS_INVALID_STATE;
        }
    }

    if ((result.quorum_size < 1U) || (result.quorum_size > result.replica_count)) {
        return RTE_STATUS_INVALID_STATE;
    }

    result.standby_mode = RTE_STANDBY_MODE_COLD;
    status = find_key_value_pos(buf, len, "standby_mode", &value_pos);
    if (status == RTE_STATUS_OK) {
        char standby_mode_name[8];
        size_t end_pos;
        status = parse_string_value(buf, len, value_pos, standby_mode_name, sizeof(standby_mode_name), &end_pos);
        if (status != RTE_STATUS_OK) {
            return RTE_STATUS_INVALID_STATE;
        }
        status = rte_redundancy_config_standby_mode_from_string(standby_mode_name, &result.standby_mode);
        if (status != RTE_STATUS_OK) {
            return RTE_STATUS_INVALID_STATE;
        }
    }

    status = find_key_value_pos(buf, len, "roles", &value_pos);
    if (status == RTE_STATUS_OK) {
        status = parse_string_array(buf, len, value_pos, result.roles,
                                     RTE_REDUNDANCY_CONFIG_MAX_REPLICAS, &result.role_count);
        if (status != RTE_STATUS_OK) {
            return RTE_STATUS_INVALID_STATE;
        }
    } else {
        result.role_count = 0U;
    }

    status = find_key_value_pos(buf, len, "channels", &value_pos);
    if (status == RTE_STATUS_OK) {
        status = parse_channels_array(buf, len, value_pos, result.channels,
                                      RTE_CHANNEL_CONFIG_MAX_CHANNELS, &result.channel_count);
        if (status != RTE_STATUS_OK) {
            return RTE_STATUS_INVALID_STATE;
        }
    } else {
        result.channel_count = 0U;
    }

    if ((s_capability_fn != NULL) && (!s_capability_fn(result.topology, result.replica_count, s_capability_context))) {
        return RTE_STATUS_NOT_SUPPORTED;
    }

    *out_config = result;
    rte_redundancy_config_set_active(&result);
    return RTE_STATUS_OK;
}

rte_status_t rte_redundancy_config_apply_to_voter(const rte_redundancy_config_t *config,
                                                      rte_voter_config_t *voter_cfg)
{
    if ((config == NULL) || (voter_cfg == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    voter_cfg->quorum_size = config->quorum_size;
    return RTE_STATUS_OK;
}

rte_status_t rte_redundancy_config_find_channel_by_name(const rte_redundancy_config_t *config,
                                                          const char *name,
                                                          rte_channel_def_t *out_channel)
{
    uint32_t i;

    if ((config == NULL) || (name == NULL) || (out_channel == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    for (i = 0U; i < config->channel_count; i++) {
        if (strncmp(config->channels[i].name, name, sizeof(config->channels[i].name)) == 0) {
            *out_channel = config->channels[i];
            return RTE_STATUS_OK;
        }
    }
    return RTE_STATUS_INVALID_PARAM;
}

rte_status_t rte_redundancy_config_find_channel_by_id(const rte_redundancy_config_t *config,
                                                        uint32_t id,
                                                        rte_channel_def_t *out_channel)
{
    uint32_t i;

    if ((config == NULL) || (out_channel == NULL)) {
        return RTE_STATUS_INVALID_PARAM;
    }
    for (i = 0U; i < config->channel_count; i++) {
        if (config->channels[i].id == id) {
            *out_channel = config->channels[i];
            return RTE_STATUS_OK;
        }
    }
    return RTE_STATUS_INVALID_PARAM;
}

static rte_redundancy_config_t s_active_config;
static bool s_has_active_config = false;

const rte_redundancy_config_t *rte_redundancy_config_get_active(void)
{
    return s_has_active_config ? &s_active_config : NULL;
}

void rte_redundancy_config_set_active(const rte_redundancy_config_t *config)
{
    if (config != NULL) {
        s_active_config = *config;
        s_has_active_config = true;
    } else {
        s_has_active_config = false;
    }
}
