/** @file test_rte_redundancy_config.c
 *  @brief Unit tests for rte_redundancy_config.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "safeapi/redundancy/config/rte_redundancy_config.h"

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    (void)fwrite(content, 1U, strlen(content), fp);
    (void)fclose(fp);
}

static int g_capability_calls;
static rte_redundancy_topology_t g_capability_last_topology;
static uint32_t g_capability_last_replicas;

static bool only_2oo2_pair(rte_redundancy_topology_t topology, uint32_t replica_count, void *context)
{
    assert(context == (void *)0x1234);
    g_capability_calls++;
    g_capability_last_topology = topology;
    g_capability_last_replicas = replica_count;
    return (topology == RTE_REDUNDANCY_TOPOLOGY_2OO2) && (replica_count == 2U);
}

static void test_capability_callback_rejects_unsupported(void)
{
    rte_redundancy_config_t cfg;

    assert(rte_redundancy_config_register_capability(only_2oo2_pair, (void *)0x1234) == RTE_STATUS_OK);

    write_file("/tmp/rte_redcfg_cap_ok.json", "{\"topology\": \"2oo2\", \"replicas\": 2}");
    g_capability_calls = 0;
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_cap_ok.json", &cfg) == RTE_STATUS_OK);
    assert(g_capability_calls == 1);
    assert(g_capability_last_topology == RTE_REDUNDANCY_TOPOLOGY_2OO2);
    assert(g_capability_last_replicas == 2U);

    write_file("/tmp/rte_redcfg_cap_bad.json", "{\"topology\": \"2oo3\", \"replicas\": 3}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_cap_bad.json", &cfg) == RTE_STATUS_NOT_SUPPORTED);

    /* Clearing the callback (NULL) restores the pre-existing behavior:
     * any successfully-parsed config is accepted. */
    assert(rte_redundancy_config_register_capability(NULL, NULL) == RTE_STATUS_OK);
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_cap_bad.json", &cfg) == RTE_STATUS_OK);
}

static void test_standby_mode_default_cold(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_standby_default.json", "{\"topology\": \"2oo2\", \"replicas\": 2}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_standby_default.json", &cfg) == RTE_STATUS_OK);
    assert(cfg.standby_mode == RTE_STANDBY_MODE_COLD);
}

static void test_standby_mode_hot(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_standby_hot.json",
               "{\"topology\": \"2oo2_redundant\", \"replicas\": 3, \"standby_mode\": \"hot\"}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_standby_hot.json", &cfg) == RTE_STATUS_OK);
    assert(cfg.standby_mode == RTE_STANDBY_MODE_HOT);
}

static void test_standby_mode_bad_value_rejected(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_standby_bad.json",
               "{\"topology\": \"2oo2\", \"replicas\": 2, \"standby_mode\": \"lukewarm\"}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_standby_bad.json", &cfg) == RTE_STATUS_INVALID_STATE);
}

static void test_standby_mode_string_roundtrip(void)
{
    rte_standby_mode_t mode;
    assert(rte_redundancy_config_standby_mode_from_string("warm", &mode) == RTE_STATUS_OK);
    assert(mode == RTE_STANDBY_MODE_WARM);
    assert(strcmp(rte_redundancy_config_standby_mode_to_string(mode), "warm") == 0);
    assert(rte_redundancy_config_standby_mode_from_string("bogus", &mode) == RTE_STATUS_INVALID_PARAM);
}

static void test_2oo2_default_quorum(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_2oo2.json", "{\"topology\": \"2oo2\", \"replicas\": 2}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_2oo2.json", &cfg) == RTE_STATUS_OK);
    assert(cfg.topology == RTE_REDUNDANCY_TOPOLOGY_2OO2);
    assert(cfg.replica_count == 2U);
    assert(cfg.quorum_size == 2U);
    assert(cfg.role_count == 0U);
}

static void test_2oo3_with_roles(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_2oo3.json",
               "{\"topology\": \"2oo3\", \"replicas\": 3, \"roles\": [\"A\", \"B\", \"C\"]}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_2oo3.json", &cfg) == RTE_STATUS_OK);
    assert(cfg.topology == RTE_REDUNDANCY_TOPOLOGY_2OO3);
    assert(cfg.replica_count == 3U);
    assert(cfg.quorum_size == 2U);
    assert(cfg.role_count == 3U);
    assert(strcmp(cfg.roles[0], "A") == 0);
    assert(strcmp(cfg.roles[2], "C") == 0);
}

static void test_nmr_explicit_quorum(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_nmr.json", "{\"topology\": \"nmr\", \"replicas\": 5, \"quorum\": 3}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_nmr.json", &cfg) == RTE_STATUS_OK);
    assert(cfg.topology == RTE_REDUNDANCY_TOPOLOGY_NMR);
    assert(cfg.replica_count == 5U);
    assert(cfg.quorum_size == 3U);
}

static void test_nmr_missing_quorum_rejected(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_nmr_bad.json", "{\"topology\": \"nmr\", \"replicas\": 5}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_nmr_bad.json", &cfg) == RTE_STATUS_INVALID_STATE);
}

static void test_bad_topology_rejected(void)
{
    rte_redundancy_config_t cfg;
    write_file("/tmp/rte_redcfg_bad_topo.json", "{\"topology\": \"5oo9\", \"replicas\": 2}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_bad_topo.json", &cfg) == RTE_STATUS_INVALID_STATE);
}

static void test_missing_file_rejected(void)
{
    rte_redundancy_config_t cfg;
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_does_not_exist.json", &cfg) == RTE_STATUS_INTERNAL_ERROR);
}

static void test_null_params_rejected(void)
{
    rte_redundancy_config_t cfg;
    assert(rte_redundancy_config_load(NULL, &cfg) == RTE_STATUS_INVALID_PARAM);
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_2oo2.json", NULL) == RTE_STATUS_INVALID_PARAM);
}

static void test_apply_to_voter(void)
{
    rte_redundancy_config_t cfg;
    rte_voter_config_t voter_cfg;
    memset(&voter_cfg, 0, sizeof(voter_cfg));
    write_file("/tmp/rte_redcfg_apply.json", "{\"topology\": \"2oo3\", \"replicas\": 3, \"quorum\": 2}");
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_apply.json", &cfg) == RTE_STATUS_OK);
    assert(rte_redundancy_config_apply_to_voter(&cfg, &voter_cfg) == RTE_STATUS_OK);
    assert(voter_cfg.quorum_size == 2U);
    assert(rte_redundancy_config_apply_to_voter(NULL, &voter_cfg) == RTE_STATUS_INVALID_PARAM);
}

static void test_topology_string_roundtrip(void)
{
    rte_redundancy_topology_t t;
    assert(rte_redundancy_config_topology_from_string("2oo2_redundant", &t) == RTE_STATUS_OK);
    assert(t == RTE_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT);
    assert(strcmp(rte_redundancy_config_topology_to_string(t), "2oo2_redundant") == 0);
    assert(rte_redundancy_config_topology_from_string("bogus", &t) == RTE_STATUS_INVALID_PARAM);
}

static void test_channels_parsing(void)
{
    rte_redundancy_config_t cfg;
    rte_channel_def_t ch;
    const char *content = "{\n"
                          "  \"topology\": \"2oo2\",\n"
                          "  \"replicas\": 2,\n"
                          "  \"channels\": [\n"
                          "    {\n"
                          "      \"id\": 1,\n"
                          "      \"name\": \"ab-peer\",\n"
                          "      \"transport\": \"flow\",\n"
                          "      \"role\": \"listen\",\n"
                          "      \"host\": \"127.0.0.1\",\n"
                          "      \"port\": 15001,\n"
                          "      \"message_size\": 384,\n"
                          "      \"connect_timeout_ms\": 1000\n"
                          "    },\n"
                          "    {\n"
                          "      \"id\": 2,\n"
                          "      \"name\": \"ab-relay\",\n"
                          "      \"transport\": \"netlink\",\n"
                          "      \"role\": \"connect\",\n"
                          "      \"host\": \"10.0.0.1\",\n"
                          "      \"port\": 15002,\n"
                          "      \"message_size\": 1024,\n"
                          "      \"connect_timeout_ms\": 2000\n"
                          "    }\n"
                          "  ]\n"
                          "}";

    write_file("/tmp/rte_redcfg_channels.json", content);
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_channels.json", &cfg) == RTE_STATUS_OK);
    assert(cfg.channel_count == 2U);

    /* Lookup by name */
    memset(&ch, 0, sizeof(ch));
    assert(rte_redundancy_config_find_channel_by_name(&cfg, "ab-peer", &ch) == RTE_STATUS_OK);
    assert(ch.id == 1U);
    assert(strcmp(ch.name, "ab-peer") == 0);
    assert(ch.transport == RTE_CHANNEL_TRANSPORT_FLOW);
    assert(ch.role == RTE_CHANNEL_ROLE_LISTEN);
    assert(strcmp(ch.host, "127.0.0.1") == 0);
    assert(ch.port == 15001U);
    assert(ch.message_size == 384U);
    assert(ch.connect_timeout_ms == 1000U);

    /* Lookup by ID */
    memset(&ch, 0, sizeof(ch));
    assert(rte_redundancy_config_find_channel_by_id(&cfg, 2U, &ch) == RTE_STATUS_OK);
    assert(ch.id == 2U);
    assert(strcmp(ch.name, "ab-relay") == 0);
    assert(ch.transport == RTE_CHANNEL_TRANSPORT_POSIX_NETLINK);
    assert(ch.role == RTE_CHANNEL_ROLE_CONNECT);
    assert(strcmp(ch.host, "10.0.0.1") == 0);
    assert(ch.port == 15002U);
    assert(ch.message_size == 1024U);
    assert(ch.connect_timeout_ms == 2000U);

    /* Missing channel lookups */
    assert(rte_redundancy_config_find_channel_by_name(&cfg, "nonexistent", &ch) == RTE_STATUS_INVALID_PARAM);
    assert(rte_redundancy_config_find_channel_by_id(&cfg, 999U, &ch) == RTE_STATUS_INVALID_PARAM);

    /* Active configuration check */
    const rte_redundancy_config_t *active = rte_redundancy_config_get_active();
    assert(active != NULL);
    assert(active->channel_count == 2U);
}

static void test_channels_malformed_rejected(void)
{
    rte_redundancy_config_t cfg;
    const char *bad_content = "{\"topology\": \"2oo2\", \"replicas\": 2, \"channels\": [ {\"id\": 1} ]}";
    write_file("/tmp/rte_redcfg_bad_ch.json", bad_content);
    assert(rte_redundancy_config_load("/tmp/rte_redcfg_bad_ch.json", &cfg) == RTE_STATUS_INVALID_STATE);
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
    test_capability_callback_rejects_unsupported();
    test_channels_parsing();
    test_channels_malformed_rejected();
    printf("test_rte_redundancy_config: all tests passed\n");
    return 0;
}

