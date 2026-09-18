/**
 * @file sapi_redundancy_config.c
 * @brief See sapi_redundancy_config.h.
 *
 * The parser below is deliberately NOT a general JSON parser - it is a small,
 * bounded scanner for exactly the flat, fixed-key schema documented in the
 * header. stdio (fopen/fread/fclose) is used only at load time, matching the
 * OCORA PI-API's own "stdio.h is init-only" allowance (see
 * docs/rca/RCA-OCORA-PI-API.md Section 3a) - this is deployment-configuration
 * loading, not a cyclic safety operation.
 */
#include "safeapi/redundancy/config/sapi_redundancy_config.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

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
 * @return SAPI_STATUS_OK with *out_value_pos set, or SAPI_STATUS_INVALID_STATE
 *         if the key is not present.
 */
static sapi_status_t find_key_value_pos(const char *buf, size_t len, const char *key, size_t *out_value_pos)
{
    char needle[40];
    size_t needle_len;
    size_t i;

    needle[0] = '"';
    (void)strncpy(&needle[1], key, sizeof(needle) - 2U);
    needle[sizeof(needle) - 1U] = '\0';
    needle_len = strlen(needle);
    if ((needle_len + 1U) >= sizeof(needle)) {
        return SAPI_STATUS_INVALID_PARAM;
    }
    needle[needle_len] = '"';
    needle_len++;
    needle[needle_len] = '\0';

    if (needle_len >= len) {
        return SAPI_STATUS_INVALID_STATE;
    }

    for (i = 0U; (i + needle_len) <= len; i++) {
        if (memcmp(&buf[i], needle, needle_len) == 0) {
            size_t pos = i + needle_len;
            pos = skip_ws(buf, len, pos);
            if ((pos >= len) || (buf[pos] != ':')) {
                continue;
            }
            pos++;
            pos = skip_ws(buf, len, pos);
            *out_value_pos = pos;
            return SAPI_STATUS_OK;
        }
    }
    return SAPI_STATUS_INVALID_STATE;
}

/** Parse a quoted string value starting at buf[pos] (must be '"'). */
static sapi_status_t parse_string_value(const char *buf, size_t len, size_t pos,
                                         char *out, size_t out_size, size_t *out_end_pos)
{
    size_t start;
    size_t n;

    if ((pos >= len) || (buf[pos] != '"')) {
        return SAPI_STATUS_INVALID_STATE;
    }
    pos++;
    start = pos;
    while ((pos < len) && (buf[pos] != '"')) {
        pos++;
    }
    if (pos >= len) {
        return SAPI_STATUS_INVALID_STATE;
    }
    n = pos - start;
    if (n >= out_size) {
        return SAPI_STATUS_INVALID_STATE;
    }
    (void)memcpy(out, &buf[start], n);
    out[n] = '\0';
    *out_end_pos = pos + 1U;
    return SAPI_STATUS_OK;
}

/** Parse an unsigned decimal integer value starting at buf[pos]. */
static sapi_status_t parse_uint_value(const char *buf, size_t len, size_t pos, uint32_t *out)
{
    size_t start = pos;
    uint32_t value = 0U;

    while ((pos < len) && (buf[pos] >= '0') && (buf[pos] <= '9')) {
        value = (value * 10U) + (uint32_t)(buf[pos] - '0');
        pos++;
    }
    if (pos == start) {
        return SAPI_STATUS_INVALID_STATE;
    }
    *out = value;
    return SAPI_STATUS_OK;
}

/** Parse a `["a", "b", ...]` array of strings starting at buf[pos] (must be '['). */
static sapi_status_t parse_string_array(const char *buf, size_t len, size_t pos,
                                         char out[][SAPI_REDUNDANCY_CONFIG_MAX_ROLE_NAME_LEN],
                                         uint32_t max_items, uint32_t *out_count)
{
    uint32_t count = 0U;

    if ((pos >= len) || (buf[pos] != '[')) {
        return SAPI_STATUS_INVALID_STATE;
    }
    pos++;
    pos = skip_ws(buf, len, pos);

    while ((pos < len) && (buf[pos] != ']')) {
        size_t end_pos;
        sapi_status_t status;

        if (count >= max_items) {
            return SAPI_STATUS_INVALID_STATE;
        }
        status = parse_string_value(buf, len, pos, out[count], SAPI_REDUNDANCY_CONFIG_MAX_ROLE_NAME_LEN, &end_pos);
        if (status != SAPI_STATUS_OK) {
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
        return SAPI_STATUS_INVALID_STATE;
    }
    *out_count = count;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_redundancy_config_topology_from_string(const char *name, sapi_redundancy_topology_t *out_topology)
{
    if ((name == NULL) || (out_topology == NULL)) {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (strcmp(name, "2oo2") == 0) {
        *out_topology = SAPI_REDUNDANCY_TOPOLOGY_2OO2;
        return SAPI_STATUS_OK;
    }
    if (strcmp(name, "2oo2_redundant") == 0) {
        *out_topology = SAPI_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT;
        return SAPI_STATUS_OK;
    }
    if (strcmp(name, "2oo3") == 0) {
        *out_topology = SAPI_REDUNDANCY_TOPOLOGY_2OO3;
        return SAPI_STATUS_OK;
    }
    if (strcmp(name, "nmr") == 0) {
        *out_topology = SAPI_REDUNDANCY_TOPOLOGY_NMR;
        return SAPI_STATUS_OK;
    }
    return SAPI_STATUS_INVALID_PARAM;
}

const char *sapi_redundancy_config_topology_to_string(sapi_redundancy_topology_t topology)
{
    const char *result;

    switch (topology) {
        case SAPI_REDUNDANCY_TOPOLOGY_2OO2:
            result = "2oo2";
            break;
        case SAPI_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT:
            result = "2oo2_redundant";
            break;
        case SAPI_REDUNDANCY_TOPOLOGY_2OO3:
            result = "2oo3";
            break;
        case SAPI_REDUNDANCY_TOPOLOGY_NMR:
            result = "nmr";
            break;
        default:
            result = "unknown";
            break;
    }
    return result;
}

sapi_status_t sapi_redundancy_config_standby_mode_from_string(const char *name, sapi_standby_mode_t *out_mode)
{
    if ((name == NULL) || (out_mode == NULL)) {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (strcmp(name, "hot") == 0) {
        *out_mode = SAPI_STANDBY_MODE_HOT;
        return SAPI_STATUS_OK;
    }
    if (strcmp(name, "warm") == 0) {
        *out_mode = SAPI_STANDBY_MODE_WARM;
        return SAPI_STATUS_OK;
    }
    if (strcmp(name, "cold") == 0) {
        *out_mode = SAPI_STANDBY_MODE_COLD;
        return SAPI_STATUS_OK;
    }
    return SAPI_STATUS_INVALID_PARAM;
}

const char *sapi_redundancy_config_standby_mode_to_string(sapi_standby_mode_t mode)
{
    const char *result;

    switch (mode) {
        case SAPI_STANDBY_MODE_HOT:
            result = "hot";
            break;
        case SAPI_STANDBY_MODE_WARM:
            result = "warm";
            break;
        case SAPI_STANDBY_MODE_COLD:
            result = "cold";
            break;
        default:
            result = "unknown";
            break;
    }
    return result;
}

sapi_status_t sapi_redundancy_config_load(const char *path, sapi_redundancy_config_t *out_config)
{
    FILE *fp;
    static char buf[SAPI_REDUNDANCY_CONFIG_MAX_FILE_SIZE];
    size_t len;
    size_t value_pos;
    sapi_status_t status;
    char topology_name[24];
    sapi_redundancy_config_t result;
    bool has_quorum;

    if ((path == NULL) || (out_config == NULL)) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    len = fread(buf, 1U, sizeof(buf), fp);
    if ((ferror(fp) != 0) || (feof(fp) == 0)) {
        /* Either a read error, or the file was too large to fit (no EOF
         * reached within SAPI_REDUNDANCY_CONFIG_MAX_FILE_SIZE bytes). */
        (void)fclose(fp);
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    (void)fclose(fp);

    (void)memset(&result, 0, sizeof(result));

    status = find_key_value_pos(buf, len, "topology", &value_pos);
    if (status != SAPI_STATUS_OK) {
        return SAPI_STATUS_INVALID_STATE;
    }
    {
        size_t end_pos;
        status = parse_string_value(buf, len, value_pos, topology_name, sizeof(topology_name), &end_pos);
        if (status != SAPI_STATUS_OK) {
            return SAPI_STATUS_INVALID_STATE;
        }
    }
    status = sapi_redundancy_config_topology_from_string(topology_name, &result.topology);
    if (status != SAPI_STATUS_OK) {
        return SAPI_STATUS_INVALID_STATE;
    }

    status = find_key_value_pos(buf, len, "replicas", &value_pos);
    if (status != SAPI_STATUS_OK) {
        return SAPI_STATUS_INVALID_STATE;
    }
    status = parse_uint_value(buf, len, value_pos, &result.replica_count);
    if (status != SAPI_STATUS_OK) {
        return SAPI_STATUS_INVALID_STATE;
    }
    if ((result.replica_count < 1U) || (result.replica_count > SAPI_REDUNDANCY_CONFIG_MAX_REPLICAS)) {
        return SAPI_STATUS_INVALID_STATE;
    }

    has_quorum = false;
    status = find_key_value_pos(buf, len, "quorum", &value_pos);
    if (status == SAPI_STATUS_OK) {
        status = parse_uint_value(buf, len, value_pos, &result.quorum_size);
        if (status != SAPI_STATUS_OK) {
            return SAPI_STATUS_INVALID_STATE;
        }
        has_quorum = true;
    }

    if (!has_quorum) {
        switch (result.topology) {
            case SAPI_REDUNDANCY_TOPOLOGY_2OO2:
            case SAPI_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT:
                if (result.replica_count < 2U) {
                    return SAPI_STATUS_INVALID_STATE;
                }
                result.quorum_size = 2U;
                break;
            case SAPI_REDUNDANCY_TOPOLOGY_2OO3:
                if (result.replica_count != 3U) {
                    return SAPI_STATUS_INVALID_STATE;
                }
                result.quorum_size = 2U;
                break;
            case SAPI_REDUNDANCY_TOPOLOGY_NMR:
            default:
                /* No implicit majority default for NMR - "quorum" is
                 * required in the file for this topology (see header doc). */
                return SAPI_STATUS_INVALID_STATE;
        }
    }

    if ((result.quorum_size < 1U) || (result.quorum_size > result.replica_count)) {
        return SAPI_STATUS_INVALID_STATE;
    }

    result.standby_mode = SAPI_STANDBY_MODE_COLD;
    status = find_key_value_pos(buf, len, "standby_mode", &value_pos);
    if (status == SAPI_STATUS_OK) {
        char standby_mode_name[8];
        size_t end_pos;
        status = parse_string_value(buf, len, value_pos, standby_mode_name, sizeof(standby_mode_name), &end_pos);
        if (status != SAPI_STATUS_OK) {
            return SAPI_STATUS_INVALID_STATE;
        }
        status = sapi_redundancy_config_standby_mode_from_string(standby_mode_name, &result.standby_mode);
        if (status != SAPI_STATUS_OK) {
            return SAPI_STATUS_INVALID_STATE;
        }
    }

    status = find_key_value_pos(buf, len, "roles", &value_pos);
    if (status == SAPI_STATUS_OK) {
        status = parse_string_array(buf, len, value_pos, result.roles,
                                     SAPI_REDUNDANCY_CONFIG_MAX_REPLICAS, &result.role_count);
        if (status != SAPI_STATUS_OK) {
            return SAPI_STATUS_INVALID_STATE;
        }
    } else {
        result.role_count = 0U;
    }

    *out_config = result;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_redundancy_config_apply_to_voter(const sapi_redundancy_config_t *config,
                                                      sapi_voter_config_t *voter_cfg)
{
    if ((config == NULL) || (voter_cfg == NULL)) {
        return SAPI_STATUS_INVALID_PARAM;
    }
    voter_cfg->quorum_size = config->quorum_size;
    return SAPI_STATUS_OK;
}
