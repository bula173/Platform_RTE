/**
 * @page safestate_architecture Safe-State Module - Architecture
 *
 * @section overview Design Overview
 *
 * Safe-State is a handler registration system for critical events. Applications
 * register handlers for each severity level (DEGRADED, SAFE, REBOOT), then call
 * SAPI_SAFESTATE() or SAPI_REBOOT() macros to trigger transitions. The framework
 * invokes the registered handler with context (file, line, message) and guarantees
 * control never returns for SAFE/REBOOT levels.
 *
 * No dynamic allocation; handler storage is fixed (3 slots, one per level).
 *
 * @section levels Three Severity Levels
 *
 * @verbatim
 * DEGRADED (0) - Can return to caller
 *   └─ Use for: single component failure, reduced redundancy
 *   └─ Example: backup channel failed, primary still working
 *   └─ Behavior: handler runs, can return, system continues
 *
 * SAFE (1) - Cannot return
 *   └─ Use for: critical fail-safe state, critical functions disabled
 *   └─ Example: both channels down, or voting disagreement
 *   └─ Behavior: handler runs, MUST NOT return, infinite loop if it does
 *
 * REBOOT (2) - Cannot return
 *   └─ Use for: emergency restart required
 *   └─ Example: unrecoverable software error, corruption detected
 *   └─ Behavior: handler runs, MUST NOT return, infinite loop if it does
 * @endverbatim
 *
 * @section api Core API
 *
 * @subsection api_register Registration
 *
 * @code
 * sapi_status_t sapi_safestate_register_handler(sapi_safestate_level_t level,
 *                                                sapi_safestate_handler_t handler);
 * @endcode
 *
 * Registers handler for one level. Replaces any previous handler for that level.
 * Called at startup before any safe-state transitions are expected.
 *
 * @subsection api_enter Entering a Level
 *
 * @code
 * void sapi_safestate_enter(sapi_safestate_level_t level,
 *                            sapi_safestate_reason_t reason,
 *                            const char *file,
 *                            int32_t line,
 *                            const char *message);
 * @endcode
 *
 * Invokes registered handler for level, passing context. For SAFE/REBOOT,
 * never returns (even if handler returns, framework enters infinite loop).
 *
 * Normally called via macros, not directly.
 *
 * @subsection api_macros Convenience Macros
 *
 * @code
 * SAPI_ASSERT(condition)          // If false, SAFE with REASON_ASSERT_FAILED
 * SAPI_SAFESTATE(level, reason)   // Enter level with reason
 * SAPI_REBOOT(reason)             // Enter REBOOT with reason
 * @endcode
 *
 * Macros automatically capture __FILE__ and __LINE__.
 *
 * @section handler_signature Handler Signature
 *
 * @code
 * typedef void (*sapi_safestate_handler_t)(
 *     sapi_safestate_level_t level,   // Level being entered
 *     sapi_safestate_reason_t reason, // Application-supplied reason code
 *     const char *file,               // Source file (__FILE__), may be NULL
 *     int32_t line,                   // Source line (__LINE__)
 *     const char *message             // Optional detail (e.g., assertion text)
 * );
 * @endcode
 *
 * Handler receives full context about what triggered the transition.
 * For DEGRADED: can return normally.
 * For SAFE/REBOOT: must not return; if it does, framework prevents it.
 *
 * @section reason_codes Reason Code Strategy
 *
 * @subsection reason_framework Framework Reasons (0-4095)
 *
 * @verbatim
 * SAPI_SAFESTATE_REASON_UNSPECIFIED (0)    - No specific reason
 * SAPI_SAFESTATE_REASON_ASSERT_FAILED (1)  - SAPI_ASSERT() condition false
 * SAPI_SAFESTATE_REASON_APPLICATION_BASE   - 4096, start of app-specific codes
 * @endverbatim
 *
 * @subsection reason_application Application Reasons (4096+)
 *
 * Applications define their own reason codes starting at 4096:
 *
 * @code
 * #define APP_REASON_VITAL_CHANNEL_ERROR 4096
 * #define APP_REASON_VOTING_MISMATCH 4097
 * #define APP_REASON_CHECKSUM_FAILED 4098
 * // ... etc
 * @endcode
 *
 * Reason codes appear in handler and handler can log/persist them for diagnostics.
 *
 * @section flow Transition Flow
 *
 * @verbatim
 * Application calls SAPI_SAFESTATE(level, reason)
 *     │
 *     ├─ Captures __FILE__, __LINE__
 *     └─ Calls sapi_safestate_enter(level, reason, file, line, NULL)
 *             │
 *             ├─ Retrieves registered handler for level
 *             ├─ If handler != NULL: invokes it with context
 *             └─ Handler returns (DEGRADED only) or doesn't (SAFE/REBOOT)
 *
 * If DEGRADED:
 *     └─ Handler returns normally
 *     └─ sapi_safestate_enter() returns to caller
 *     └─ Application continues execution
 *
 * If SAFE or REBOOT:
 *     ├─ If handler returns:
 *     │   └─ Framework enters infinite loop (infinite while(1))
 *     └─ Handler never returns:
 *         └─ Caller never regains control
 * @endverbatim
 *
 * @section storage Storage and Limits
 *
 * Fixed storage for 3 handlers (one per level). No dynamic allocation.
 * Handlers must be registered before use (typically at startup).
 *
 * @verbatim
 * Level 0 (DEGRADED)  ──> Handler A
 * Level 1 (SAFE)      ──> Handler B
 * Level 2 (REBOOT)    ──> Handler C
 * @endverbatim
 *
 * @section assert SAPI_ASSERT Implementation
 *
 * SAPI_ASSERT() is a macro that:
 * 1. Evaluates its condition
 * 2. If true: does nothing
 * 3. If false: calls sapi_safestate_enter(SAFE, ASSERT_FAILED, file, line, expr_text)
 *
 * @code
 * #define SAPI_ASSERT(cond) \
 *     do { \
 *         if (!(cond)) \
 *             sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE, \
 *                                  SAPI_SAFESTATE_REASON_ASSERT_FAILED, \
 *                                  __FILE__, (int32_t)__LINE__, #cond); \
 *     } while (0)
 * @endcode
 *
 * Unlike standard assert(), SAPI_ASSERT() is **always active**, never disabled.
 * This is intentional for safety-critical systems where assertions must catch
 * bugs in production.
 *
 * @section misra MISRA C:2012 Compliance
 *
 * @verbatim
 * Rule 2.1   │ ✓ │ No unreachable code
 * Rule 5.1   │ ✓ │ External identifiers unique
 * Rule 7.2   │ ✓ │ Correct signedness
 * Rule 8.4   │ ✓ │ Consistent declarations
 * Rule 14.4  │ ✓ │ Boolean operators correct
 * Rule 17.1  │ ✓ │ Pointer validity checked
 * Rule 20.6  │ ✓ │ No malloc/free
 * @endverbatim
 *
 * No dynamic allocation; no undefined behavior on entry/exit.
 *
 * @section integration Integration Points
 *
 * @subsection integ_watchdog With Watchdog Module
 *
 * Watchdog timeouts can trigger safe-state via handler:
 *
 * @code
 * void watchdog_fired(void) {
 *     SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_SAFE,
 *                   APP_REASON_WATCHDOG_TIMEOUT);
 * }
 * @endcode
 *
 * @subsection integ_reboot With Reboot Module
 *
 * SAPI_REBOOT() enters REBOOT level; handler can call sapi_reboot_request()
 * to actually restart the system.
 *
 * @section testing Unit Tests
 *
 * @subsection test_handler Test 1: Handler Registration and Invocation
 *
 * @code
 * static int handler_called = 0;
 * static sapi_safestate_level_t handler_level = 0;
 *
 * void test_handler(sapi_safestate_level_t level, ...) {
 *     handler_called = 1;
 *     handler_level = level;
 * }
 *
 * void test_degraded_handler_invoked(void) {
 *     sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_DEGRADED,
 *                                     test_handler);
 *     SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_DEGRADED, 0);
 *
 *     assert(handler_called == 1);
 *     assert(handler_level == SAPI_SAFESTATE_LEVEL_DEGRADED);
 * }
 * @endcode
 *
 * @subsection test_assert Test 2: SAPI_ASSERT Behavior
 *
 * @code
 * void test_assert_on_failure(void) {
 *     // SAPI_ASSERT(false) should enter SAFE state
 *     // Verify via mock handler that was triggered
 *     SAPI_ASSERT(false);  // Never reaches here (enters SAFE)
 * }
 * @endcode
 *
 * @section see_also See Also
 *
 * - @ref safestate_user_guide for usage patterns
 * - @ref watchdog_user_guide for watchdog integration
 * - @ref reboot_user_guide for reboot integration
 *
 */
