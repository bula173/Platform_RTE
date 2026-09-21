/**
 * @file rte_crc64_tool.c
 * @brief CLI wrapper around rte_checksum_crc64() - prints a file's CRC-64
 *        (RTE_CRC64_ERTMS polynomial, the same one this framework's own
 *        peer-link integrity check already uses - see
 *        app_main_common_init()'s own rte_checksum_crc64_init() call) as
 *        lowercase hex.
 *
 * Exists so build-time (compute, in a BOM) and install-time (verify, in a
 * self-extracting installer's postcheck) integrity checks use the EXACT
 * same algorithm via the same real code path - no separate shell/Python
 * CRC64 reimplementation to keep in sync with this one.
 *
 * Not part of the safety-rated library - a host-side build tool, like
 * scripts/coverage.sh or scripts/run-cppcheck.sh, not something a target
 * application links.
 *
 * Usage: rte_crc64_tool <file>
 */
#include <stdint.h>
#include <stdio.h>

#include "safeapi/redundancy/checksum/rte_checksum.h"
#include "safeapi/utils/status/rte_status.h"

/* Static, not malloc'd (this project's own no-dynamic-memory rule applies
 * workspace-wide, not just to the shipped library - see CLAUDE.md) - large
 * enough for any artifact this workspace currently produces (a few hundred
 * KB) with generous headroom; a file that doesn't fit is a real, loud
 * error below, not a silent truncation. */
#define RTE_CRC64_TOOL_MAX_FILE_SIZE (64U * 1024U * 1024U)
static uint8_t g_file_buffer[RTE_CRC64_TOOL_MAX_FILE_SIZE];

int main(int argc, char **argv)
{
    FILE *fp;
    size_t bytes_read;
    rte_status_t status;
    rte_crc64_t crc;

    if (argc != 2)
    {
        (void)fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (fp == NULL)
    {
        (void)fprintf(stderr, "%s: cannot open '%s'\n", argv[0], argv[1]);
        return 1;
    }

    bytes_read = fread(g_file_buffer, 1U, sizeof(g_file_buffer), fp);
    if (ferror(fp) != 0)
    {
        (void)fprintf(stderr, "%s: read error on '%s'\n", argv[0], argv[1]);
        (void)fclose(fp);
        return 1;
    }
    if (feof(fp) == 0)
    {
        /* More data remained after filling the buffer - the file exceeds
         * RTE_CRC64_TOOL_MAX_FILE_SIZE. Fail loudly rather than checksum
         * a truncated prefix and report it as if it were the whole file. */
        (void)fprintf(stderr, "%s: '%s' exceeds the %u MB buffer this tool supports\n",
                       argv[0], argv[1], (unsigned int)(RTE_CRC64_TOOL_MAX_FILE_SIZE / (1024U * 1024U)));
        (void)fclose(fp);
        return 1;
    }
    (void)fclose(fp);

    status = rte_checksum_crc64_init(RTE_CRC64_ERTMS);
    if (status != RTE_STATUS_OK)
    {
        (void)fprintf(stderr, "%s: rte_checksum_crc64_init() failed: %s\n", argv[0], rte_status_to_string(status));
        return 1;
    }

    crc = rte_checksum_crc64(g_file_buffer, bytes_read);
    (void)printf("%016llx\n", (unsigned long long)crc);
    return 0;
}
