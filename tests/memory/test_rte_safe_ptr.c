/* Tests for rte_safe_ptr (bounds + NULL + corruption-canary checked
 * pointer wrapper - see rte_safe_ptr.h). No backend needed: this module
 * has no OS dependency of its own, unlike rte_memory's pool allocator. */
#include <assert.h>
#include <string.h>

#include "safeapi/oal/memory/rte_safe_ptr.h"

static void test_init_rejects_bad_args(void)
{
    rte_safe_ptr_t sp;
    uint8_t buf[4];

    assert(rte_safe_ptr_init(NULL, buf, sizeof(buf)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_safe_ptr_init(&sp, NULL, sizeof(buf)) == RTE_STATUS_INVALID_PARAM);
    /* NULL ptr with size 0 is a deliberately-empty wrapper, not an error. */
    assert(rte_safe_ptr_init(&sp, NULL, 0U) == RTE_STATUS_OK);
}

static void test_get_validates_and_returns_pointer(void)
{
    rte_safe_ptr_t sp;
    uint8_t buf[4] = { 1, 2, 3, 4 };
    void *out = NULL;

    assert(rte_safe_ptr_init(&sp, buf, sizeof(buf)) == RTE_STATUS_OK);
    assert(rte_safe_ptr_is_valid(&sp));

    assert(rte_safe_ptr_get(NULL, &out) == RTE_STATUS_INVALID_PARAM);
    assert(rte_safe_ptr_get(&sp, NULL) == RTE_STATUS_INVALID_PARAM);

    assert(rte_safe_ptr_get(&sp, &out) == RTE_STATUS_OK);
    assert(out == (void *)buf);
}

static void test_corrupted_canary_is_rejected(void)
{
    rte_safe_ptr_t sp;
    uint8_t buf[4] = { 0 };
    void *out = NULL;

    assert(rte_safe_ptr_init(&sp, buf, sizeof(buf)) == RTE_STATUS_OK);
    assert(rte_safe_ptr_is_valid(&sp));

    /* Simulate corruption (e.g. a stray write, or a bit-flip) directly on
     * the struct - this is exactly the scenario the canary exists to
     * catch: a wrapper whose OWN state was clobbered, not its target
     * buffer. */
    sp.canary ^= 0xFFFFFFFFU;

    assert(!rte_safe_ptr_is_valid(&sp));
    assert(rte_safe_ptr_get(&sp, &out) == RTE_STATUS_DATA_CORRUPTION);
    assert(rte_safe_ptr_offset(&sp, 0U, 1U, &out) == RTE_STATUS_DATA_CORRUPTION);
}

static void test_offset_bounds_checking(void)
{
    rte_safe_ptr_t sp;
    uint8_t buf[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    void *out = NULL;

    assert(rte_safe_ptr_init(&sp, buf, sizeof(buf)) == RTE_STATUS_OK);

    /* In-bounds: offset 2, length 3 -> covers bytes [2,5), fits in 8. */
    assert(rte_safe_ptr_offset(&sp, 2U, 3U, &out) == RTE_STATUS_OK);
    assert(out == (void *)&buf[2]);

    /* Exactly at the boundary: offset 6, length 2 -> [6,8), fits exactly. */
    assert(rte_safe_ptr_offset(&sp, 6U, 2U, &out) == RTE_STATUS_OK);
    assert(out == (void *)&buf[6]);

    /* One byte past the end. */
    assert(rte_safe_ptr_offset(&sp, 6U, 3U, &out) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    /* Offset itself already past the end. */
    assert(rte_safe_ptr_offset(&sp, 9U, 1U, &out) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    /* offset + length overflowing size_t must not wrap around into a
     * false "in bounds" result. */
    assert(rte_safe_ptr_offset(&sp, SIZE_MAX, 1U, &out) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    assert(rte_safe_ptr_offset(NULL, 0U, 1U, &out) == RTE_STATUS_INVALID_PARAM);
    assert(rte_safe_ptr_offset(&sp, 0U, 1U, NULL) == RTE_STATUS_INVALID_PARAM);
}

static void test_invalidate(void)
{
    rte_safe_ptr_t sp;
    uint8_t buf[4] = { 0 };
    void *out = NULL;

    assert(rte_safe_ptr_init(&sp, buf, sizeof(buf)) == RTE_STATUS_OK);
    assert(rte_safe_ptr_is_valid(&sp));

    rte_safe_ptr_invalidate(&sp);
    assert(!rte_safe_ptr_is_valid(&sp));
    assert(rte_safe_ptr_get(&sp, &out) == RTE_STATUS_DATA_CORRUPTION);

    /* NULL is a documented no-op, not a crash. */
    rte_safe_ptr_invalidate(NULL);
}

int main(void)
{
    test_init_rejects_bad_args();
    test_get_validates_and_returns_pointer();
    test_corrupted_canary_is_rejected();
    test_offset_bounds_checking();
    test_invalidate();
    return 0;
}
