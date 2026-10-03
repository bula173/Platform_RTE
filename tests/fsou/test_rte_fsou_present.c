/**
 * @file test_rte_fsou_present.c
 * @brief REQ-FSOU-003, present case: with an FSOU registered the first report logs "FSOU present", later ones
 *        nothing. Its own process, because the presence latch of rte_fsou cannot be reset (test_rte_fsou.c covers
 *        the absent case).
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rte/oal/fsou/rte_fsou.h"
#include "rte_osadapter/fsou/rte_osadapter_fsou.h"
#include "rte_osadapter/log/rte_osadapter_log.h"

static int s_log_lines;
static char s_last_log[64];

static rte_status_t stub_grant(uint32_t cycle_id)
{
    (void)cycle_id;
    return RTE_STATUS_OK;
}

static rte_status_t log_init(void)
{
    return RTE_STATUS_OK;
}

static void log_write(rte_log_level_t level, const char *tag, const char *message)
{
    assert(level == RTE_LOG_LEVEL_INFO);
    assert((tag != NULL) && (strcmp(tag, "FSOU") == 0));
    s_log_lines++;
    (void)snprintf(s_last_log, sizeof(s_last_log), "%s", (message != NULL) ? message : "");
}

static const rte_osadapter_fsou_t s_fsou = { stub_grant, NULL };
static const rte_osadapter_log_t s_log = { log_init, log_write };

int main(void)
{
    printf("Running test_rte_fsou_present...\n");
    assert(rte_osadapter_log_register(&s_log) == RTE_STATUS_OK);
    assert(rte_osadapter_fsou_register(&s_fsou) == RTE_STATUS_OK);
    assert(rte_fsou_is_present());

    rte_fsou_report_presence();
    assert(s_log_lines == 1);
    assert(strcmp(s_last_log, "FSOU present") == 0);

    rte_fsou_report_presence();
    rte_fsou_report_presence();
    assert(s_log_lines == 1);
    printf("All test_rte_fsou_present tests passed!\n");
    return 0;
}
