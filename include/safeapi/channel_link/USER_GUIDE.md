/**
 * @page vital_channel_user_guide Vital Channel Module - User Guide
 *
 * @section vital_channel_user_guide_overview What is Vital Channel?
 *
 * Vital Channel provides redundant communication with automatic voting/arbitration.
 * Wraps multiple underlying IPC channels with:
 * - 2oo2 (dual-channel): Both must agree
 * - 2oo3 (triple-channel): Majority vote, tolerates 1 fault
 * - NMR (N-modular): Arbitrary N channels, majority vote
 *
 * Safety-critical systems use vital channels for hardware redundancy to detect
 * and recover from single-point failures (e.g., CPU faults in dual-core systems).
 *
 * @section vital_channel_user_guide_quick_start Quick Start: 2oo2 Dual-Redundant CPU
 *
 * @code
 * // 1. Create two underlying IPC channels (one per CPU)
 * sapi_buffer_t channel_a_buf, channel_b_buf;
 * sapi_buffer_init(&channel_a_buf, storage_a, capacity_a);
 * sapi_buffer_init(&channel_b_buf, storage_b, capacity_b);
 *
 * // 2. Create vital channel that wraps both with 2oo2 voting
 * sapi_buffer_t channels[2] = {channel_a_buf, channel_b_buf};
 * sapi_channel_config_t config = {
 *     .name = "dual_redundant_signal",
 *     .strategy = SAPI_VOTING_2OO2,
 *     .channels = channels,
 *     .num_channels = 2,
 *     .on_disagreement = handle_cpu_fault,
 *     .timeout_ms = 100
 * };
 * 
 * sapi_channel_t vital_ch;
 * sapi_vital_channel_create(&vital_ch, &config);
 *
 * // 3. Send command to both CPUs
 * signal_command_t cmd = {.signal_id = 5, .state = SIGNAL_RED};
 * sapi_status_t status = sapi_vital_send(vital_ch, &cmd, sizeof(cmd), 100);
 *
 * if (status != SAPI_STATUS_OK) {
 *     sapi_log_error("Send failed: %d", status);
 *     sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE);
 * }
 *
 * // 4. Receive reply from both CPUs (voting ensures agreement)
 * signal_reply_t reply;
 * status = sapi_vital_receive(vital_ch, &reply, sizeof(reply), 100);
 *
 * if (status == SAPI_STATUS_OK) {
 *     // Both CPUs agreed - safe to use reply
 *     printf("Signal update confirmed by both CPUs\n");
 * } else {
 *     // CPUs disagreed - likely a CPU fault
 *     sapi_log_error("CPU disagreement detected!");
 *     sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE);
 * }
 *
 * // 5. Check health
 * sapi_channel_health_t health;
 * sapi_vital_get_health(vital_ch, &health);
 * printf("Messages: %u sent, %u received, %u disagreements\n",
 *        health.messages_sent, health.messages_received, health.disagreements);
 * @endcode
 *
 * @section vital_channel_user_guide_strategies Voting Strategies
 *
 * @subsection vital_channel_user_guide_ss_2oo2 2oo2 (Dual-Channel)
 *
 * Both channels must agree; any disagreement triggers fault.
 *
 * @verbatim
 * Input A ────┐
 *            ├──→ Voting Logic ──→ Output
 * Input B ────┤
 *            └─ EQUAL? YES → Output OK
 *                          NO  → FAULT
 * @endverbatim
 *
 * - Use case: Dual-core processors
 * - Fault tolerance: 0 (any disagreement fails)
 * - Safety: Detects all single-point failures via disagreement
 *
 * @subsection vital_channel_user_guide_ss_2oo3 2oo3 (Triple-Channel)
 *
 * Majority vote (≥2 channels agree); tolerates 1 channel fault.
 *
 * @verbatim
 * Input A ────┐
 *            ├──→ Voting Logic ──→ Output
 * Input B ────┤
 *            │  (majority vote)
 * Input C ────┘
 *            └─ ≥2 EQUAL? YES → Output (from majority)
 *                             NO  → FAULT
 * @endverbatim
 *
 * - Use case: Triple-core processors, higher availability
 * - Fault tolerance: 1 channel
 * - Safety: Can detect and isolate single channel failure
 *
 * @subsection vital_channel_user_guide_ss_nmr NMR (N-Modular)
 *
 * N channels with majority vote; tolerates N/2 faults.
 *
 * - Use case: 4+ redundant processors
 * - Fault tolerance: N/2 channels
 * - Scalable redundancy: Use with any N (1-4 supported)
 *
 * @section vital_channel_user_guide_patterns Common Patterns
 *
 * @subsection vital_channel_user_guide_pattern_fault Fault Handling Callback
 *
 * @code
 * void handle_cpu_fault(void *context) {
 *     // Called when voting fails
 *     sapi_log_error("CPU fault detected!");
 *     // Trigger safe state
 *     sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE);
 * }
 * @endcode
 *
 * @subsection vital_channel_user_guide_pattern_health Health Monitoring
 *
 * @code
 * // Periodic health check (e.g., in watchdog callback)
 * void monitor_vital_channels(void) {
 *     sapi_channel_health_t health;
 *     sapi_vital_get_health(vital_ch, &health);
 *
 *     if (health.disagreements > 0) {
 *         sapi_log_warning("Disagreements detected: %u", health.disagreements);
 *     }
 *     if (health.errors > 0) {
 *         sapi_log_warning("Communication errors: %u", health.errors);
 *     }
 * }
 * @endcode
 *
 * @section vital_channel_user_guide_guidelines Best Practices
 *
 * 1. **Always register disagreement callback** for vital channels
 *    - Ensures safe-state transition on any fault
 * 2. **Validate timeout_ms carefully**
 *    - Must be longer than worst-case channel latency
 *    - Shorter timeouts detect faults faster but risk false positives
 * 3. **Monitor health regularly**
 *    - Check disagreement and error counts
 *    - Log trends to detect degradation
 * 4. **Use 2oo2 only when CPU faults are rare**
 *    - Any disagreement triggers safe state
 *    - Consider 2oo3 for higher availability
 * 5. **Test fault scenarios**
 *    - Simulate channel faults to verify safe-state transitions
 *    - Verify disagreement callback is invoked
 *
 * @section vital_channel_user_guide_see_also See Also
 *
 * - @ref vital_channel_architecture for design details
 * - @ref safestate_user_guide for safe-state transitions
 * - @ref ipc_guide for underlying IPC channels
 *
 */
