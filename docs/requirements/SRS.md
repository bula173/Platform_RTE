\page safeapi_srs Software Requirements Specification (SRS)

# Safe API Framework — Software Requirements Specification (SRS)

Date: 2026-08-02
Status: Draft, consolidated from `REQ-*` traceability tags already present
in the header comments under `include/safeapi/`. This document is the
canonical source those tags cite; if a requirement's wording ever needs to
change, change it here first, then update the corresponding code comment
to match. Design rationale for each requirement lives in the referenced
ADR under `docs/architecture/`; this document states *what* is required,
the ADRs explain *why*.

Format: `REQ-<AREA>-<MODULE>-<NUMBER>`. `AREA` is `OAL` (OS Abstraction
Layer) or `COMMON` (layer-agnostic) — a conceptual grouping, not a
directory: since ADR-007 each module below lives in its own
`include/safeapi/<feature>/` + `src/<feature>/` pair rather than a shared
`common/`/`os/` folder.

## 1. Common facilities

### 1.1 Status codes — `rte_status.h` (ADR-001 §3.6)

| ID | Requirement |
|---|---|
| REQ-OAL-COMMON-001 | Every fallible API function in the framework shall return `rte_status_t` and shall not use exceptions or `errno`-style side channels. |
| REQ-OAL-COMMON-002 | `rte_status_to_string()` shall return a short, static, human-readable string for every defined status code, for diagnostics/logging use only — never on a safety-decision path. |

### 1.2 Common types — `rte_types.h` (ADR-001 §3.4)

| ID | Requirement |
|---|---|
| REQ-OAL-COMMON-010 | No dynamic memory allocation is used by the framework after initialization; every stateful object is created from caller-supplied storage. |

### 1.3 Cross-layer data buffer — `rte_buffer.h` (ADR-002)

| ID | Requirement |
|---|---|
| REQ-COMMON-BUF-001 | `rte_buffer_t` shall never allocate memory; the caller owns the backing storage for the lifetime of the buffer view. |
| REQ-COMMON-BUF-002 | Every buffer copy operation shall be bounds-checked against the declared capacity and shall never write past it. |
| REQ-COMMON-BUF-010 | `rte_buffer_init()` shall bind a buffer view to caller-owned storage with an initial length of 0. |
| REQ-COMMON-BUF-011 | `rte_buffer_clear()` shall reset length to 0 without altering capacity or data. |
| REQ-COMMON-BUF-012 | `rte_buffer_set_length()` shall reject a length greater than capacity with `RTE_STATUS_RESOURCE_EXHAUSTED`. |
| REQ-COMMON-BUF-013 | `rte_buffer_copy_in()` shall perform a bounds-checked copy of external data into the buffer and update length; a source exceeding capacity shall be rejected without partial modification. |
| REQ-COMMON-BUF-014 | `rte_buffer_copy_out()` shall perform a bounds-checked copy of the buffer's valid bytes to an external destination and report the number of bytes copied. |
| REQ-COMMON-BUF-015 | `rte_buffer_as_const()` shall produce a read-only view of the buffer's currently valid bytes. |
| REQ-COMMON-BUF-016 | `rte_buffer_is_valid()` shall perform a defensive check that data is non-NULL and length does not exceed capacity. |
| REQ-COMMON-BUF-020..025 | `rte_buffer_write_u16/u32/u64_le/be()` shall append the value's bytes in the stated byte order at the buffer's current length and advance it; byte order is always explicit at the call site (ADR-006 §2.5, no default). |
| REQ-COMMON-BUF-026..031 | `rte_buffer_read_u16/u32/u64_le/be()` shall decode a value in the stated byte order from the given offset without mutating the buffer. |

### 1.4 Checked integer casting — `rte_cast.h` (ADR-003)

| ID | Requirement |
|---|---|
| REQ-COMMON-CAST-001 | A checked cast function shall never write to `*out` when the source value does not fit the destination type; the caller's variable is left exactly as passed in. |
| REQ-COMMON-CAST-002 | `out == NULL` shall yield `RTE_STATUS_INVALID_PARAM` without dereferencing `out`. |
| REQ-COMMON-CAST-003 | A value that does not fit the destination type's range shall yield `RTE_STATUS_VALUE_OUT_OF_RANGE`. |

Applies uniformly to all 72 `rte_cast_<from>_to_<to>` functions covering
every ordered pair among `int8_t`/`int16_t`/`int32_t`/`int64_t`/
`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`/`size_t` — see ADR-003 §2.1 for
the full type matrix; not enumerated as 72 separate IDs since all 72
satisfy the same three requirements by construction.

| REQ-COMMON-CAST-004 | `rte_cast_checked_add_u32()`/`_sub_u32()`/`_mul_u32()` and the `size_t` equivalents shall detect overflow/underflow and yield `RTE_STATUS_VALUE_OUT_OF_RANGE` instead of wrapping silently; `*out` is left untouched on failure (REQ-COMMON-CAST-001). |
| REQ-COMMON-CAST-005 | `rte_cast_bounds_check(index, count)` shall return `RTE_STATUS_OK` if `index < count`, else `RTE_STATUS_VALUE_OUT_OF_RANGE` - a checked helper standardizing the array/index-bounds check pattern already hand-rolled ad hoc across this codebase's consumers. |

Added during the framework module reorg (module directories nested under
`common`/`oal`/`redundancy`/`app` tiers - see this session's own commit
history/`CLAUDE.md` for the reorg itself, unrelated to these two new
requirements): overflow-checked arithmetic and a bounds-check helper, the
two "extra MISRA-safety helpers" requested alongside the safe-pointer
wrapper (2.3 below) and the already-existing `RTE_ASSERT()` (1.5 below).

### 1.5 Safe-state transitions — `rte_safestate.h` (ADR-004)

| ID | Requirement |
|---|---|
| REQ-COMMON-SAFESTATE-001 | Handler storage shall be a fixed array of 3 slots (one per `rte_safestate_level_t` value); no dynamic allocation. |
| REQ-COMMON-SAFESTATE-002 | `RTE_SAFESTATE_LEVEL_SAFE` and `RTE_SAFESTATE_LEVEL_REBOOT` shall never return control to the caller — even if no handler is registered, the registered handler itself returns, or the requested level value is invalid — falling back to a defensive infinite loop. |
| REQ-COMMON-SAFESTATE-010 | `rte_safestate_register_handler()` shall reject a NULL handler or an unrecognized level with `RTE_STATUS_INVALID_PARAM`; re-registering for the same level shall replace the previous handler. |
| REQ-COMMON-SAFESTATE-011 | `rte_safestate_enter()` shall invoke the registered handler for the given level (if any) before applying REQ-COMMON-SAFESTATE-002's non-return guarantee. |

`RTE_ASSERT` is always active in every build configuration (no
`NDEBUG`-style compile-out) and, on a false condition, calls
`rte_safestate_enter(RTE_SAFESTATE_LEVEL_SAFE, RTE_SAFESTATE_REASON_ASSERT_FAILED, ...)`
— this behavior is a consequence of REQ-COMMON-SAFESTATE-002/011 applied
to the `SAFE` level and is not tracked as a separate requirement ID.

#### 1.5.1 Safety-primitive violation notification — `rte_safety_violation.h`

| ID | Requirement |
|---|---|
| REQ-COMMON-SAFETYVIOLATION-001 | Handler storage shall be a single fixed slot (no dynamic allocation) — a lower-frequency, broader-scope hook than `rte_safestate`'s own per-level slots, since one registered handler covers all three instrumented primitive families (safe pointer, checked cast arithmetic, bounds check). |
| REQ-COMMON-SAFETYVIOLATION-002 | `rte_safety_violation_report()` shall be a no-op when no handler is registered — reporting a violation shall never itself become a new failure mode. |

Deliberately a separate, opt-in mechanism from `rte_safestate` (1.5
above): unlike a checkpoint timeout or voter disagreement (always
genuine faults, escalated unconditionally through
`rte_safestate_enter()`), the three primitives this module instruments
are also invoked during ordinary, expected control flow (e.g.
`rte_cast_bounds_check()` used to ask "is this the last valid index" is
not itself a fault) — forcing every failure through
`rte_safestate_enter()` would trigger REQ-COMMON-SAFESTATE-002's
permanent halt on entirely normal code paths. Instrumented call sites:
`rte_safe_ptr_get()`/`_offset()` (REQ-OAL-SAFEPTR-001/002, kind
`RTE_SAFETY_VIOLATION_CORRUPTION`/`_OUT_OF_RANGE`),
`rte_cast_checked_add_u32()`/`_sub_u32()`/`_mul_u32()`/`_add_size()`/
`_sub_size()`/`_mul_size()` (REQ-COMMON-CAST-004, kind
`RTE_SAFETY_VIOLATION_OVERFLOW`), and `rte_cast_bounds_check()`
(REQ-COMMON-CAST-005, kind `RTE_SAFETY_VIOLATION_OUT_OF_RANGE`). The
reported `file`/`line` is the detection site inside this framework's own
implementation, not the ultimate caller's site — these three primitives
are ordinary functions, not macros, so they cannot capture the caller's
`__FILE__`/`__LINE__` the way `RTE_ASSERT`/`RTE_SAFESTATE` do.

### 1.6 Bounded string manipulation — `rte_string.h` (ADR-006)

| ID | Requirement |
|---|---|
| REQ-COMMON-STR-001 | No dynamic allocation; the caller owns the backing storage for the lifetime of the string. |
| REQ-COMMON-STR-002 | `rte_string_copy()` shall never call `strlen()` on its source; it scans for a NUL only up to the destination's capacity and fails rather than reading past it. |
| REQ-COMMON-STR-003 | Content is treated as raw bytes/ASCII; no multi-byte/UTF-8-aware operations are provided. |
| REQ-COMMON-STR-010 | `rte_string_init()` shall bind a string to caller-owned storage with an initial length of 0. |
| REQ-COMMON-STR-011 | `rte_string_clear()` shall reset a string to empty without altering capacity or storage. |
| REQ-COMMON-STR-012 | `rte_string_length()` shall report the current length (not counting a NUL terminator), returning 0 for a NULL input. |
| REQ-COMMON-STR-013 | `rte_string_c_str()` shall ensure a NUL terminator is present within capacity (without incrementing length) and shall fail with `RTE_STATUS_RESOURCE_EXHAUSTED` rather than write past capacity. |
| REQ-COMMON-STR-014 | `rte_string_copy()` shall replace dest's content per REQ-COMMON-STR-002, leaving dest unmodified on failure. |
| REQ-COMMON-STR-015 | `rte_string_copy_n()` shall copy an exact-length, not-necessarily-NUL-terminated source without scanning it. |
| REQ-COMMON-STR-016 | `rte_string_concat()` shall append to dest's content, scanning src for a NUL only up to dest's remaining capacity, leaving dest unmodified on failure. |
| REQ-COMMON-STR-017 | `rte_string_compare()` shall compare up to the shorter string's length, then by length, reporting the result via an output parameter. |
| REQ-COMMON-STR-018 | `rte_string_find_char()` shall report "not found" as a normal (non-error) outcome via an output parameter. |
| REQ-COMMON-STR-019 | `rte_string_find_substr()` shall report "not found" as a normal outcome; an empty needle is always found at index 0. |
| REQ-COMMON-STR-020 | `rte_string_split_next()` shall use only caller-owned cursor state (no hidden/global state, unlike `strtok`), producing a zero-copy view of each token. |
| REQ-COMMON-STR-021..024 | `rte_string_from_u32/i32/u64/i64()` shall format the value as base-10 ASCII, replacing dest's content. |
| REQ-COMMON-STR-025..028 | `rte_string_to_u32/i32/u64/i64()` shall parse base-10 ASCII, yielding `RTE_STATUS_INVALID_PARAM` for malformed input and `RTE_STATUS_VALUE_OUT_OF_RANGE` for a value that doesn't fit the destination type. |
| REQ-COMMON-STR-029..032 | `rte_string_append_u32/i32/u64/i64()` shall append the value as base-10 ASCII to dest's existing content (advancing its length), scanning nothing, leaving dest unmodified and yielding `RTE_STATUS_RESOURCE_EXHAUSTED` when the digits do not fit the remaining capacity. |
| REQ-COMMON-STR-033 | `rte_string_append_hex_u32()` shall append the value as lowercase hexadecimal (no `0x` prefix) with a caller-supplied minimum digit count clamped to 1..8, never truncating a value that needs more digits, under the same bounds/immutability contract as REQ-COMMON-STR-029..032. |

## 2. OS Abstraction Layer

Every OAL service below shares the backend-registration contract defined
once in ADR-005 rather than repeating it per service. **Note:**
`REQ-OAL-BACKEND-001/002/003` below are a documentation-only consolidation
for readability — unlike every other ID in this SRS, they do **not**
appear verbatim as a tag in any header comment (see the traceability
caveat in section 4); the behavior they describe is what each per-service
`REQ-OAL-<SERVICE>-01X` "register_backend" entry already requires:

> **REQ-OAL-BACKEND-001** (ADR-005 §2.1, applies to every OAL service):
> `rte_<service>_register_backend()` shall reject a NULL backend with
> `RTE_STATUS_INVALID_PARAM`; re-registering shall replace the previous
> backend.
>
> **REQ-OAL-BACKEND-002** (ADR-005 §2.2, applies to every OAL service
> except `rte_log`): every public function shall validate its own
> parameters before consulting the backend; with no backend registered it
> shall return `RTE_STATUS_NOT_INITIALIZED`; with a backend registered
> but the corresponding vtable slot NULL, it shall return
> `RTE_STATUS_NOT_SUPPORTED`.
>
> **REQ-OAL-BACKEND-003** (ADR-005 §2.5, `rte_log` only): an
> unregistered backend is not an error; `rte_log_write()` with no
> backend registered shall silently do nothing.

### 2.1 Timer — `rte_timer.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-TIMER-001 | No dynamic allocation; caller supplies storage for each timer. |
| REQ-OAL-TIMER-002 | Callback execution time is the caller's responsibility to bound; the timer service itself must not block. |
| REQ-OAL-TIMER-003 | The expiry callback executes in a bounded-time, non-blocking context, documented per backend. |
| REQ-OAL-TIMER-010 | `rte_timer_create()` shall bind a timer to caller-owned storage without starting it. |
| REQ-OAL-TIMER-011 | `rte_timer_start()` shall start or restart a created timer. |
| REQ-OAL-TIMER-012 | `rte_timer_stop()` shall stop a running timer and be safe to call on an already-stopped timer. |
| REQ-OAL-TIMER-013 | `rte_timer_destroy()` shall release any backend resources bound to the timer. |
| REQ-OAL-TIMER-014 | `rte_timer_now()` shall report the current monotonic time base used by all timers. |
| REQ-OAL-TIMER-015 | `rte_timer_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.2 Non-volatile memory — `rte_nvm.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-NVM-001 | Every read shall be integrity-checked (e.g. CRC or redundant-copy voting) before data is returned to the caller. |
| REQ-OAL-NVM-002 | No dynamic allocation; caller supplies storage/buffers. |
| REQ-OAL-NVM-010 | `rte_nvm_open()` shall open (creating if necessary) a named region. |
| REQ-OAL-NVM-011 | `rte_nvm_read()` shall read and integrity-check data, returning `RTE_STATUS_DATA_CORRUPTION` on integrity failure with undefined output buffer content. |
| REQ-OAL-NVM-012 | `rte_nvm_write()` shall write data and its integrity metadata. |
| REQ-OAL-NVM-013 | `rte_nvm_sync()` shall force any buffered writes to durable storage. |
| REQ-OAL-NVM-014 | `rte_nvm_close()` shall close a region handle. |
| REQ-OAL-NVM-015 | `rte_nvm_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.3 Static memory reservation — `rte_memory.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-MEM-001 | All pools are reserved during system initialization; reservation after init is backend-defined and may be refused. |
| REQ-OAL-MEM-010 | `rte_mem_pool_create()` shall reserve a fixed-size-block pool. |
| REQ-OAL-MEM-011 | `rte_mem_pool_acquire()` shall return `RTE_STATUS_RESOURCE_EXHAUSTED` when no blocks remain. |
| REQ-OAL-MEM-012 | `rte_mem_pool_release()` shall return a previously acquired block to its pool. |
| REQ-OAL-MEM-013 | `rte_mem_pool_stats()` shall report current free/used block counts. |
| REQ-OAL-MEM-014 | `rte_mem_pool_register_backend()` per REQ-OAL-BACKEND-001. |

#### 2.3.1 Safe pointer wrapper — `rte_safe_ptr.h`

Bounds + NULL + corruption-canary checked access to a raw memory region a
caller already owns (a static buffer, a `rte_mem_pool_acquire()`
block, ...) - `rte_safe_ptr_t` never allocates anything itself.

| ID | Requirement |
|---|---|
| REQ-OAL-SAFEPTR-001 | Every access function (`rte_safe_ptr_get()`/`_offset()`) shall verify the canary first; a corrupted wrapper yields `RTE_STATUS_DATA_CORRUPTION` before any other check runs. |
| REQ-OAL-SAFEPTR-002 | `rte_safe_ptr_offset()` shall verify `offset + length <= size` (using overflow-checked addition - REQ-COMMON-CAST-004) before computing the resulting pointer, never raw `ptr + offset` at the call site. |
| REQ-OAL-SAFEPTR-003 | `rte_safe_ptr_invalidate()` shall clear both the wrapped pointer and the canary, so a subsequent access on the same wrapper fails `RTE_STATUS_DATA_CORRUPTION` rather than silently succeeding against a logically-released region. |

### 2.4 Task/thread scheduling — `rte_task.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-TASK-001 | No dynamic allocation; caller supplies storage and a fixed-size stack/context region. |
| REQ-OAL-TASK-002 | Priorities are fixed at creation time; dynamic priority inheritance/inversion handling is a backend/RTOS concern, not assumed by this API. |
| REQ-OAL-TASK-010 | `rte_task_create()` shall create a task in the suspended state. |
| REQ-OAL-TASK-011 | `rte_task_start()` shall start a created task. |
| REQ-OAL-TASK-012 | `rte_task_suspend()` shall suspend a running task. |
| REQ-OAL-TASK-013 | `rte_task_destroy()` shall terminate and destroy a task. |
| REQ-OAL-TASK-014 | `rte_task_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.5 Inter-process/inter-task communication — `rte_ipc.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-IPC-001 | Queues are bounded and statically sized at creation; no unbounded growth. |
| REQ-OAL-IPC-002 | Send/receive shall accept an explicit timeout and shall never block indefinitely by default. |
| REQ-OAL-IPC-010 | `rte_ipc_create()` shall create a bounded message channel. |
| REQ-OAL-IPC-011 | `rte_ipc_send()` shall block at most `timeout_ms`, returning `RTE_STATUS_TIMEOUT` if the queue stays full for the whole timeout. |
| REQ-OAL-IPC-012 | `rte_ipc_receive()` shall block at most `timeout_ms`, returning `RTE_STATUS_TIMEOUT` if no message arrives. |
| REQ-OAL-IPC-013 | `rte_ipc_destroy()` shall destroy a message channel. |
| REQ-OAL-IPC-014 | `rte_ipc_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.6 Logging/diagnostics — `rte_log.h` (ADR-001 §4, non-safety-related)

| ID | Requirement |
|---|---|
| REQ-OAL-LOG-001 | Log calls are best-effort and non-blocking; a full backend buffer silently drops the newest entries rather than blocking or erroring the caller's control flow. This service shall never sit on a safety execution path. |
| REQ-OAL-LOG-010 | `rte_log_init()` shall be safe to call once at startup. |
| REQ-OAL-LOG-011 | `rte_log_write()` shall be non-blocking and never fail the caller's control flow. |
| REQ-OAL-LOG-012 | `rte_log_register_backend()` per REQ-OAL-BACKEND-001, with the REQ-OAL-BACKEND-003 exception for the unregistered case. |
| REQ-OAL-LOG-013 | `rte_log_level_to_string()` shall return a fixed, non-NULL string for every `rte_log_level_t` value, including an unrecognized one ("UNKNOWN"). |
| REQ-OAL-LOG-014 | `rte_log_write_event()` shall emit `Site=<site> Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src> Destination=<dst> Type=<type> Info=<info>[ <extra_fields>]` (space-separated `Key=Value` pairs, fixed order) as the `message` passed to the registered backend's `write()`, with `source` as that call's `tag`; `Timestamp` shall degrade to `0` (never block or skip the event) if no `rte_timer` backend is registered or `rte_timer_now()` fails; the whole call shall be a silent no-op under the same conditions as `rte_log_write()` (REQ-OAL-LOG-001) when no `rte_log` backend is registered. |
| REQ-OAL-LOG-015 | `rte_log_set_level(min_level)` shall set a process-global minimum severity, below which `rte_log_write()` and `rte_log_write_event()` drop the call before dispatching to the backend (and before any formatting work); `rte_log_get_level()` shall return the current value. The default is `RTE_LOG_LEVEL_DEBUG` (nothing filtered). It is callable at any time from any thread (not setup-only); a `min_level` outside `RTE_LOG_LEVEL_DEBUG..RTE_LOG_LEVEL_ERROR` leaves the threshold unchanged. A concurrent change can only cause one in-flight best-effort log line to be kept or dropped unexpectedly (REQ-OAL-LOG-001), never a torn read. |
| REQ-OAL-LOG-016 | `rte_log_level_from_string(name, out_level)` shall parse exactly the four canonical spellings `rte_log_level_to_string()` renders (`"DEBUG"`/`"INFO"`/`"WARNING"`/`"ERROR"`, case-sensitive) into `*out_level` and return `RTE_STATUS_OK`; any other `name`, or a NULL `name`/`out_level`, shall return `RTE_STATUS_INVALID_PARAM` and leave `*out_level` unchanged. |
| REQ-OAL-LOG-017 | A `rte_log_fields_t` builder shall let a caller accumulate space-separated `key=value` pairs — typed via `rte_log_fields_add_str/_u32/_i32/_u64/_i64/_hex_u32/_bool()` — for use as `rte_log_write_event()`'s `extra_fields` (or `info`) argument without hand-rolling `rte_string` chains and without a variadic (MISRA C:2012 Rule 17.1). It shall use only in-struct storage (no allocation), place exactly one separating space between pairs and none at the ends, truncate rather than reject over-long content (REQ-OAL-LOG-001), tolerate a NULL builder / NULL key at every entry point as a silent no-op, and expose the accumulated text as a never-NULL C string via `rte_log_fields_c_str()` (`""` when empty). `rte_log_write_event_fields(...)` shall behave as `rte_log_write_event(...)` with the builder's text as `extra_fields`, an empty or NULL builder being identical to a NULL `extra_fields`. `rte_log_write_event()` shall treat an empty-string `extra_fields` identically to NULL (no field, no trailing space). |

### 2.7 Controlled reboot — `rte_reboot.h` (ADR-004 §3)

| ID | Requirement |
|---|---|
| REQ-OAL-REBOOT-001 | `rte_reboot_request()` is not expected to return on success; a return only occurs if the backend cannot perform the reboot. |
| REQ-OAL-REBOOT-010 | `rte_reboot_request()` shall request a controlled system restart via the registered backend. |
| REQ-OAL-REBOOT-011 | `rte_reboot_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.8 Point-to-point network link — `rte_netlink.h` (ADR-001 §4, ADR-005, ADR-021, ADR-027)

Framework ships the interface and validate-then-dispatch layer only; a
concrete backend (e.g. POSIX UDP sockets) is integrator-supplied and
lives with the application that registers it — see `RBC_GP`'s
`src/posix_backend/rte_posix_backend_netlink.c`. This table was backfilled
alongside ADR-027 (TCP → UDP migration) — the requirement IDs were
already cited in `rte_netlink.h`'s own header comments beforehand, but
had no matching table entry here; the wording below reflects the
now-transport-agnostic contract, not the earlier TCP-specific one.

| ID | Requirement |
|---|---|
| REQ-OAL-NETLINK-001 | No dynamic allocation; caller supplies storage (`rte_netlink_storage_t`). |
| REQ-OAL-NETLINK-002 | `rte_netlink_open()` shall never block longer than `config->connect_timeout_ms`. |
| REQ-OAL-NETLINK-003 | Send/receive shall accept an explicit timeout and shall never block indefinitely by default. |
| REQ-OAL-NETLINK-010 | `rte_netlink_open()` shall establish a point-to-point link per `config->role` (LISTEN binds and waits for its one peer; CONNECT dials), returning `RTE_STATUS_TIMEOUT` if not established within `connect_timeout_ms`. |
| REQ-OAL-NETLINK-011 | `rte_netlink_send()` shall send one fixed-size message, blocking at most `timeout_ms`; `RTE_STATUS_HARDWARE_FAULT` is returned only when the backend can positively confirm the peer is gone — a guarantee no backend can make on every failure mode (e.g. a lost/silently-dropped peer on an unreliable transport), so callers must not treat its absence as proof of liveness. |
| REQ-OAL-NETLINK-012 | `rte_netlink_receive()` shall receive one fixed-size message, blocking at most `timeout_ms`; `RTE_STATUS_DATA_CORRUPTION` is returned if the backend can detect the received message violated this link's wire contract (e.g. wrong length) but not necessarily its content. |
| REQ-OAL-NETLINK-013 | `rte_netlink_close()` shall close a link; the handle is invalid to use afterward. |
| REQ-OAL-NETLINK-014 | This service provides no message ordering, deduplication, or delivery guarantee of its own — a backend may be built on an unreliable transport (e.g. UDP). Any such guarantee is the caller's responsibility (`rte_dual_msgchannel`/`rte_dual_channel`, ADR-020, is the reusable sequence+CRC+ACK layer for callers that need one). |

### 2.9 Real-time platform configuration — `rte_platform.h` (ADR-035)

Framework ships the interface and validate-then-dispatch layer only; a
concrete backend (e.g. POSIX `mlockall()` + `SCHED_FIFO`) is
integrator-supplied and lives with `Platform_OS_POSIX`. Exists so
application startup code can ask for real-time bring-up through the OAL
rather than calling a backend symbol directly (the layering ADR-001 §3.5
requires).

| ID | Requirement |
|---|---|
| REQ-OAL-PLATFORM-001 | `rte_platform_realtime_init()` is best-effort: a backend that cannot obtain some or all of the requested capabilities (unprivileged host, no RT scheduler, a bounded lockable-memory limit) shall still return `RTE_STATUS_OK`, having applied what it could, and shall not leave the process in a state that prevents later timer/task thread creation. |
| REQ-OAL-PLATFORM-010 | `rte_platform_realtime_init(rt_priority)` shall reject `rt_priority > 99` with `RTE_STATUS_INVALID_PARAM` before any backend dispatch, then request the registered backend apply memory-residency configuration and (for `rt_priority > 0`) a real-time scheduling policy/priority for the calling process/thread. |
| REQ-OAL-PLATFORM-011 | `rte_platform_register_backend()` per REQ-OAL-BACKEND-001. |

## 3. Project-wide requirements (CLAUDE.md, not yet tagged per-function)

These apply across every module above and are enforced by convention and
the MISRA compliance review (`docs/MISRA_COMPLIANCE_REPORT.md`) rather
than an individual `REQ-*` tag on every function:

- Target language: Embedded C (C99/C11); MISRA C:2012 Mandatory/Required.
- No dynamic memory allocation (`malloc`/`free`/`realloc` banned) — see
  MISRA report §2 for verification evidence.
- Fixed-width types (`<stdint.h>`) instead of native `int`/`long`.
- All integer type conversions go through `rte_cast_*` (§1.3) — no bare
  C-style casts.
- No recursion, no uninitialized variables, no standard `errno`/`assert()`
  in production paths (`RTE_ASSERT`, §1.4, is the replacement).

## 3a. Distributed channel synchronization (ADR-017)

Not yet backfilled into this document for every module added since the
sections above were written (`rte_checksum` currently has no
REQ-tagged entries here despite existing in `include/`/`src/` — a
pre-existing gap, not introduced by this section). This section covers
the two modules added by ADR-017.

### 3a.1 Checkpoint rendezvous — `rte_checkpoint.h` (ADR-017 §2.2)

| ID | Requirement |
|---|---|
| REQ-CHECKPOINT-001 | `rte_channel_checkpoint()` shall never block longer than `config->max_delay_ms`. |
| REQ-CHECKPOINT-002 | A checkpoint-arrival reply that fails CRC verification or carries a different `checkpoint_id` shall not count toward `expected_node_count`. |
| REQ-CHECKPOINT-003 | If fewer than `expected_node_count` valid replies arrive within `max_delay_ms`, `rte_channel_checkpoint()` shall call `rte_safestate_enter()` at `RTE_SAFESTATE_LEVEL_SAFE` with `RTE_SAFESTATE_REASON_CHECKPOINT_TIMEOUT` before returning `RTE_STATUS_TIMEOUT`. |

**ADR-034 note (does not change the three requirements above):** `rte_channel_checkpoint()`
itself is unchanged — still exact `checkpoint_id` equality plus CRC verification, still the
same bounded-retry-loop/watchdog-kick behavior. What changed is upstream, in
`rte_appmanager_run()`'s built-in checkpoint integration (3c below): `checkpoint_id` is no
longer a bare per-cycle counter, it is computed by folding `RTE_CHECKPOINT_MARK()` calls into
a running signature (REQ-APPMANAGER-014). A `RBC_GP` caller **not** going through
`rte_appmanager`'s built-in integration still owns `checkpoint_id` entirely itself and these
three requirements are the complete contract it needs.

### 3a.2 Clock synchronization (diagnostic only) — `rte_clocksync.h` (ADR-017 §2.3)

| ID | Requirement |
|---|---|
| REQ-CLOCKSYNC-001 | `rte_clocksync_get_offset_ms()` and `rte_clocksync_get_quality()` shall return `RTE_STATUS_NOT_INITIALIZED` if no backend has been registered. |
| REQ-CLOCKSYNC-002 | This module shall never be called from, or influence the outcome of, `rte_channel_checkpoint()` or any other vital comparison — checkpoint-ID rendezvous, not clock agreement, is the basis of comparison correctness (ADR-017 §2.3). |

## 3b. Watchdog — `rte_watchdog.h` (partial)

Not a full backfill of this module (see 3a's own note on the pre-existing
gap) - just the `RTE_WATCHDOG_ACTION_FAILOVER` action, added when a real
integrator (RBC_GP's dual-channel A/B link and SITE<->SITE
heartbeat) needed a "my redundant peer stopped responding" reaction and
found the action a documented dead stub.

| ID | Requirement |
|---|---|
| REQ-WATCHDOG-001 | `rte_watchdog_create()` shall return `RTE_STATUS_INVALID_PARAM` if `config->action` is `RTE_WATCHDOG_ACTION_FAILOVER` and `config->custom_action` is `NULL` (same requirement already in force for `RTE_WATCHDOG_ACTION_CUSTOM`). |
| REQ-WATCHDOG-002 | On timeout, a watchdog configured with `RTE_WATCHDOG_ACTION_FAILOVER` shall invoke `config->custom_action(config->context)` — identical dispatch to `RTE_WATCHDOG_ACTION_CUSTOM` — and shall not itself decide what the timeout means; that decision belongs to the integrator's `custom_action`. |

## 3c. Application lifecycle hooks and cycle checkpoint — `rte_appmanager.h` (ADR-019/ADR-034)

`rte_appmanager` was one of the modules 3a flagged as not yet backfilled;
this section starts that backfill with the `REQ-APPMANAGER-*` IDs
introduced or already present in the header as of ADR-019/ADR-034, not a
full retroactive pass over every pre-existing behavior of the module.

**ADR-034 (checkpoint-signature marks, stage reorder):** found live in
`RBC_GP` (A/WEST and B/WEST cycling reboots roughly every 40s,
never stabilizing): `checkpoint_id` used to be `iteration_count`, a
process-local counter that resets to 0 on every reboot — two
independently-rebooting channels' counters have no reason to ever
coincide again after either one reboots alone, so REQ-CHECKPOINT-002
correctly (but uselessly) kept rejecting every reply as "wrong
checkpoint_id" until, by chance, both channels next rebooted together.
Fixed by having application code call `RTE_CHECKPOINT_MARK()` (or the
labelled variant) at meaningful decision/preparation points during
`pre_execute()`/`execute()`/`post_execute()`; each call folds a CRC64
hash of its call site into a running per-cycle signature (see
REQ-APPMANAGER-014), which becomes `checkpoint_id` — naturally equal on
both sides whenever they actually took the same program path this cycle,
regardless of either side's reboot history. This also moved the
checkpoint stage from first (before `pre_execute()`) to last (after
`post_execute()`, REQ-APPMANAGER-012), since marks made during a cycle
can only be compared once that cycle's own stages have run — which in
turn made REQ-APPMANAGER-008's pacing workaround unnecessary (see that
entry's own note) and enabled a genuine safety improvement,
stage-then-commit output (REQ-APPMANAGER-013): an application can queue
output during the cycle and defer actually transmitting it until the
checkpoint confirms both channels agree, so a diverged cycle's output is
never sent at all, instead of only being noticed after the fact.

| ID | Requirement |
|---|---|
| REQ-APPMANAGER-001 | Applications shall use the Application Manager (`rte_appmanager_run()`) for controlled initialization, execution, and shutdown lifecycle. |
| REQ-APPMANAGER-002 | Applications shall implement all mandatory operations in `rte_appmanager_operations_t` (`init`, `execute`, `shutdown`, `get_name`, `get_version`); `pre_execute`, `post_execute`, and `on_checkpoint_result` are optional and may be left `NULL`. |
| REQ-APPMANAGER-006 | `rte_appmanager_run()` shall treat a `NULL` `pre_execute` or `post_execute` as "skip this stage", not an error, and shall not call it. |
| REQ-APPMANAGER-007 | `rte_appmanager_run()` shall handle the checkpoint stage's result identically to `pre_execute`/`execute`/`post_execute`: on non-`RTE_STATUS_OK`, log it, increment `error_count`, and check `error_threshold` — no separate reaction path for a checkpoint failure/timeout. (ADR-034: the checkpoint stage is now the LAST stage of a cycle — REQ-APPMANAGER-012 — so this is the last opportunity for a cycle to be counted as an error, not the first.) |
| REQ-APPMANAGER-008 | **Superseded by ADR-034, kept for history.** Originally: a genuine checkpoint-stage failure shall not be retried faster than `max_delay_ms`, via `rte_appmanager_pace_failed_checkpoint()` — needed because the checkpoint stage used to run *before* `pre_execute()`, the only place a consumer's own cycle pacing lived, so a fast-failing checkpoint could spin the loop at ~90000 iterations/second. ADR-034 moved the checkpoint stage to run LAST (REQ-APPMANAGER-012): `pre_execute()` now always runs before checkpoint even has a chance to fail, so it already paces every cycle regardless of that cycle's own checkpoint outcome — the starvation this requirement guarded against can no longer occur by construction. `rte_appmanager_pace_failed_checkpoint()` was removed accordingly. |
| REQ-APPMANAGER-009 | `rte_appmanager_run()` is the single entry point for an application's lifecycle (REQ-APPMANAGER-001) and shall refuse re-entry: a call arriving while a previous call is still mid-lifecycle (`RTE_APP_STATE_INITIALIZING`/`_RUNNING`/`_SHUTTING_DOWN`) shall return `EXIT_FAILURE` immediately, without altering any state belonging to the call already in progress. A new call made only after a previous one has fully returned (state `RTE_APP_STATE_SHUTDOWN`/`_ERROR`) is unaffected — this framework's own test suite relies on exactly that sequential-call pattern (ADR-026). |
| REQ-APPMANAGER-010 | The moment `ops->init()` returns `RTE_STATUS_OK`, `rte_appmanager_run()` shall lock the application's setup phase (`rte_lifecycle_lock()`) for the remainder of that run, and shall unlock it (`rte_lifecycle_unlock()`) both at the start of every call and the moment that call's own execution phase ends — see ADR-026 and REQ-LIFECYCLE-001. |
| REQ-APPMANAGER-011 | `rte_appmanager_run()` shall treat `config->checkpoint->voter == NULL` identically to `config->checkpoint == NULL`: skip the `rte_channel_checkpoint()` call entirely for that cycle (no pacing, no error counted) — never as a failed stage. (ADR-034: since the checkpoint stage now runs LAST — REQ-APPMANAGER-012 — `pre_execute()`/`execute()`/`post_execute()` already ran unconditionally before this check, so a paused checkpoint can no longer starve them by construction; this requirement's original "...and proceed to pre_execute()/execute()/post_execute() normally" clause is now vacuous, kept here only as historical context for why the check exists at all.) Found live (ADR-027 Phase 3, `RBC_GP`): before this fix, a caller-paused checkpoint (`voter` toggled to `NULL` while its own underlying link is known down — a normal, documented pattern, not rare) was fed to `rte_channel_checkpoint()`, got back `RTE_STATUS_INVALID_PARAM`, and had that treated as a failed stage — paced (REQ-APPMANAGER-008) and `continue`d, which (under the pre-ADR-034 stage order) skipped every later stage for as long as `voter` stayed `NULL`, silently starving every one of a consumer's own per-cycle safety checks too, including `rte_watchdog_timer_tick()`. |
| REQ-APPMANAGER-012 | (ADR-034) `rte_appmanager_run()` shall run the checkpoint stage, if configured, as the LAST stage of a cycle — after `pre_execute()`/`execute()`/`post_execute()` have all had the opportunity to run — using the signature accumulated by `RTE_CHECKPOINT_MARK()` calls made during those stages THIS cycle (REQ-APPMANAGER-014) as `checkpoint_id`, not the previous stage order's `iteration_count`. |
| REQ-APPMANAGER-013 | (ADR-034) If `ops->on_checkpoint_result` is non-`NULL`, `rte_appmanager_run()` shall call it exactly once per cycle, immediately after the checkpoint stage, with `committed = true` when `config->checkpoint` is `NULL`, `config->checkpoint->voter` is `NULL` (paused), or the checkpoint rendezvous succeeded, and `committed = false` only when `rte_channel_checkpoint()` itself returned non-`RTE_STATUS_OK` (REQ-CHECKPOINT-003 has already driven `rte_safestate_enter()` by that point). Its own non-`RTE_STATUS_OK` return is handled identically to every other stage (REQ-APPMANAGER-007). |
| REQ-APPMANAGER-014 | (ADR-034) `rte_appmanager_checkpoint_mark(file, line, label)` — normally invoked via `RTE_CHECKPOINT_MARK()`/`RTE_CHECKPOINT_MARK_LABEL()` — shall, when called during an active `rte_appmanager_run()` cycle, fold a CRC64 hash of `label` (if non-`NULL`) or `file:line` into that cycle's running signature via `signature = crc64(encode_le(signature) \|\| encode_le(mark_hash))`, and shall be a documented no-op (the signature untouched) when called outside an active cycle (before the loop starts, or after it ends, including from `init()`/`shutdown()`). The signature shall reset to the fixed seed `RTE_APPMANAGER_CHECKPOINT_SIGNATURE_SEED` at the start of every cycle, before `pre_execute()` runs. `rte_appmanager_checkpoint_fold_signature(uint64_t)` shall be a pure function (no reference to any in-progress cycle) computing the same 64-to-32-bit fold `rte_appmanager_run()` uses internally, so a caller can independently compute the expected `checkpoint_id` for a known signature value (e.g. a no-marks cycle always folds to `0`, since the seed's own two 32-bit halves are equal). |

## 3c-bis. Application setup-phase lock — `rte_lifecycle.h` (ADR-026)

| ID | Requirement |
|---|---|
| REQ-LIFECYCLE-001 | Every setup-only constructor this framework ships (`rte_timer_create()`, `rte_channel_init()`, `rte_voter_init()`/`_register_channel()`, `rte_cross_comparator_init()`/`_register_channel()`, `rte_watchdog_create()`) shall reject its call with `RTE_STATUS_INVALID_STATE` once `rte_lifecycle_lock()` has been called and `rte_lifecycle_unlock()` has not been called since — i.e. once the application's setup phase is locked (see REQ-APPMANAGER-010). `rte_netlink_open()`, `rte_dual_channel_init()`, and `rte_dual_negotiator_init()` are deliberately **not** gated by this lock: all three are legitimately re-invoked after the setup phase locks by an application's own reconnect-after-link-loss logic (e.g. `RBC_GP`'s `channel_ab_io.c`/`channel_ab_negotiate_reconnect()`), re-establishing a link the application already owns rather than adding a new one its own design never accounted for. |
| REQ-LIFECYCLE-002 | The setup-phase lock shall be a single, process-wide flag (no dynamic allocation, no OS dependency, no per-`rte_appmanager_config_t` instance) — this framework has no concept of more than one concurrently-running application per process, matching `rte_appmanager`'s own existing `g_app_state` single-instance assumption. |

## 3d. Single-link channel, N-way voting, and 2-way cross-comparison — ADR-025

ADR-025 split what this section previously described (a single
`rte_channel` type combining a redundant transport link with N-way
voting logic, `channel_count`/`voting_strategy` included in its own
init config) into three modules with a clean responsibility boundary:
`rte_channel` is now a single point-to-point link only, `rte_voter`
does N-way 2oo2/2oo3/NMR voting over channels registered into it, and
`rte_cross_comparator` does 2-way peer comparison. `channel_count`
and `voting_strategy` moved off `rte_channel_init()`'s config entirely
and onto `rte_voter_init()`'s — the floor this section used to
describe (previously relaxed by ADR-019 §5.1 to allow a single-channel
NMR voter with `quorum_size == 1`) now lives there instead, unchanged
in substance: `RTE_VOTING_2OO2` requires exactly 2 registered
channels, `RTE_VOTING_2OO3` exactly 3, `RTE_VOTING_NMR` at least 1
with `1 <= quorum_size <= channel_count`.

### 3d.1 Single-link channel — `rte_channel.h` (`channel_link/`, ADR-025 §2.1)

| ID | Requirement |
|---|---|
| REQ-CHANNEL-001 | No dynamic allocation; caller supplies storage for every `rte_channel_t`. |
| REQ-CHANNEL-002 | `rte_channel_init()` shall return `RTE_STATUS_INVALID_PARAM` if `config->send` or `config->recv` is `NULL` — a channel with no way to move data is a construction-time error, not a deferred one. |
| REQ-CHANNEL-003 | `rte_channel_send()`/`_receive()` shall dispatch to `config->send`/`config->recv` and update `health.send_count`/`health.receive_count` (or the matching `_error_count`, plus `health.last_error`) on every call, regardless of outcome. |
| REQ-CHANNEL-004 | `is_healthy` shall default to `true` at `rte_channel_init()` and shall never be cleared automatically by a send/receive failure — only an explicit `rte_channel_set_healthy(handle, false)` call by the channel's owner (e.g. `rte_voter`, `rte_cross_comparator`) may mark it unhealthy. A single transient I/O failure alone does not condemn a link; that judgment belongs to whichever component is tracking the pattern of failures across calls. |
| REQ-CHANNEL-005 | `rte_channel_config_t::name` (e.g. `"ChannelAtoB"`) is optional (may be `NULL`) and is not copied - same caller-owned-pointer convention as `rte_watchdog_config_t::name` - so it must outlive the channel. `rte_channel_get_name()` shall return it verbatim (`NULL` if `handle` is `NULL` or no name was configured). |

### 3d.2 N-way voter — `rte_voter.h` (ADR-025 §2.2)

| ID | Requirement |
|---|---|
| REQ-VOTER-001 | No dynamic allocation; every `rte_voter_t` holds a fixed array of at most `RTE_VOTER_MAX_CHANNELS` (8) registered `rte_channel_t *` pointers. |
| REQ-VOTER-002 | `rte_voter_init()` shall return `RTE_STATUS_INVALID_PARAM` for a `voting_strategy` other than `RTE_VOTING_2OO2`/`_2OO3`/`_NMR`, or for `RTE_VOTING_NMR` with `quorum_size == 0`. |
| REQ-VOTER-003 | `rte_voter_send()`/`_receive()` shall return `RTE_STATUS_INVALID_PARAM` unless the number of currently registered channels matches the configured strategy's required count (`RTE_VOTING_2OO2` = exactly 2, `RTE_VOTING_2OO3` = exactly 3, `RTE_VOTING_NMR` = at least `quorum_size`). |
| REQ-VOTER-004 | `rte_voter_receive()` shall group every successfully-received, per-channel payload into equality classes (via `config->compare` if registered, otherwise `memcmp`) and select the *largest* class, reporting `RTE_VOTING_AGREED` with that class's data if its size meets the strategy's required quorum (`voter_required_quorum()`), or `RTE_VOTING_DISAGREED` otherwise. This is a majority vote across all registered channels, not a pairwise comparison against a single reference channel — see ADR-025 §1 for the bug this replaced. |
| REQ-VOTER-005 | On `RTE_VOTING_DISAGREED`, `rte_voter_receive()` shall call `rte_safestate_enter()` at `RTE_SAFESTATE_LEVEL_SAFE` when `config->trigger_safestate_on_disagreement` is `true` (the default), and shall always invoke `config->on_disagreement` (if registered) regardless of that flag. |
| REQ-VOTER-006 | `rte_voter_get_channel_by_name(voter, name)` shall return the first registered channel whose own `rte_channel_get_name()` exactly (`strcmp()`) matches `name`, or `NULL` if `voter`/`name` is `NULL` or no registered channel's name matches (including a channel whose own name is itself `NULL` - never matched by any lookup, REQ-CHANNEL-005). |

### 3d.3 2-way cross-comparator — `rte_cross_comparator.h` (ADR-025 §2.3)

| ID | Requirement |
|---|---|
| REQ-CROSSCOMPARATOR-001 | No dynamic allocation; every `rte_cross_comparator_t` holds exactly 2 registered `rte_channel_t *` slots. |
| REQ-CROSSCOMPARATOR-002 | A 3rd `rte_cross_comparator_register_channel()` call on an already-fully-registered comparator shall return `RTE_STATUS_RESOURCE_EXHAUSTED` without disturbing the 2 already-registered channels. |
| REQ-CROSSCOMPARATOR-003 | `rte_cross_comparator_execute()` shall require both registered channels to be healthy and to successfully receive `data_size` bytes before comparing; any unhealthy channel or receive failure shall short-circuit to `RTE_VOTING_TIMEOUT`/`RTE_VOTING_INSUFFICIENT_QUORUM` as appropriate without invoking the compare step. |
| REQ-CROSSCOMPARATOR-004 | The comparison itself shall use `config->compare` if registered, otherwise a full `memcmp()` of the two channels' received payloads — CRC-64 transport-integrity verification (`rte_checksum`) is a separate, already-applied concern and is never itself treated as "the comparison" (ADR-025 §2.4). |

## 3e. Dual-transfer state negotiation — `rte_dual` (ADR-020)

Three files under `include/safeapi/dual/`: `rte_dual_msgchannel.h` (Layer
1 "Channel", one EN 50159-defended message channel over one
`rte_netlink_handle_t`), `rte_dual_channel.h` ("DualChannel", 1..N
redundant Layer-1 links with always-send + bounded-ACK-wait delivery and
connection-status tracking), and `rte_dual_negotiator.h` (own/peer
`rte_dual_state_t` negotiation driven over an attached DualChannel).
`rte_dual_types.h` and `rte_dual_frames.h` hold shared enums/wire
structs with no behavior of their own beyond what the three modules above
require.

### 3e.1 Shared types — `rte_dual_types.h` (ADR-020 Decision §1)

| ID | Requirement |
|---|---|
| REQ-DUAL-TYPES-001 | `rte_dual_state_to_string()` and `rte_dual_channel_status_to_string()` shall return a non-NULL, static string for every defined enum value and a defensive `"UNKNOWN_STATE"`/`"UNKNOWN_STATUS"` respectively for an unrecognized one (same posture as `rte_log_level_to_string()`, REQ-OAL-LOG-013). |

### 3e.2 Base Channel — `rte_dual_msgchannel.h` (ADR-020 §1, Layer 1)

| ID | Requirement |
|---|---|
| REQ-DUAL-MSGCHANNEL-001 | No dynamic allocation; caller supplies storage for every `rte_dual_msgchannel_t`. |
| REQ-DUAL-MSGCHANNEL-002 | `rte_dual_msgchannel_send()`/`_receive()` shall never block longer than the caller-supplied timeout waiting on `rte_netlink_send()`/`_receive()`. |
| REQ-DUAL-MSGCHANNEL-003 | `rte_checksum_crc64_init()` must already have been called (process-global, call-once) before any `rte_dual_msgchannel_t` send/receive is used. |
| REQ-DUAL-MSGCHANNEL-004 | `rte_dual_msgchannel_receive()` shall check the received frame's `sender_id` against `expected_peer_id` before verifying its CRC/sequence, and shall reject a mismatch with `RTE_STATUS_HARDWARE_FAULT` without advancing `expected_sequence` — the masquerade defense EN 50159 requires that `rte_checksum_vital_message_verify()` alone does not provide. |
| REQ-DUAL-MSGCHANNEL-005 | A CRC or sequence-continuity failure on receive shall yield `RTE_STATUS_DATA_CORRUPTION` and leave `expected_sequence` unchanged (the failed frame is not consumed into the sequence stream). |

### 3e.3 DualChannel — `rte_dual_channel.h` (ADR-020 §2, Layer 2)

| ID | Requirement |
|---|---|
| REQ-DUAL-CHANNEL-001 | No dynamic allocation; every `rte_dual_channel_t` holds a fixed array of at most `RTE_DUAL_CHANNEL_MAX_LINKS` redundant links. |
| REQ-DUAL-CHANNEL-002 | `rte_dual_channel_send()` shall always transmit the DATA frame on every configured link, regardless of any negotiated `rte_dual_state_t` — it is never gated on state (ADR-020 §2). |
| REQ-DUAL-CHANNEL-003 | After every configured link has been tried, `rte_dual_channel_send()` shall recompute the aggregate `rte_dual_channel_status_t` (`FULL` = all links up, `DEGRADED` = some, `DOWN` = none) and invoke `config->status_callback` only when the aggregate value actually changed. |
| REQ-DUAL-CHANNEL-004 | DATA traffic (`rte_dual_channel_send()`/`_receive()`) and STATE-beacon traffic (`_send_state_frame()`/`_receive_state_frame()`) share the same redundant links and the same up/down bookkeeping, but a STATE frame is fire-and-forget (no ACK wait) and shall never be counted toward or against DATA's own ACK accounting. |
| REQ-DUAL-CHANNEL-005 | `rte_dual_channel_receive()` and `_receive_state_frame()` shall poll every configured link on every call, even after an earlier link in the same sweep already staged a frame — stopping early would let one link (e.g. one with consistently shorter latency) starve every other redundant link of its own auto-ACK indefinitely. |
| REQ-DUAL-CHANNEL-006 | An inbound frame shorter than this layer's own 4-byte `rte_dual_frame_header_t`, or shorter than the full fixed frame its `kind` implies, shall be reported as `RTE_STATUS_DATA_CORRUPTION` rather than silently ignored or misinterpreted. |
| REQ-DUAL-CHANNEL-007 | `rte_dual_channel_send()`'s per-link ACK-wait loop shall keep polling for further frames within `config->ack_timeout_ms` even when `rte_timer_now()` shows no measurable progress between polls (a real round trip may legitimately complete within a single timer tick) — bounded by a fixed cap (`RTE_DUAL_CHANNEL_STALL_POLL_LIMIT`) on consecutive no-progress polls, so a link with a genuinely non-advancing or absent timer backend still cannot spin unboundedly. Added post-acceptance after a live run over a real transport (ADR-022's SITE migration) surfaced that the prior behavior gave up after exactly one poll — see ADR-020's "Post-acceptance fix" section. |
| REQ-DUAL-CHANNEL-008 | If no link produces a usable frame/ACK before `rte_dual_channel_send()`/`_receive()` return, and at least one link's own underlying send/receive reported `RTE_STATUS_HARDWARE_FAULT` (a closed/reset connection, not just "nothing arrived within this poll"), that status shall be returned instead of the generic `RTE_STATUS_TIMEOUT` every other "nothing usable this call" case returns. Found via a `RBC_GP` container-topology failover test: `rte_dual_channel_send()`/`_receive()` previously collapsed *every* non-success outcome — a genuinely dead TCP connection (peer container restarted) exactly as much as an ordinary "peer hasn't answered yet" — into the same `RTE_STATUS_TIMEOUT`, so a consumer's own reconnect logic (`channel_ab_negotiate_execute()`, gating link teardown on "status is neither OK nor TIMEOUT") could never distinguish the two and never reconnected, leaving one side listening forever for a peer that had already come back up on a fresh socket. A malformed/corrupted frame (`RTE_STATUS_DATA_CORRUPTION`) on an otherwise-healthy link is deliberately NOT included in this escalation — it does not indicate a broken transport, only a defended-integrity rejection of one bad frame (REQ-DUAL-CHANNEL-006), and continues to fold into the generic `RTE_STATUS_TIMEOUT` as before. |

### 3e.4 Dual state negotiator — `rte_dual_negotiator.h` (ADR-020 §3)

| ID | Requirement |
|---|---|
| REQ-DUAL-NEGOTIATOR-001 | No dynamic allocation; caller supplies storage and an already-initialized `rte_dual_channel_t`. |
| REQ-DUAL-NEGOTIATOR-002 | `rte_dual_negotiator_execute()` shall never call `rte_safestate_enter()` itself — deciding what a sustained `RTE_DUAL_STATE_UNKNOWN` means for safety stays an application policy decision (ADR-020 §4's "no automatic safety reaction" non-goal). |
| REQ-DUAL-NEGOTIATOR-003 | The initial ONLINE-vs-STANDBY decision shall use an older-startup-timestamp-wins rule, with each side's configured `own_id`/`peer_id` as a deterministic fallback only on an exact timestamp tie (same rule `RBC_GP`'s `site.c` `decide_online()` uses today). |
| REQ-DUAL-NEGOTIATOR-004 | The HOT/COLD determination for whichever side is currently STANDBY shall always be derived from the ONLINE side's own channel-degradation bit — never from the STANDBY side's own self-reported degradation, and never from the ONLINE side's opinion of its own label. This applies symmetrically regardless of which side (own or peer) is the one currently ONLINE. |
| REQ-DUAL-NEGOTIATOR-005 | Loss of peer contact for longer than `config->peer_lost_timeout_ms` shall set `peer_state` to `RTE_DUAL_STATE_UNKNOWN`; `own_state` shall degrade to `RTE_DUAL_STATE_UNKNOWN` too unless it was already `RTE_DUAL_STATE_ONLINE`, in which case it shall remain `RTE_DUAL_STATE_ONLINE` (an active instance keeps acting without needing continuous peer confirmation). |

## 3f. Unified channel factory — `rte_safechannel.h` (ADR-022)

Hides `rte_netlink`/`rte_ipc` from application code: an application
opens one `rte_safechannel_t` by type and host/port endpoints and never
holds a `rte_netlink_handle_t` itself.

| ID | Requirement |
|---|---|
| REQ-SAFECHANNEL-001 | `rte_safechannel_open()` shall open every configured endpoint itself via the registered `rte_netlink` backend; the caller shall never need to call `rte_netlink_open()` or hold a `rte_netlink_handle_t`. |
| REQ-SAFECHANNEL-002 | No dynamic allocation; all storage (`rte_safechannel_t`, including its opened links and wrapped `rte_dual_channel_t`/`rte_channel_t`) is caller-owned and fixed-size, sized to `RTE_SAFECHANNEL_MAX_LINKS`. |
| REQ-SAFECHANNEL-003 | `rte_safechannel_send()`/`_receive()` shall behave identically to the caller regardless of `config.type` — a uniform facade over `rte_dual_channel_t`/`rte_channel_t`. |

## 3g. Checksum / CRC-64 data integrity — `rte_checksum.h`

CRC-64 computation and a "vital message" envelope (sequence + sender +
CRC-64) used by `rte_dual_msgchannel` (REQ-DUAL-MSGCHANNEL-003) and
`rte_checkpoint` for cross-channel/cross-site data integrity. Depends
only on `rte_timer` (best-effort message timestamping — see
REQ-CHECKSUM-005's note that a missing timer backend degrades
gracefully, not a hard failure of message creation), not on `rte_log`
or `rte_safestate` despite once `#include`-ing both unused.

| ID | Requirement |
|---|---|
| REQ-CHECKSUM-001 | `rte_checksum_crc64_init()` shall be callable exactly once; a subsequent call before any re-init mechanism exists shall return `RTE_STATUS_ALREADY_INITIALIZED` and leave the already-selected table/polynomial unchanged. |
| REQ-CHECKSUM-002 | `rte_checksum_crc64()` shall return 0 — never dereferencing `data` — if the module is not yet initialized, if its lookup table is unset, or if `data` is `NULL` while `size` is nonzero. |
| REQ-CHECKSUM-003 | `rte_checksum_crc64()` shall be deterministic and O(n) in `size`, using a precomputed 256-entry lookup table. |
| REQ-CHECKSUM-004 | `rte_checksum_crc64_verify()` shall report `RTE_STATUS_DATA_CORRUPTION` (not merely a boolean) on mismatch and increment `stats.verification_failures`; on match it shall return `RTE_STATUS_OK` and increment `stats.verification_passes`. |
| REQ-CHECKSUM-005 | `rte_checksum_vital_message_create()` shall reject a payload larger than `sizeof(rte_vital_message_t::payload)` with `RTE_STATUS_INVALID_PARAM`, incrementing `stats.payload_oversize`, without writing `msg_out`. |
| REQ-CHECKSUM-006 | `rte_checksum_vital_message_verify()` shall verify the message's CRC-64 before trusting any other field, and report `RTE_STATUS_DATA_CORRUPTION` — without writing to `payload_out`/`payload_size_out` — on either a CRC mismatch or a `sequence_number` that does not equal the caller-supplied `expected_sequence` (incrementing `stats.sequence_errors` in the latter case). |
| REQ-CHECKSUM-007 | `rte_checksum_vital_message_verify()` shall reject a decoded `payload_size` exceeding the caller's `payload_max_size` with `RTE_STATUS_INVALID_PARAM`, incrementing `stats.payload_oversize`, without copying into `payload_out`. |
| REQ-CHECKSUM-008 | `rte_checksum_get_stats()`/`_reset_stats()` are diagnostics-only (never on a safety-decision path); `_get_stats()` returns `RTE_STATUS_INVALID_PARAM` for a `NULL stats_out`, otherwise both always return `RTE_STATUS_OK`. |

## 3h. RBC Train/IL/CTC scenario — `RBC_GP` (ADR-029)

Entirely `RBC_GP`-side (no framework header changes) - kept here
per this document's own cross-repo convention (REQ-APPMANAGER-011 already
set this precedent for a `RBC_GP`-discovered requirement). See
ADR-029 for the full design and the live debugging that produced several
of these.

| ID | Requirement |
|---|---|
| REQ-RBC-001 | **REVISED (this session, wording only — see status note below).** The RBC envelope (`rbc_envelope_t`, `rbc_wire_types.h`) shall carry a real Subset-026-style header — `NID_MESSAGE` (message identity), `L_MESSAGE` (message length in bytes, genuinely variable per message kind), `T_TRAIN` (message timestamp) — and MAY be variable-length per message kind, decoded/encoded via `L_MESSAGE` rather than a single fixed frame size for every kind. This supersedes the prior fixed-28-byte-for-every-kind mandate to allow real Subset-026 message/packet shapes (nested/`N_ITER`-repeated packets, e.g. Packet 15 Movement Authority) that do not fit a flat fixed-size struct — see `RBC_Test_Sim_Train/src/train/message_catalog.json`'s `_subset026Reference`/`_subset026PacketsReference` for the real field/packet data this now needs to support. **Status: partially implemented (this session).** `rbc_wire_types.h`/`rbc_wire.c` (C) and `RBC_Test_Sim_Core/src/simcore/rbc_wire.py` (Python mirror) now carry a real, always-fully-populated 80-byte envelope (`RBC_ENVELOPE_WIRE_SIZE`/`ENVELOPE_SIZE`) with genuine `NID_MESSAGE`/`L_MESSAGE`/`T_TRAIN` header fields, plus the real flat Subset-026 content fields for message 146 (`t_train_ack`) and message 136 (the ten real Packet 0 fields — `nid_lrbg`, `q_dirlrbg`, `q_dlrbg`, `l_doubtover`, `l_doubtunder`, `q_length`, `v_train`, `q_dirtrain`, `m_mode`, `m_level`). `L_MESSAGE` genuinely varies by kind (`content_size_for_kind()`/`build_envelope()`'s own calculation) and is checked on decode, but the frame itself is NOT actually variable-length on the wire — every kind still occupies the same fixed 80-byte slot, with fields not meaningful for a given kind simply left zero, specifically to avoid the C/Python offset-aliasing bug this approach was chosen to sidestep (see `rbc_wire.c`'s own header comment). Byte-for-byte C/Python encode parity verified for both message 146 and 136. Still NOT done: a genuinely variable-length wire frame (so an unrelated kind doesn't pay for fields it never uses), and the nested/`N_ITER`-repeated packet content (e.g. Packet 15 Movement Authority for message 3) — both remain separately-scoped follow-on work; check `rbc_wire_types.h`'s own header comment for the current exact field layout before relying on either doc. |
| REQ-RBC-002 | `rbc_wire_decode()` shall reject a frame whose leading kind byte is not a recognized `rbc_msg_kind_t` value, returning `false` and leaving `*out_env` unmodified, rather than casting an out-of-range byte into the enum. |
| REQ-RBC-003 | Every link that can carry more than one distinct envelope within a single report cycle (C's Train/IL/A/B links, A/B's own link from C) shall demultiplex arrivals through a fixed-capacity single-producer/single-consumer queue (`rbc_envelope_queue_t`, `RBC_ENVELOPE_QUEUE_CAPACITY`) drained fully every cycle, not a single-slot "latest value only" primitive - a link carrying interleaved multi-train traffic can legitimately receive more than one distinct event before the next drain. |
| REQ-RBC-004 | `rbc_envelope_queue_push()` shall return `false` (dropping the new entry) rather than growing dynamically when the queue is full (`RBC_ENVELOPE_QUEUE_CAPACITY` unread entries already pending); the caller shall log the drop rather than fail silently. |
| REQ-RBC-005 | Any code path that indexes a per-train array (`ctx->sessions[]`, `train_rx_queue[]`, etc.) by a wire-supplied `train_id` shall validate `1 <= train_id <= SAFEAPI_EXAMPLE_MAX_TRAINS` first and drop (logged) an out-of-range value, never indexing out of bounds with unchecked wire input. |
| REQ-RBC-006 | On a STANDBY-to-ONLINE promotion (`channel_ab_negotiate.c`'s `apply_state_transfer()`), the whole train-session table (`ctx->sessions[]`) shall be overwritten from the transferred snapshot unconditionally - unlike the transferred cycle counter (still gated by `SAFEAPI_EXAMPLE_TRANSFER_POLICY_ENV`), there is no operator-configurable "restart" policy for live train sessions: a promoted site refusing to remember an in-flight Movement Authority would be unsafe, not a preference. |
| REQ-RBC-007 | Cross-compare (`channel_ab_crosscompare.c`) shall vote on the whole per-train session state relevant to the RBC's own decisions (`in_use`/`train_id`/`cycle`/`d_lrbg`/`granted_length`/`ma_seq`/`ma_acked`), not a single scalar value - `ma_pending_send` and the CTC-notification-sent flags are local scratch only and shall be excluded, since they carry no cross-compare-relevant decision content. The compared payload shall be the wire-encoded form, not the raw `train_session_t` struct, since `rte_cross_comparator_execute()` compares via raw `memcmp()` and the struct's mixed-width members admit compiler-inserted padding a raw comparison would treat as significant. |
| REQ-RBC-008 | `channel_ab_crosscompare_execute()` shall skip (not disagree) cross-comparing a given train for up to `SAFEAPI_EXAMPLE_XCOMPARE_SYNC_SKIP_LIMIT` consecutive cycles while its local and peer session snapshots do not yet field-match, to tolerate the ordinary asynchronous-arrival timing skew between two independently-cycling channels; past that bound it shall fall through to the real comparator so a genuine, persistent divergence (e.g. a relay datagram lost to only one channel) is still detected and reacted to, not silently skipped forever. |
| REQ-RBC-009 | `monitor_c_init()` shall NOT block C's own startup waiting for a Train/IL/CTC client to connect (unlike the A/B links, which are this project's own co-deployed, expected-reachable containers) - each such link's background rx task shall start with a NULL handle and establish the connection lazily on its own reconnect-forever loop, since a Train/IL/CTC sim is a genuinely external, opportunistically-connecting client that may not be running yet. |

## 4. Traceability

Every `REQ-*` ID in this document appears verbatim in the corresponding
header's Doxygen comment in `include/safeapi/`, **except**
`REQ-OAL-BACKEND-001/002/003` in section 2, which are a documentation-only
consolidation of a pattern repeated across all seven `REQ-OAL-*-01X`
"register_backend" entries (see the note above section 2.1) and
intentionally do not appear as literal source tags.

Regenerate the source-side list
(`grep -rhoE "REQ-[A-Z]+-[A-Z]+-[0-9]+" include | sort -u`) whenever a
module is added or changed, and update both this file and the source
comment together — they are required to stay in sync by inspection, since
there is currently no automated cross-check between the two (a natural
next step: a CI script that fails if a `REQ-*` ID appears in code but not
here, or vice versa, ignoring the documented `REQ-OAL-BACKEND-*` exception).
