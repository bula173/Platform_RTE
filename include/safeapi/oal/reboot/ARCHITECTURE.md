/**
 * @page reboot_architecture Reboot Module - Architecture
 *
 * @section reboot_architecture_overview Design Overview
 *
 * Minimal layer providing single entry point: rte_reboot_request(reason).
 * Integrator registers platform-specific backend via rte_reboot_register_backend().
 * No dynamic allocation; single backend pointer storage.
 *
 * @section reboot_architecture_api Public API
 *
 * @code
 * rte_status_t rte_reboot_request(uint16_t reason_code);
 * @endcode
 *
 * Requests controlled system restart. Does not return on success (CPU resets).
 *
 * @code
 * rte_status_t rte_reboot_register_backend(const rte_reboot_backend_t *backend);
 * @endcode
 *
 * Registers platform-specific reboot implementation. Called at startup.
 *
 * @section reboot_architecture_backend Backend Interface
 *
 * @code
 * typedef struct {
 *     rte_status_t (*request)(uint16_t reason_code);
 * } rte_reboot_backend_t;
 * @endcode
 *
 * Backend implements platform-specific reset mechanism:
 * - Trigger hardware watchdog
 * - Execute CPU reset instruction
 * - Send command to supervisory processor
 * - etc.
 *
 * @section reboot_architecture_flow Reboot Flow
 *
 * @verbatim
 * rte_reboot_request(reason)
 *     │
 *     └─ Retrieve registered backend
 *        ├─ If no backend: return NOT_INITIALIZED
 *        └─ If has backend: call backend->request(reason)
 *               │
 *               ├─ On success: CPU resets (no return)
 *               └─ On failure: return error code
 * @endverbatim
 *
 * @section reboot_architecture_reason Reason Code Handling
 *
 * 16-bit reason code identifies why reboot requested. Backend can:
 * - Log to syslog
 * - Persist to NVM/flash (black box)
 * - Send to supervisory processor
 * - Use in post-reboot diagnostics
 *
 * Reason code is backend-interpreted; framework defines none.
 *
 * @section reboot_architecture_resource Resource Model
 *
 * Single static backend pointer. No allocation. Minimal state:
 *
 * @verbatim
 * backend_ptr: NULL or pointer to rte_reboot_backend_t
 * @endverbatim
 *
 * @section reboot_architecture_misra MISRA Compliance
 *
 * ✓ No dynamic allocation
 * ✓ Single responsibility (request only)
 * ✓ Function pointer via struct (vtable pattern)
 * ✓ No global function pointers
 *
 * @section reboot_architecture_see_also See Also
 *
 * - @ref reboot_user_guide for usage patterns
 * - @ref safestate_architecture for safe-state integration
 *
 */
