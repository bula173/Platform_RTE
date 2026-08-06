/**
 * @page task_architecture Task Queue Module - Architecture
 *
 * Priority queue for deferred work. Higher priority tasks execute first.
 * Fixed storage, no allocation. Application dequeues and executes.
 *
 * @section task_architecture_api Queue Operations
 *
 * sapi_task_queue_enqueue(queue, task) - Add to queue
 * sapi_task_queue_dequeue(queue, task) - Remove highest priority
 *
 * @section task_architecture_priority Priority Ordering
 *
 * Tasks sorted by priority field (higher = first). Same priority: FIFO.
 *
 * @section task_architecture_performance Complexity
 *
 * Enqueue: O(n) insertion sort
 * Dequeue: O(n) shift
 * n = queue size (typically < 64)
 *
 */

