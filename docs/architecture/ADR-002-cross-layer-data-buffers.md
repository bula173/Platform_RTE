# ADR-002: Cross-Layer Data Buffer Abstraction

Status: Draft
Date: 2026-08-02
Applies to: safeAPIFreamwork, `include/safeapi/common/rte_buffer.h`

## 1. Context

ADR-001 defines the OS Abstraction Layer (L0) and anticipates a future L1
Safety Communication Layer that will move data between application layers
(L2 RBC core, L1, L0 services). Several existing L0 services already pass
raw `(pointer, size)` pairs around (`rte_nvm_read/write`, `rte_ipc_send/
receive`). As more layers and services are added, every one of them would
otherwise reinvent its own bounds-checking for "here is some data, here is
how much of it is valid."

This ADR defines a single, minimal, cross-layer data buffer abstraction so
every layer speaks the same shape when handing data to another layer,
without duplicating bounds-checking logic and without introducing dynamic
allocation.

This is a **generic data buffer**, not the L1 Safety Communication Layer
itself. It carries no sequence numbers, timeouts, or integrity metadata —
those remain scoped to the future L1 ADR (or to services that already own
their own integrity mechanism, e.g. `rte_nvm`'s CRC-checked reads).

## 2. Decision

### 2.1 Ownership model: caller-owned view over static storage

`rte_buffer_t` is a lightweight **view**, not a container that owns memory:

```c
typedef struct rte_buffer_s
{
    void   *data;      /* caller-owned storage, e.g. a static byte array */
    size_t  capacity;  /* total usable bytes in `data` */
    size_t  length;    /* bytes currently valid, 0 <= length <= capacity */
} rte_buffer_t;
```

Rationale: consistent with ADR-001 section 3.2 (no dynamic allocation after
init) and section 3.4 (caller-owned storage). A pool-based alternative
(`rte_mem_pool` acquire/release per buffer) was considered and rejected for
this generic type because it forces every call site to manage pool
lifecycle even for simple, short-lived, stack-local exchanges; pools remain
available and appropriate for services that need shared/queued ownership
(e.g. a future L1 message pool), layered on top of this same view type.

A separate read-only view, `rte_const_buffer_t`, is provided so a producer
can hand out data without granting the consumer write access:

```c
typedef struct rte_const_buffer_s
{
    const void *data;
    size_t      length;
} rte_const_buffer_t;
```

### 2.2 No integrity metadata on the generic buffer

`rte_buffer_t` intentionally carries no CRC/checksum field. Integrity
belongs to the service that has the context to define what "valid" means
(`rte_nvm` already CRC-checks on read; a future L1 layer will define
message-level integrity per EN 50159). Baking a generic checksum into every
buffer would either be redundant with those mechanisms or, worse, create a
false sense of safety at a layer that has no way to act on a mismatch.

### 2.3 API surface

All operations are bounds-checked and return `rte_status_t`; none allocate.

| Function | Purpose |
|---|---|
| `rte_buffer_init` | Bind a buffer view to caller-owned storage + capacity. |
| `rte_buffer_clear` | Reset length to 0 (capacity/data unchanged). |
| `rte_buffer_set_length` | Mark N bytes of already-written storage as valid. |
| `rte_buffer_copy_in` | Bounds-checked copy of external data into the buffer; sets length. |
| `rte_buffer_copy_out` | Bounds-checked copy of the buffer's valid bytes to an external destination. |
| `rte_buffer_as_const` | Produce a read-only view of the buffer's current valid bytes. |
| `rte_buffer_is_valid` | Defensive check: non-null data and `length <= capacity`. |

`rte_buffer_copy_in`/`copy_out` return `RTE_STATUS_RESOURCE_EXHAUSTED`
(not a new status code) when the source doesn't fit the destination
capacity — reusing the existing "insufficient capacity" semantics from
ADR-001's shared status table rather than growing the enum for this.

## 3. Consequences

- Positive: one bounds-checking implementation reused by every current and
  future service that exchanges data across a layer boundary; no dynamic
  allocation; small enough to pass by value or by pointer on the stack.
- Positive: read-only view type lets producers prevent accidental mutation
  by consumers without a `const`-correctness footgun on raw pointers.
- Negative / deferred: existing services (`rte_nvm`, `rte_ipc`) are **not**
  retrofitted to use `rte_buffer_t` in this change — they keep their raw
  `(pointer, size)` signatures for now to avoid an unscoped breaking change.
  Adopting `rte_buffer_t` in those APIs is a follow-up decision once this
  type has been used in practice.
- Deferred to the future L1 ADR: sequence numbers, timeout supervision,
  integrity/authentication (EN 50159), and any pooled/queued buffer
  ownership model for multi-consumer scenarios.

## 4. Location

> **Superseded by ADR-007.** See below for the original path; the current
> physical layout is `include/safeapi/buffer/rte_buffer.h` +
> `src/buffer/rte_buffer.c`, target `safeapi::buffer`.

`include/safeapi/common/rte_buffer.h` + `src/common/rte_buffer.c`, built
as a new `safeapi_common` static library target, independent of
`safeapi_os` (no OS dependency — pure data manipulation).
