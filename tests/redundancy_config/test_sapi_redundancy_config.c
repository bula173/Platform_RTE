/** @file test_sapi_redundancy_config.c
 *  @brief Unit tests for sapi_redundancy_config.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "safeapi/redundancy/config/sapi_redundancy_config.h"

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    (void)fwrite(content, 1U, strlen(content), fp);
    (void)fclose(fp);
}

static void test_standby_mode_default_cold(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_standby_default.json", "{\"topology\": \"2oo2\", \"replicas\": 2}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_standby_default.json", &cfg) == SAPI_STATUS_OK);
    assert(cfg.standby_mode == SAPI_STANDBY_MODE_COLD);
}

static void test_standby_mode_hot(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_standby_hot.json",
               "{\"topology\": \"2oo2_redundant\", \"replicas\": 3, \"standby_mode\": \"hot\"}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_standby_hot.json", &cfg) == SAPI_STATUS_OK);
    assert(cfg.standby_mode == SAPI_STANDBY_MODE_HOT);
}

static void test_standby_mode_bad_value_rejected(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_standby_bad.json",
               "{\"topology\": \"2oo2\", \"replicas\": 2, \"standby_mode\": \"lukewarm\"}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_standby_bad.json", &cfg) == SAPI_STATUS_INVALID_STATE);
}

static void test_standby_mode_string_roundtrip(void)
{
    sapi_standby_mode_t mode;
    assert(sapi_redundancy_config_standby_mode_from_string("warm", &mode) == SAPI_STATUS_OK);
    assert(mode == SAPI_STANDBY_MODE_WARM);
    assert(strcmp(sapi_redundancy_config_standby_mode_to_string(mode), "warm") == 0);
    assert(sapi_redundancy_config_standby_mode_from_string("bogus", &mode) == SAPI_STATUS_INVALID_PARAM);
}

static void test_2oo2_default_quorum(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_2oo2.json", "{\"topology\": \"2oo2\", \"replicas\": 2}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_2oo2.json", &cfg) == SAPI_STATUS_OK);
    assert(cfg.topology == SAPI_REDUNDANCY_TOPOLOGY_2OO2);
    assert(cfg.replica_count == 2U);
    assert(cfg.quorum_size == 2U);
    assert(cfg.role_count == 0U);
}

static void test_2oo3_with_roles(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_2oo3.json",
               "{\"topology\": \"2oo3\", \"replicas\": 3, \"roles\": [\"A\", \"B\", \"C\"]}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_2oo3.json", &cfg) == SAPI_STATUS_OK);
    assert(cfg.topology == SAPI_REDUNDANCY_TOPOLOGY_2OO3);
    assert(cfg.replica_count == 3U);
    assert(cfg.quorum_size == 2U);
    assert(cfg.role_count == 3U);
    assert(strcmp(cfg.roles[0], "A") == 0);
    assert(strcmp(cfg.roles[2], "C") == 0);
}

static void test_nmr_explicit_quorum(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_nmr.json", "{\"topology\": \"nmr\", \"replicas\": 5, \"quorum\": 3}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_nmr.json", &cfg) == SAPI_STATUS_OK);
    assert(cfg.topology == SAPI_REDUNDANCY_TOPOLOGY_NMR);
    assert(cfg.replica_count == 5U);
    assert(cfg.quorum_size == 3U);
}

static void test_nmr_missing_quorum_rejected(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_nmr_bad.json", "{\"topology\": \"nmr\", \"replicas\": 5}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_nmr_bad.json", &cfg) == SAPI_STATUS_INVALID_STATE);
}

static void test_bad_topology_rejected(void)
{
    sapi_redundancy_config_t cfg;
    write_file("/tmp/sapi_redcfg_bad_topo.json", "{\"topology\": \"5oo9\", \"replicas\": 2}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_bad_topo.json", &cfg) == SAPI_STATUS_INVALID_STATE);
}

static void test_missing_file_rejected(void)
{
    sapi_redundancy_config_t cfg;
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_does_not_exist.json", &cfg) == SAPI_STATUS_INTERNAL_ERROR);
}

static void test_null_params_rejected(void)
{
    sapi_redundancy_config_t cfg;
    assert(sapi_redundancy_config_load(NULL, &cfg) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_2oo2.json", NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_apply_to_voter(void)
{
    sapi_redundancy_config_t cfg;
    sapi_voter_config_t voter_cfg;
    memset(&voter_cfg, 0, sizeof(voter_cfg));
    write_file("/tmp/sapi_redcfg_apply.json", "{\"topology\": \"2oo3\", \"replicas\": 3, \"quorum\": 2}");
    assert(sapi_redundancy_config_load("/tmp/sapi_redcfg_apply.json", &cfg) == SAPI_STATUS_OK);
    assert(sapi_redundancy_config_apply_to_voter(&cfg, &voter_cfg) == SAPI_STATUS_OK);
    assert(voter_cfg.quorum_size == 2U);
    assert(sapi_redundancy_config_apply_to_voter(NULL, &voter_cfg) == SAPI_STATUS_INVALID_PARAM);
}

static void test_topology_string_roundtrip(void)
{
    sapi_redundancy_topology_t t;
    assert(sapi_redundancy_config_topology_from_string("2oo2_redundant", &t) == SAPI_STATUS_OK);
    assert(t == SAPI_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT);
    assert(strcmp(sapi_redundancy_config_topology_to_string(t), "2oo2_redundant") == 0);
    assert(sapi_redundancy_config_topology_from_string("bogus", &t) == SAPI_STATUS_INVALID_PARAM);
}

int main(void)
{
    test_2oo2_default_quorum();
    test_2oo3_with_roles();
    test_nmr_explicit_quorum();
    test_nmr_missing_quorum_rejected();
    test_bad_topology_rejected();
    test_missing_file_rejected();
    test_null_params_rejected();
    test_apply_to_voter();
    test_topology_string_roundtrip();
    test_standby_mode_default_cold();
    test_standby_mode_hot();
    test_standby_mode_bad_value_rejected();
    test_standby_mode_string_roundtrip();
    printf("test_sapi_redundancy_config: all tests passed\n");
    return 0;
}
