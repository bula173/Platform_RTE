/**
 * @file rte_notify.h
 * @brief Fixed-capacity registered-callback list shape (ADR-030).
 *
 * ADR-030: extracted from a hand-rolled pattern in the downstream
 * `safeAPIRBC2oo2` project - one module (an "orchestrator") asking N
 * registered peers "is this proposed action OK" (a veto gate, all
 * registered validators must agree) and, separately, telling N
 * registered peers "this happened" (a fan-out notification, no return
 * value) - because no equivalent primitive existed here, exactly the
 * same trigger ADR-025 documents for `rte_cross_comparator`.
 *
 * This header declares the STORAGE SHAPE only
 * (RTE_DECLARE_CALLBACK_LIST below) - it does not generate
 * init/register/dispatch functions. That is a deliberate choice, not an
 * omission: `rte_voter`/`rte_cross_comparator`'s own registered
 * callbacks (`rte_voter_compare_fn`, `on_disagreement`, ...) are each a
 * single, concretely-typed function pointer field baked into that
 * module's own config struct - none of this framework's existing code
 * generates CONTROL FLOW (loops, bounds checks) from a macro, only
 * types (RTE_DECLARE_STORAGE, rte_types.h, is type-only too). A
 * macro that tried to also generate a variadic "call every registered
 * slot with these arguments" dispatcher would need either
 * `__VA_ARGS__`-based control flow inside a function-like macro (poor
 * MISRA posture - hidden control flow, awkward to review/trace) or
 * type-erased `void *` function pointers cast back to their real
 * signature at the call site (MISRA C:2012 Rule 11.1 - "Conversions
 * shall not be performed between a pointer to a function and any other
 * type" - forbidden outright). Neither is acceptable here.
 *
 * Usage: declare the storage shape once per callback signature -
 *
 *   typedef bool (*my_validator_fn)(void *context, uint16_t a, uint8_t b);
 *   RTE_DECLARE_CALLBACK_LIST(my_validator_list_t, my_validator_fn, 2U);
 *
 * - then write your own small, ordinary (non-macro) init()/register()/
 * dispatch() functions against `my_validator_list_t`, following this
 * shape (every setup-only constructor in this framework - `rte_timer_create()`,
 * `rte_voter_init()`/`_register_channel()`, `rte_cross_comparator_init()`/
 * `_register_channel()`, `rte_watchdog_create()` - calls
 * `rte_lifecycle_check_setup_allowed()`, rte/lifecycle/rte_lifecycle.h,
 * as one of its own first checks; a callback-list register() function
 * shall do the same, since registering a callback is exactly the same
 * kind of INIT-phase-only resource construction):
 *
 *   static rte_status_t my_validator_list_init(my_validator_list_t *list)
 *   {
 *       uint32_t i;
 *       if (list == NULL) { return RTE_STATUS_INVALID_PARAM; }
 *       list->count = 0U;
 *       for (i = 0U; i < 2U; i++) { list->slots[i].fn = NULL; list->slots[i].context = NULL; }
 *       return RTE_STATUS_OK;
 *   }
 *
 *   static rte_status_t my_validator_list_register(my_validator_list_t *list, my_validator_fn fn, void *context)
 *   {
 *       rte_status_t lifecycle_status;
 *       if ((list == NULL) || (fn == NULL)) { return RTE_STATUS_INVALID_PARAM; }
 *       lifecycle_status = rte_lifecycle_check_setup_allowed();
 *       if (lifecycle_status != RTE_STATUS_OK) { return lifecycle_status; }
 *       if (list->count >= 2U) { return RTE_STATUS_RESOURCE_EXHAUSTED; }
 *       list->slots[list->count].fn = fn;
 *       list->slots[list->count].context = context;
 *       list->count++;
 *       return RTE_STATUS_OK;
 *   }
 *
 *   // veto gate: false if ANY registered validator returns false, OR if
 *   // zero validators are registered (fail-safe default-deny - nothing
 *   // is authorized without an explicit, registered authority).
 *   static bool my_validator_list_all_agree(const my_validator_list_t *list, uint16_t a, uint8_t b)
 *   {
 *       uint32_t i;
 *       bool result = (list->count > 0U);
 *       for (i = 0U; i < list->count; i++)
 *       {
 *           if ((list->slots[i].fn != NULL) && (!list->slots[i].fn(list->slots[i].context, a, b)))
 *           {
 *               result = false;
 *           }
 *       }
 *       return result;
 *   }
 *
 *   // fan-out notify: calls every registered slot, ignores nothing (void
 *   // return - there is nothing to ignore), NULL-guarded per slot
 *   // (same "same function pointer type, same call site, same NULL-guard"
 *   // custom-callback precedent as rte_watchdog_create()).
 *   static void my_notify_list_notify_all(const my_notify_list_t *list, uint16_t a, uint8_t b)
 *   {
 *       uint32_t i;
 *       for (i = 0U; i < list->count; i++)
 *       {
 *           if (list->slots[i].fn != NULL)
 *           {
 *               list->slots[i].fn(list->slots[i].context, a, b);
 *           }
 *       }
 *   }
 *
 * Every function above is a single, small, ordinary C function - fully
 * reviewable, fully traceable, no macro-generated control flow, no
 * function-pointer type punning. What this header actually shares
 * across every caller is the STORAGE SHAPE and the DISPATCH PATTERN
 * (documented above); the small amount of per-signature boilerplate
 * this leaves at each call site is the honest cost of C having no
 * generics, not an omission.
 *
 * @defgroup NOTIFY Registered-Callback List Storage Shape
 * @brief Fixed-capacity {callback, context} list shape (ADR-030)
 * @{
 */
#ifndef RTE_COMMON_NOTIFY_H
#define RTE_COMMON_NOTIFY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def RTE_DECLARE_CALLBACK_LIST
 * @brief Declares a fixed-capacity list of {callback, context} slots for
 *        one concretely-typed callback function pointer - see this
 *        file's own header for the full usage pattern (init/register/
 *        dispatch are hand-written against the declared type, not
 *        generated by this macro).
 * @param list_type          Name of the generated list type.
 * @param callback_fn_type   An already-declared function pointer
 *                           typedef (e.g. `typedef bool (*my_fn)(void *, ...)`).
 * @param max_subscribers    Fixed slot count (compile-time constant).
 *
 * Usage: RTE_DECLARE_CALLBACK_LIST(my_validator_list_t, my_validator_fn, 2U);
 */
#define RTE_DECLARE_CALLBACK_LIST(list_type, callback_fn_type, max_subscribers) \
    typedef struct                                                                  \
    {                                                                               \
        callback_fn_type fn;                                                        \
        void            *context;                                                   \
    } list_type##_slot_t;                                                          \
    typedef struct                                                                  \
    {                                                                               \
        list_type##_slot_t slots[(max_subscribers)];                               \
        uint32_t             count;                                                 \
    } list_type

#ifdef __cplusplus
}
#endif

#endif /* RTE_COMMON_NOTIFY_H */

/** @} */ /* NOTIFY */
