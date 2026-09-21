/**
 * @page task_user_guide Task Queue Module - User Guide
 *
 * @section task_user_guide_overview What is Task Queue?
 *
 * Task Queue provides priority-based deferred work scheduling. Enqueue
 * tasks with priority; dequeue highest priority first. Caller executes tasks.
 *
 * @section task_user_guide_quick_start Quick Start
 *
 * @code
 * // Create task
 * rte_task_t task = {
 *     .priority = 50,
 *     .function = my_task_handler,
 *     .context = &context
 * };
 *
 * // Enqueue
 * rte_task_queue_enqueue(&queue, &task);
 *
 * // Main loop: dequeue and execute
 * rte_task_t next;
 * if (rte_task_queue_dequeue(&queue, &next) == RTE_STATUS_OK) {
 *     next.function(next.context);
 * }
 * @endcode
 *
 * @section task_user_guide_priority Priority Levels
 *
 * 0-100 scale (higher = more urgent)
 * 90-100: Critical (safety, watchdog)
 * 70-89: Important (vital data)
 * 50-69: Normal (command processing)
 * 30-49: Background (diagnostics)
 * 10-29: Low (cleanup)
 *
 * @section task_user_guide_patterns Patterns
 *
 * 1. Separate critical from non-critical
 * 2. Keep task execution time bounded
 * 3. Don't chain dependencies
 * 4. Monitor queue depth
 *
 * @section task_user_guide_see_also See Also
 *
 * - @ref task_architecture for design
 *
 */

