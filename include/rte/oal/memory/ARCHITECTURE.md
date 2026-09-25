@page memory_architecture Memory Module - Architecture

@section memory_architecture_overview Static Allocation Only

Memory module enforces no dynamic allocation. All memory available at
compile-time or initialization. Framework never calls malloc/free.

@section memory_architecture_strategy Strategy

Allocate at compile-time (static arrays, stack) or initialization (once).
Pre-calculate max sizes. Use static_assert to verify budget.

@section memory_architecture_misra MISRA Rule 20.6

✓ No malloc/free
✓ No dynamic allocation
✓ All storage static or stack-scoped

@section memory_architecture_see_also See Also

- @ref memory_user_guide for allocation patterns
