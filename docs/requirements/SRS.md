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

### 1.1 Status codes — `sapi_status.h` (ADR-001 §3.6)

| ID | Requirement |
|---|---|
| REQ-OAL-COMMON-001 | Every fallible API function in the framework shall return `sapi_status_t` and shall not use exceptions or `errno`-style side channels. |
| REQ-OAL-COMMON-002 | `sapi_status_to_string()` shall return a short, static, human-readable string for every defined status code, for diagnostics/logging use only — never on a safety-decision path. |

### 1.2 Common types — `sapi_types.h` (ADR-001 §3.4)

| ID | Requirement |
|---|---|
| REQ-OAL-COMMON-010 | No dynamic memory allocation is used by the framework after initialization; every stateful object is created from caller-supplied storage. |

### 1.3 Cross-layer data buffer — `sapi_buffer.h` (ADR-002)

| ID | Requirement |
|---|---|
| REQ-COMMON-BUF-001 | `sapi_buffer_t` shall never allocate memory; the caller owns the backing storage for the lifetime of the buffer view. |
| REQ-COMMON-BUF-002 | Every buffer copy operation shall be bounds-checked against the declared capacity and shall never write past it. |
| REQ-COMMON-BUF-010 | `sapi_buffer_init()` shall bind a buffer view to caller-owned storage with an initial length of 0. |
| REQ-COMMON-BUF-011 | `sapi_buffer_clear()` shall reset length to 0 without altering capacity or data. |
| REQ-COMMON-BUF-012 | `sapi_buffer_set_length()` shall reject a length greater than capacity with `SAPI_STATUS_RESOURCE_EXHAUSTED`. |
| REQ-COMMON-BUF-013 | `sapi_buffer_copy_in()` shall perform a bounds-checked copy of external data into the buffer and update length; a source exceeding capacity shall be rejected without partial modification. |
| REQ-COMMON-BUF-014 | `sapi_buffer_copy_out()` shall perform a bounds-checked copy of the buffer's valid bytes to an external destination and report the number of bytes copied. |
| REQ-COMMON-BUF-015 | `sapi_buffer_as_const()` shall produce a read-only view of the buffer's currently valid bytes. |
| REQ-COMMON-BUF-016 | `sapi_buffer_is_valid()` shall perform a defensive check that data is non-NULL and length does not exceed capacity. |
| REQ-COMMON-BUF-020..025 | `sapi_buffer_write_u16/u32/u64_le/be()` shall append the value's bytes in the stated byte order at the buffer's current length and advance it; byte order is always explicit at the call site (ADR-006 §2.5, no default). |
| REQ-COMMON-BUF-026..031 | `sapi_buffer_read_u16/u32/u64_le/be()` shall decode a value in the stated byte order from the given offset without mutating the buffer. |

### 1.4 Checked integer casting — `sapi_cast.h` (ADR-003)

| ID | Requirement |
|---|---|
| REQ-COMMON-CAST-001 | A checked cast function shall never write to `*out` when the source value does not fit the destination type; the caller's variable is left exactly as passed in. |
| REQ-COMMON-CAST-002 | `out == NULL` shall yield `SAPI_STATUS_INVALID_PARAM` without dereferencing `out`. |
| REQ-COMMON-CAST-003 | A value that does not fit the destination type's range shall yield `SAPI_STATUS_VALUE_OUT_OF_RANGE`. |

Applies uniformly to all 72 `sapi_cast_<from>_to_<to>` functions covering
every ordered pair among `int8_t`/`int16_t`/`int32_t`/`int64_t`/
`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`/`size_t` — see ADR-003 §2.1 for
the full type matrix; not enumerated as 72 separate IDs since all 72
satisfy the same three requirements by construction.

### 1.5 Safe-state transitions — `sapi_safestate.h` (ADR-004)

| ID | Requirement |
|---|---|
| REQ-COMMON-SAFESTATE-001 | Handler storage shall be a fixed array of 3 slots (one per `sapi_safestate_level_t` value); no dynamic allocation. |
| REQ-COMMON-SAFESTATE-002 | `SAPI_SAFESTATE_LEVEL_SAFE` and `SAPI_SAFESTATE_LEVEL_REBOOT` shall never return control to the caller — even if no handler is registered, the registered handler itself returns, or the requested level value is invalid — falling back to a defensive infinite loop. |
| REQ-COMMON-SAFESTATE-010 | `sapi_safestate_register_handler()` shall reject a NULL handler or an unrecognized level with `SAPI_STATUS_INVALID_PARAM`; re-registering for the same level shall replace the previous handler. |
| REQ-COMMON-SAFESTATE-011 | `sapi_safestate_enter()` shall invoke the registered handler for the given level (if any) before applying REQ-COMMON-SAFESTATE-002's non-return guarantee. |

`SAPI_ASSERT` is always active in every build configuration (no
`NDEBUG`-style compile-out) and, on a false condition, calls
`sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE, SAPI_SAFESTATE_REASON_ASSERT_FAILED, ...)`
— this behavior is a consequence of REQ-COMMON-SAFESTATE-002/011 applied
to the `SAFE` level and is not tracked as a separate requirement ID.

### 1.6 Bounded string manipulation — `sapi_string.h` (ADR-006)

| ID | Requirement |
|---|---|
| REQ-COMMON-STR-001 | No dynamic allocation; the caller owns the backing storage for the lifetime of the string. |
| REQ-COMMON-STR-002 | `sapi_string_copy()` shall never call `strlen()` on its source; it scans for a NUL only up to the destination's capacity and fails rather than reading past it. |
| REQ-COMMON-STR-003 | Content is treated as raw bytes/ASCII; no multi-byte/UTF-8-aware operations are provided. |
| REQ-COMMON-STR-010 | `sapi_string_init()` shall bind a string to caller-owned storage with an initial length of 0. |
| REQ-COMMON-STR-011 | `sapi_string_clear()` shall reset a string to empty without altering capacity or storage. |
| REQ-COMMON-STR-012 | `sapi_string_length()` shall report the current length (not counting a NUL terminator), returning 0 for a NULL input. |
| REQ-COMMON-STR-013 | `sapi_string_c_str()` shall ensure a NUL terminator is present within capacity (without incrementing length) and shall fail with `SAPI_STATUS_RESOURCE_EXHAUSTED` rather than write past capacity. |
| REQ-COMMON-STR-014 | `sapi_string_copy()` shall replace dest's content per REQ-COMMON-STR-002, leaving dest unmodified on failure. |
| REQ-COMMON-STR-015 | `sapi_string_copy_n()` shall copy an exact-length, not-necessarily-NUL-terminated source without scanning it. |
| REQ-COMMON-STR-016 | `sapi_string_concat()` shall append to dest's content, scanning src for a NUL only up to dest's remaining capacity, leaving dest unmodified on failure. |
| REQ-COMMON-STR-017 | `sapi_string_compare()` shall compare up to the shorter string's length, then by length, reporting the result via an output parameter. |
| REQ-COMMON-STR-018 | `sapi_string_find_char()` shall report "not found" as a normal (non-error) outcome via an output parameter. |
| REQ-COMMON-STR-019 | `sapi_string_find_substr()` shall report "not found" as a normal outcome; an empty needle is always found at index 0. |
| REQ-COMMON-STR-020 | `sapi_string_split_next()` shall use only caller-owned cursor state (no hidden/global state, unlike `strtok`), producing a zero-copy view of each token. |
| REQ-COMMON-STR-021..024 | `sapi_string_from_u32/i32/u64/i64()` shall format the value as base-10 ASCII, replacing dest's content. |
| REQ-COMMON-STR-025..028 | `sapi_string_to_u32/i32/u64/i64()` shall parse base-10 ASCII, yielding `SAPI_STATUS_INVALID_PARAM` for malformed input and `SAPI_STATUS_VALUE_OUT_OF_RANGE` for a value that doesn't fit the destination type. |

## 2. OS Abstraction Layer

Every OAL service below shares the backend-registration contract defined
once in ADR-005 rather than repeating it per service. **Note:**
`REQ-OAL-BACKEND-001/002/003` below are a documentation-only consolidation
for readability — unlike every other ID in this SRS, they do **not**
appear verbatim as a tag in any header comment (see the traceability
caveat in section 4); the behavior they describe is what each per-service
`REQ-OAL-<SERVICE>-01X` "register_backend" entry already requires:

> **REQ-OAL-BACKEND-001** (ADR-005 §2.1, applies to every OAL service):
> `sapi_<service>_register_backend()` shall reject a NULL backend with
> `SAPI_STATUS_INVALID_PARAM`; re-registering shall replace the previous
> backend.
>
> **REQ-OAL-BACKEND-002** (ADR-005 §2.2, applies to every OAL service
> except `sapi_log`): every public function shall validate its own
> parameters before consulting the backend; with no backend registered it
> shall return `SAPI_STATUS_NOT_INITIALIZED`; with a backend registered
> but the corresponding vtable slot NULL, it shall return
> `SAPI_STATUS_NOT_SUPPORTED`.
>
> **REQ-OAL-BACKEND-003** (ADR-005 §2.5, `sapi_log` only): an
> unregistered backend is not an error; `sapi_log_write()` with no
> backend registered shall silently do nothing.

### 2.1 Timer — `sapi_timer.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-TIMER-001 | No dynamic allocation; caller supplies storage for each timer. |
| REQ-OAL-TIMER-002 | Callback execution time is the caller's responsibility to bound; the timer service itself must not block. |
| REQ-OAL-TIMER-003 | The expiry callback executes in a bounded-time, non-blocking context, documented per backend. |
| REQ-OAL-TIMER-010 | `sapi_timer_create()` shall bind a timer to caller-owned storage without starting it. |
| REQ-OAL-TIMER-011 | `sapi_timer_start()` shall start or restart a created timer. |
| REQ-OAL-TIMER-012 | `sapi_timer_stop()` shall stop a running timer and be safe to call on an already-stopped timer. |
| REQ-OAL-TIMER-013 | `sapi_timer_destroy()` shall release any backend resources bound to the timer. |
| REQ-OAL-TIMER-014 | `sapi_timer_now()` shall report the current monotonic time base used by all timers. |
| REQ-OAL-TIMER-015 | `sapi_timer_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.2 Non-volatile memory — `sapi_nvm.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-NVM-001 | Every read shall be integrity-checked (e.g. CRC or redundant-copy voting) before data is returned to the caller. |
| REQ-OAL-NVM-002 | No dynamic allocation; caller supplies storage/buffers. |
| REQ-OAL-NVM-010 | `sapi_nvm_open()` shall open (creating if necessary) a named region. |
| REQ-OAL-NVM-011 | `sapi_nvm_read()` shall read and integrity-check data, returning `SAPI_STATUS_DATA_CORRUPTION` on integrity failure with undefined output buffer content. |
| REQ-OAL-NVM-012 | `sapi_nvm_write()` shall write data and its integrity metadata. |
| REQ-OAL-NVM-013 | `sapi_nvm_sync()` shall force any buffered writes to durable storage. |
| REQ-OAL-NVM-014 | `sapi_nvm_close()` shall close a region handle. |
| REQ-OAL-NVM-015 | `sapi_nvm_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.3 Static memory reservation — `sapi_memory.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-MEM-001 | All pools are reserved during system initialization; reservation after init is backend-defined and may be refused. |
| REQ-OAL-MEM-010 | `sapi_mem_pool_create()` shall reserve a fixed-size-block pool. |
| REQ-OAL-MEM-011 | `sapi_mem_pool_acquire()` shall return `SAPI_STATUS_RESOURCE_EXHAUSTED` when no blocks remain. |
| REQ-OAL-MEM-012 | `sapi_mem_pool_release()` shall return a previously acquired block to its pool. |
| REQ-OAL-MEM-013 | `sapi_mem_pool_stats()` shall report current free/used block counts. |
| REQ-OAL-MEM-014 | `sapi_mem_pool_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.4 Task/thread scheduling — `sapi_task.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-TASK-001 | No dynamic allocation; caller supplies storage and a fixed-size stack/context region. |
| REQ-OAL-TASK-002 | Priorities are fixed at creation time; dynamic priority inheritance/inversion handling is a backend/RTOS concern, not assumed by this API. |
| REQ-OAL-TASK-010 | `sapi_task_create()` shall create a task in the suspended state. |
| REQ-OAL-TASK-011 | `sapi_task_start()` shall start a created task. |
| REQ-OAL-TASK-012 | `sapi_task_suspend()` shall suspend a running task. |
| REQ-OAL-TASK-013 | `sapi_task_destroy()` shall terminate and destroy a task. |
| REQ-OAL-TASK-014 | `sapi_task_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.5 Inter-process/inter-task communication — `sapi_ipc.h` (ADR-001 §4)

| ID | Requirement |
|---|---|
| REQ-OAL-IPC-001 | Queues are bounded and statically sized at creation; no unbounded growth. |
| REQ-OAL-IPC-002 | Send/receive shall accept an explicit timeout and shall never block indefinitely by default. |
| REQ-OAL-IPC-010 | `sapi_ipc_create()` shall create a bounded message channel. |
| REQ-OAL-IPC-011 | `sapi_ipc_send()` shall block at most `timeout_ms`, returning `SAPI_STATUS_TIMEOUT` if the queue stays full for the whole timeout. |
| REQ-OAL-IPC-012 | `sapi_ipc_receive()` shall block at most `timeout_ms`, returning `SAPI_STATUS_TIMEOUT` if no message arrives. |
| REQ-OAL-IPC-013 | `sapi_ipc_destroy()` shall destroy a message channel. |
| REQ-OAL-IPC-014 | `sapi_ipc_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.6 Logging/diagnostics — `sapi_log.h` (ADR-001 §4, non-safety-related)

| ID | Requirement |
|---|---|
| REQ-OAL-LOG-001 | Log calls are best-effort and non-blocking; a full backend buffer silently drops the newest entries rather than blocking or erroring the caller's control flow. This service shall never sit on a safety execution path. |
| REQ-OAL-LOG-010 | `sapi_log_init()` shall be safe to call once at startup. |
| REQ-OAL-LOG-011 | `sapi_log_write()` shall be non-blocking and never fail the caller's control flow. |
| REQ-OAL-LOG-012 | `sapi_log_register_backend()` per REQ-OAL-BACKEND-001, with the REQ-OAL-BACKEND-003 exception for the unregistered case. |
| REQ-OAL-LOG-013 | `sapi_log_level_to_string()` shall return a fixed, non-NULL string for every `sapi_log_level_t` value, including an unrecognized one ("UNKNOWN"). |
| REQ-OAL-LOG-014 | `sapi_log_write_event()` shall emit `Site=<site> Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src> Destination=<dst> Type=<type> Info=<info>[ <extra_fields>]` (space-separated `Key=Value` pairs, fixed order) as the `message` passed to the registered backend's `write()`, with `source` as that call's `tag`; `Timestamp` shall degrade to `0` (never block or skip the event) if no `sapi_timer` backend is registered or `sapi_timer_now()` fails; the whole call shall be a silent no-op under the same conditions as `sapi_log_write()` (REQ-OAL-LOG-001) when no `sapi_log` backend is registered. |

### 2.7 Controlled reboot — `sapi_reboot.h` (ADR-004 §3)

| ID | Requirement |
|---|---|
| REQ-OAL-REBOOT-001 | `sapi_reboot_request()` is not expected to return on success; a return only occurs if the backend cannot perform the reboot. |
| REQ-OAL-REBOOT-010 | `sapi_reboot_request()` shall request a controlled system restart via the registered backend. |
| REQ-OAL-REBOOT-011 | `sapi_reboot_register_backend()` per REQ-OAL-BACKEND-001. |

### 2.8 Point-to-point network link — `sapi_netlink.h` (ADR-001 §4, ADR-005, ADR-021, ADR-027)

Framework ships the interface and validate-then-dispatch layer only; a
concrete backend (e.g. POSIX UDP sockets) is integrator-supplied and
lives with the application that registers it — see `safeAPIRBC2oo2`'s
`src/posix_backend/sapi_posix_backend_netlink.c`. This table was backfilled
alongside ADR-027 (TCP → UDP migration) — the requirement IDs were
already cited in `sapi_netlink.h`'s own header comments beforehand, but
had no matching table entry here; the wording below reflects the
now-transport-agnostic contract, not the earlier TCP-specific one.

| ID | Requirement |
|---|---|
| REQ-OAL-NETLINK-001 | No dynamic allocation; caller supplies storage (`sapi_netlink_storage_t`). |
| REQ-OAL-NETLINK-002 | `sapi_netlink_open()` shall never block longer than `config->connect_timeout_ms`. |
| REQ-OAL-NETLINK-003 | Send/receive shall accept an explicit timeout and shall never block indefinitely by default. |
| REQ-OAL-NETLINK-010 | `sapi_netlink_open()` shall establish a point-to-point link per `config->role` (LISTEN binds and waits for its one peer; CONNECT dials), returning `SAPI_STATUS_TIMEOUT` if not established within `connect_timeout_ms`. |
| REQ-OAL-NETLINK-011 | `sapi_netlink_send()` shall send one fixed-size message, blocking at most `timeout_ms`; `SAPI_STATUS_HARDWARE_FAULT` is returned only when the backend can positively confirm the peer is gone — a guarantee no backend can make on every failure mode (e.g. a lost/silently-dropped peer on an unreliable transport), so callers must not treat its absence as proof of liveness. |
| REQ-OAL-NETLINK-012 | `sapi_netlink_receive()` shall receive one fixed-size message, blocking at most `timeout_ms`; `SAPI_STATUS_DATA_CORRUPTION` is returned if the backend can detect the received message violated this link's wire contract (e.g. wrong length) but not necessarily its content. |
| REQ-OAL-NETLINK-013 | `sapi_netlink_close()` shall close a link; the handle is invalid to use afterward. |
| REQ-OAL-NETLINK-014 | This service provides no message ordering, deduplication, or delivery guarantee of its own — a backend may be built on an unreliable transport (e.g. UDP). Any such guarantee is the caller's responsibility (`sapi_dual_msgchannel`/`sapi_dual_channel`, ADR-020, is the reusable sequence+CRC+ACK layer for callers that need one). |

## 3. Project-wide requirements (CLAUDE.md, not yet tagged per-function)

These apply across every module above and are enforced by convention and
the MISRA compliance review (`docs/MISRA_COMPLIANCE_REPORT.md`) rather
than an individual `REQ-*` tag on every function:

- Target language: Embedded C (C99/C11); MISRA C:2012 Mandatory/Required.
- No dynamic memory allocation (`malloc`/`free`/`realloc` banned) — see
  MISRA report §2 for verification evidence.
- Fixed-width types (`<stdint.h>`) instead of native `int`/`long`.
- All integer type conversions go through `sapi_cast_*` (§1.3) — no bare
  C-style casts.
- No recursion, no uninitialized variables, no standard `errno`/`assert()`
  in production paths (`SAPI_ASSERT`, §1.4, is the replacement).

## 3a. Distributed channel synchronization (ADR-017)

Not yet backfilled into this document for every module added since the
sections above were written (`sapi_checksum` currently has no
REQ-tagged entries here despite existing in `include/`/`src/` — a
pre-existing gap, not introduced by this section). This section covers
the two modules added by ADR-017.

### 3a.1 Checkpoint rendezvous — `sapi_checkpoint.h` (ADR-017 §2.2)

| ID | Requirement |
|---|---|
| REQ-CHECKPOINT-001 | `sapi_channel_checkpoint()` shall never block longer than `config->max_delay_ms`. |
| REQ-CHECKPOINT-002 | A checkpoint-arrival reply that fails CRC verification or carries a different `checkpoint_id` shall not count toward `expected_node_count`. |
| REQ-CHECKPOINT-003 | If fewer than `expected_node_count` valid replies arrive within `max_delay_ms`, `sapi_channel_checkpoint()` shall call `sapi_safestate_enter()` at `SAPI_SAFESTATE_LEVEL_SAFE` with `SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT` before returning `SAPI_STATUS_TIMEOUT`. |

### 3a.2 Clock synchronization (diagnostic only) — `sapi_clocksync.h` (ADR-017 §2.3)

| ID | Requirement |
|---|---|
| REQ-CLOCKSYNC-001 | `sapi_clocksync_get_offset_ms()` and `sapi_clocksync_get_quality()` shall return `SAPI_STATUS_NOT_INITIALIZED` if no backend has been registered. |
| REQ-CLOCKSYNC-002 | This module shall never be called from, or influence the outcome of, `sapi_channel_checkpoint()` or any other vital comparison — checkpoint-ID rendezvous, not clock agreement, is the basis of comparison correctness (ADR-017 §2.3). |

## 3b. Watchdog — `sapi_watchdog.h` (partial)

Not a full backfill of this module (see 3a's own note on the pre-existing
gap) - just the `SAPI_WATCHDOG_ACTION_FAILOVER` action, added when a real
integrator (safeAPIRBC2oo2's dual-channel A/B link and SITE<->SITE
heartbeat) needed a "my redundant peer stopped responding" reaction and
found the action a documented dead stub.

| ID | Requirement |
|---|---|
| REQ-WATCHDOG-001 | `sapi_watchdog_create()` shall return `SAPI_STATUS_INVALID_PARAM` if `config->action` is `SAPI_WATCHDOG_ACTION_FAILOVER` and `config->custom_action` is `NULL` (same requirement already in force for `SAPI_WATCHDOG_ACTION_CUSTOM`). |
| REQ-WATCHDOG-002 | On timeout, a watchdog configured with `SAPI_WATCHDOG_ACTION_FAILOVER` shall invoke `config->custom_action(config->context)` — identical dispatch to `SAPI_WATCHDOG_ACTION_CUSTOM` — and shall not itself decide what the timeout means; that decision belongs to the integrator's `custom_action`. |

## 3c. Application lifecycle hooks and cycle checkpoint — `sapi_appmanager.h` (ADR-019)

`sapi_appmanager` was one of the modules 3a flagged as not yet backfilled;
this section starts that backfill with the four `REQ-APPMANAGER-*` IDs
introduced or already present in the header as of ADR-019, not a full
retroactive pass over every pre-existing behavior of the module.

| ID | Requirement |
|---|---|
| REQ-APPMANAGER-001 | Applications shall use the Application Manager (`sapi_appmanager_run()`) for controlled initialization, execution, and shutdown lifecycle. |
| REQ-APPMANAGER-002 | Applications shall implement all mandatory operations in `sapi_appmanager_operations_t` (`init`, `execute`, `shutdown`, `get_name`, `get_version`); `pre_execute` and `post_execute` are optional and may be left `NULL`. |
| REQ-APPMANAGER-006 | `sapi_appmanager_run()` shall treat a `NULL` `pre_execute` or `post_execute` as "skip this stage", not an error, and shall not call it. |
| REQ-APPMANAGER-007 | `sapi_appmanager_run()` shall handle a checkpoint-stage result identically to `pre_execute`/`execute`/`post_execute`: on non-`SAPI_STATUS_OK`, log it, increment `error_count`, and check `error_threshold` — no separate reaction path for a checkpoint failure/timeout. |
| REQ-APPMANAGER-008 | A GENUINE checkpoint-stage failure (the rendezvous itself did not confirm in time, `config->checkpoint->voter` non-`NULL`) shall not be retried faster than `config->checkpoint->max_delay_ms` (measured from immediately before the failing `sapi_channel_checkpoint()` call), when a timer backend is registered. Originally written to also cover `voter == NULL` (see REQ-APPMANAGER-011, which supersedes that part of this requirement's own history) — found via a `safeAPIRBC2oo2` failover test: the checkpoint stage runs *before* `pre_execute()` every cycle (REQ-APPMANAGER-007's own ordering) so a desynced peer is caught before either channel acts on that cycle's data — but `pre_execute()` is the only place any consumer's own cycle pacing lives (`sapi_appmanager_run()` owns no timer itself, ADR-001 §4), so a checkpoint stage that failed immediately used to spin the whole loop as fast as the CPU allowed, one failed attempt and one log line at a time (observed: ~90000 iterations/second, 1.8M log lines in ~20s). Fixed by flooring the retry interval at the checkpoint's own configured `max_delay_ms` via a bounded `sapi_timer_now()` poll — see `sapi_appmanager_pace_failed_checkpoint()` in `sapi_appmanager.c`. No-op (degrades to the old, unpaced behavior) if no timer backend is registered or `max_delay_ms` is 0 — neither can be paced without fabricating a wait nobody configured. |
| REQ-APPMANAGER-009 | `sapi_appmanager_run()` is the single entry point for an application's lifecycle (REQ-APPMANAGER-001) and shall refuse re-entry: a call arriving while a previous call is still mid-lifecycle (`SAPI_APP_STATE_INITIALIZING`/`_RUNNING`/`_SHUTTING_DOWN`) shall return `EXIT_FAILURE` immediately, without altering any state belonging to the call already in progress. A new call made only after a previous one has fully returned (state `SAPI_APP_STATE_SHUTDOWN`/`_ERROR`) is unaffected — this framework's own test suite relies on exactly that sequential-call pattern (ADR-026). |
| REQ-APPMANAGER-010 | The moment `ops->init()` returns `SAPI_STATUS_OK`, `sapi_appmanager_run()` shall lock the application's setup phase (`sapi_lifecycle_lock()`) for the remainder of that run, and shall unlock it (`sapi_lifecycle_unlock()`) both at the start of every call and the moment that call's own execution phase ends — see ADR-026 and REQ-LIFECYCLE-001. |
| REQ-APPMANAGER-011 | `sapi_appmanager_run()` shall treat `config->checkpoint->voter == NULL` identically to `config->checkpoint == NULL`: skip the checkpoint stage entirely for that cycle (no `sapi_channel_checkpoint()` call, no pacing, no error counted) and proceed to `pre_execute()`/`execute()`/`post_execute()` normally — never as a failed stage (superseding REQ-APPMANAGER-008's original scope for this specific case). Found live (ADR-027 Phase 3, `safeAPIRBC2oo2`): before this fix, a caller-paused checkpoint (`voter` toggled to `NULL` while its own underlying link is known down — a normal, documented pattern, not rare) was fed to `sapi_channel_checkpoint()`, got back `SAPI_STATUS_INVALID_PARAM`, and had that treated as a failed stage — paced (REQ-APPMANAGER-008) and `continue`d, which skips *every later stage* for as long as `voter` stays `NULL`. This silently starved every one of a consumer's own per-cycle safety checks too, including `sapi_watchdog_timer_tick()` (this framework's own single-threaded, timestamp-comparison watchdog design — REQ-WATCHDOG-*, no watchdog has an independent timer/thread of its own) — so a watchdog-driven fault reaction (e.g. `safeAPIRBC2oo2`'s own REBOOT-on-negotiation-link-loss) could never fire during exactly the sustained-outage scenario it exists for, because the cyclic executive never reached the code that ticks it. Confirmed fixed live: the same fault scenario that previously spun at ~100% CPU with the watchdog silently never firing now reboots correctly within its own configured timeout. |

## 3c-bis. Application setup-phase lock — `sapi_lifecycle.h` (ADR-026)

| ID | Requirement |
|---|---|
| REQ-LIFECYCLE-001 | Every setup-only constructor this framework ships (`sapi_timer_create()`, `sapi_channel_init()`, `sapi_voter_init()`/`_register_channel()`, `sapi_cross_comparator_init()`/`_register_channel()`, `sapi_watchdog_create()`) shall reject its call with `SAPI_STATUS_INVALID_STATE` once `sapi_lifecycle_lock()` has been called and `sapi_lifecycle_unlock()` has not been called since — i.e. once the application's setup phase is locked (see REQ-APPMANAGER-010). `sapi_netlink_open()`, `sapi_dual_channel_init()`, and `sapi_dual_negotiator_init()` are deliberately **not** gated by this lock: all three are legitimately re-invoked after the setup phase locks by an application's own reconnect-after-link-loss logic (e.g. `safeAPIRBC2oo2`'s `channel_ab_io.c`/`channel_ab_negotiate_reconnect()`), re-establishing a link the application already owns rather than adding a new one its own design never accounted for. |
| REQ-LIFECYCLE-002 | The setup-phase lock shall be a single, process-wide flag (no dynamic allocation, no OS dependency, no per-`sapi_appmanager_config_t` instance) — this framework has no concept of more than one concurrently-running application per process, matching `sapi_appmanager`'s own existing `g_app_state` single-instance assumption. |

## 3d. Single-link channel, N-way voting, and 2-way cross-comparison — ADR-025

ADR-025 split what this section previously described (a single
`sapi_channel` type combining a redundant transport link with N-way
voting logic, `channel_count`/`voting_strategy` included in its own
init config) into three modules with a clean responsibility boundary:
`sapi_channel` is now a single point-to-point link only, `sapi_voter`
does N-way 2oo2/2oo3/NMR voting over channels registered into it, and
`sapi_cross_comparator` does 2-way peer comparison. `channel_count`
and `voting_strategy` moved off `sapi_channel_init()`'s config entirely
and onto `sapi_voter_init()`'s — the floor this section used to
describe (previously relaxed by ADR-019 §5.1 to allow a single-channel
NMR voter with `quorum_size == 1`) now lives there instead, unchanged
in substance: `SAPI_VOTING_2OO2` requires exactly 2 registered
channels, `SAPI_VOTING_2OO3` exactly 3, `SAPI_VOTING_NMR` at least 1
with `1 <= quorum_size <= channel_count`.

### 3d.1 Single-link channel — `sapi_channel.h` (`channel_link/`, ADR-025 §2.1)

| ID | Requirement |
|---|---|
| REQ-CHANNEL-001 | No dynamic allocation; caller supplies storage for every `sapi_channel_t`. |
| REQ-CHANNEL-002 | `sapi_channel_init()` shall return `SAPI_STATUS_INVALID_PARAM` if `config->send` or `config->recv` is `NULL` — a channel with no way to move data is a construction-time error, not a deferred one. |
| REQ-CHANNEL-003 | `sapi_channel_send()`/`_receive()` shall dispatch to `config->send`/`config->recv` and update `health.send_count`/`health.receive_count` (or the matching `_error_count`, plus `health.last_error`) on every call, regardless of outcome. |
| REQ-CHANNEL-004 | `is_healthy` shall default to `true` at `sapi_channel_init()` and shall never be cleared automatically by a send/receive failure — only an explicit `sapi_channel_set_healthy(handle, false)` call by the channel's owner (e.g. `sapi_voter`, `sapi_cross_comparator`) may mark it unhealthy. A single transient I/O failure alone does not condemn a link; that judgment belongs to whichever component is tracking the pattern of failures across calls. |

### 3d.2 N-way voter — `sapi_voter.h` (ADR-025 §2.2)

| ID | Requirement |
|---|---|
| REQ-VOTER-001 | No dynamic allocation; every `sapi_voter_t` holds a fixed array of at most `SAPI_VOTER_MAX_CHANNELS` (8) registered `sapi_channel_t *` pointers. |
| REQ-VOTER-002 | `sapi_voter_init()` shall return `SAPI_STATUS_INVALID_PARAM` for a `voting_strategy` other than `SAPI_VOTING_2OO2`/`_2OO3`/`_NMR`, or for `SAPI_VOTING_NMR` with `quorum_size == 0`. |
| REQ-VOTER-003 | `sapi_voter_send()`/`_receive()` shall return `SAPI_STATUS_INVALID_PARAM` unless the number of currently registered channels matches the configured strategy's required count (`SAPI_VOTING_2OO2` = exactly 2, `SAPI_VOTING_2OO3` = exactly 3, `SAPI_VOTING_NMR` = at least `quorum_size`). |
| REQ-VOTER-004 | `sapi_voter_receive()` shall group every successfully-received, per-channel payload into equality classes (via `config->compare` if registered, otherwise `memcmp`) and select the *largest* class, reporting `SAPI_VOTING_AGREED` with that class's data if its size meets the strategy's required quorum (`voter_required_quorum()`), or `SAPI_VOTING_DISAGREED` otherwise. This is a majority vote across all registered channels, not a pairwise comparison against a single reference channel — see ADR-025 §1 for the bug this replaced. |
| REQ-VOTER-005 | On `SAPI_VOTING_DISAGREED`, `sapi_voter_receive()` shall call `sapi_safestate_enter()` at `SAPI_SAFESTATE_LEVEL_SAFE` when `config->trigger_safestate_on_disagreement` is `true` (the default), and shall always invoke `config->on_disagreement` (if registered) regardless of that flag. |

### 3d.3 2-way cross-comparator — `sapi_cross_comparator.h` (ADR-025 §2.3)

| ID | Requirement |
|---|---|
| REQ-CROSSCOMPARATOR-001 | No dynamic allocation; every `sapi_cross_comparator_t` holds exactly 2 registered `sapi_channel_t *` slots. |
| REQ-CROSSCOMPARATOR-002 | A 3rd `sapi_cross_comparator_register_channel()` call on an already-fully-registered comparator shall return `SAPI_STATUS_RESOURCE_EXHAUSTED` without disturbing the 2 already-registered channels. |
| REQ-CROSSCOMPARATOR-003 | `sapi_cross_comparator_execute()` shall require both registered channels to be healthy and to successfully receive `data_size` bytes before comparing; any unhealthy channel or receive failure shall short-circuit to `SAPI_VOTING_TIMEOUT`/`SAPI_VOTING_INSUFFICIENT_QUORUM` as appropriate without invoking the compare step. |
| REQ-CROSSCOMPARATOR-004 | The comparison itself shall use `config->compare` if registered, otherwise a full `memcmp()` of the two channels' received payloads — CRC-64 transport-integrity verification (`sapi_checksum`) is a separate, already-applied concern and is never itself treated as "the comparison" (ADR-025 §2.4). |

## 3e. Dual-transfer state negotiation — `sapi_dual` (ADR-020)

Three files under `include/safeapi/dual/`: `sapi_dual_msgchannel.h` (Layer
1 "Channel", one EN 50159-defended message channel over one
`sapi_netlink_handle_t`), `sapi_dual_channel.h` ("DualChannel", 1..N
redundant Layer-1 links with always-send + bounded-ACK-wait delivery and
connection-status tracking), and `sapi_dual_negotiator.h` (own/peer
`sapi_dual_state_t` negotiation driven over an attached DualChannel).
`sapi_dual_types.h` and `sapi_dual_frames.h` hold shared enums/wire
structs with no behavior of their own beyond what the three modules above
require.

### 3e.1 Shared types — `sapi_dual_types.h` (ADR-020 Decision §1)

| ID | Requirement |
|---|---|
| REQ-DUAL-TYPES-001 | `sapi_dual_state_to_string()` and `sapi_dual_channel_status_to_string()` shall return a non-NULL, static string for every defined enum value and a defensive `"UNKNOWN_STATE"`/`"UNKNOWN_STATUS"` respectively for an unrecognized one (same posture as `sapi_log_level_to_string()`, REQ-OAL-LOG-013). |

### 3e.2 Base Channel — `sapi_dual_msgchannel.h` (ADR-020 §1, Layer 1)

| ID | Requirement |
|---|---|
| REQ-DUAL-MSGCHANNEL-001 | No dynamic allocation; caller supplies storage for every `sapi_dual_msgchannel_t`. |
| REQ-DUAL-MSGCHANNEL-002 | `sapi_dual_msgchannel_send()`/`_receive()` shall never block longer than the caller-supplied timeout waiting on `sapi_netlink_send()`/`_receive()`. |
| REQ-DUAL-MSGCHANNEL-003 | `sapi_checksum_crc64_init()` must already have been called (process-global, call-once) before any `sapi_dual_msgchannel_t` send/receive is used. |
| REQ-DUAL-MSGCHANNEL-004 | `sapi_dual_msgchannel_receive()` shall check the received frame's `sender_id` against `expected_peer_id` before verifying its CRC/sequence, and shall reject a mismatch with `SAPI_STATUS_HARDWARE_FAULT` without advancing `expected_sequence` — the masquerade defense EN 50159 requires that `sapi_checksum_vital_message_verify()` alone does not provide. |
| REQ-DUAL-MSGCHANNEL-005 | A CRC or sequence-continuity failure on receive shall yield `SAPI_STATUS_DATA_CORRUPTION` and leave `expected_sequence` unchanged (the failed frame is not consumed into the sequence stream). |

### 3e.3 DualChannel — `sapi_dual_channel.h` (ADR-020 §2, Layer 2)

| ID | Requirement |
|---|---|
| REQ-DUAL-CHANNEL-001 | No dynamic allocation; every `sapi_dual_channel_t` holds a fixed array of at most `SAPI_DUAL_CHANNEL_MAX_LINKS` redundant links. |
| REQ-DUAL-CHANNEL-002 | `sapi_dual_channel_send()` shall always transmit the DATA frame on every configured link, regardless of any negotiated `sapi_dual_state_t` — it is never gated on state (ADR-020 §2). |
| REQ-DUAL-CHANNEL-003 | After every configured link has been tried, `sapi_dual_channel_send()` shall recompute the aggregate `sapi_dual_channel_status_t` (`FULL` = all links up, `DEGRADED` = some, `DOWN` = none) and invoke `config->status_callback` only when the aggregate value actually changed. |
| REQ-DUAL-CHANNEL-004 | DATA traffic (`sapi_dual_channel_send()`/`_receive()`) and STATE-beacon traffic (`_send_state_frame()`/`_receive_state_frame()`) share the same redundant links and the same up/down bookkeeping, but a STATE frame is fire-and-forget (no ACK wait) and shall never be counted toward or against DATA's own ACK accounting. |
| REQ-DUAL-CHANNEL-005 | `sapi_dual_channel_receive()` and `_receive_state_frame()` shall poll every configured link on every call, even after an earlier link in the same sweep already staged a frame — stopping early would let one link (e.g. one with consistently shorter latency) starve every other redundant link of its own auto-ACK indefinitely. |
| REQ-DUAL-CHANNEL-006 | An inbound frame shorter than this layer's own 4-byte `sapi_dual_frame_header_t`, or shorter than the full fixed frame its `kind` implies, shall be reported as `SAPI_STATUS_DATA_CORRUPTION` rather than silently ignored or misinterpreted. |
| REQ-DUAL-CHANNEL-007 | `sapi_dual_channel_send()`'s per-link ACK-wait loop shall keep polling for further frames within `config->ack_timeout_ms` even when `sapi_timer_now()` shows no measurable progress between polls (a real round trip may legitimately complete within a single timer tick) — bounded by a fixed cap (`SAPI_DUAL_CHANNEL_STALL_POLL_LIMIT`) on consecutive no-progress polls, so a link with a genuinely non-advancing or absent timer backend still cannot spin unboundedly. Added post-acceptance after a live run over a real transport (ADR-022's SITE migration) surfaced that the prior behavior gave up after exactly one poll — see ADR-020's "Post-acceptance fix" section. |
| REQ-DUAL-CHANNEL-008 | If no link produces a usable frame/ACK before `sapi_dual_channel_send()`/`_receive()` return, and at least one link's own underlying send/receive reported `SAPI_STATUS_HARDWARE_FAULT` (a closed/reset connection, not just "nothing arrived within this poll"), that status shall be returned instead of the generic `SAPI_STATUS_TIMEOUT` every other "nothing usable this call" case returns. Found via a `safeAPIRBC2oo2` container-topology failover test: `sapi_dual_channel_send()`/`_receive()` previously collapsed *every* non-success outcome — a genuinely dead TCP connection (peer container restarted) exactly as much as an ordinary "peer hasn't answered yet" — into the same `SAPI_STATUS_TIMEOUT`, so a consumer's own reconnect logic (`channel_ab_negotiate_execute()`, gating link teardown on "status is neither OK nor TIMEOUT") could never distinguish the two and never reconnected, leaving one side listening forever for a peer that had already come back up on a fresh socket. A malformed/corrupted frame (`SAPI_STATUS_DATA_CORRUPTION`) on an otherwise-healthy link is deliberately NOT included in this escalation — it does not indicate a broken transport, only a defended-integrity rejection of one bad frame (REQ-DUAL-CHANNEL-006), and continues to fold into the generic `SAPI_STATUS_TIMEOUT` as before. |

### 3e.4 Dual state negotiator — `sapi_dual_negotiator.h` (ADR-020 §3)

| ID | Requirement |
|---|---|
| REQ-DUAL-NEGOTIATOR-001 | No dynamic allocation; caller supplies storage and an already-initialized `sapi_dual_channel_t`. |
| REQ-DUAL-NEGOTIATOR-002 | `sapi_dual_negotiator_execute()` shall never call `sapi_safestate_enter()` itself — deciding what a sustained `SAPI_DUAL_STATE_UNKNOWN` means for safety stays an application policy decision (ADR-020 §4's "no automatic safety reaction" non-goal). |
| REQ-DUAL-NEGOTIATOR-003 | The initial ONLINE-vs-STANDBY decision shall use an older-startup-timestamp-wins rule, with each side's configured `own_id`/`peer_id` as a deterministic fallback only on an exact timestamp tie (same rule `safeAPIRBC2oo2`'s `site.c` `decide_online()` uses today). |
| REQ-DUAL-NEGOTIATOR-004 | The HOT/COLD determination for whichever side is currently STANDBY shall always be derived from the ONLINE side's own channel-degradation bit — never from the STANDBY side's own self-reported degradation, and never from the ONLINE side's opinion of its own label. This applies symmetrically regardless of which side (own or peer) is the one currently ONLINE. |
| REQ-DUAL-NEGOTIATOR-005 | Loss of peer contact for longer than `config->peer_lost_timeout_ms` shall set `peer_state` to `SAPI_DUAL_STATE_UNKNOWN`; `own_state` shall degrade to `SAPI_DUAL_STATE_UNKNOWN` too unless it was already `SAPI_DUAL_STATE_ONLINE`, in which case it shall remain `SAPI_DUAL_STATE_ONLINE` (an active instance keeps acting without needing continuous peer confirmation). |

## 3f. Unified channel factory — `sapi_safechannel.h` (ADR-022)

Hides `sapi_netlink`/`sapi_ipc` from application code: an application
opens one `sapi_safechannel_t` by type and host/port endpoints and never
holds a `sapi_netlink_handle_t` itself.

| ID | Requirement |
|---|---|
| REQ-SAFECHANNEL-001 | `sapi_safechannel_open()` shall open every configured endpoint itself via the registered `sapi_netlink` backend; the caller shall never need to call `sapi_netlink_open()` or hold a `sapi_netlink_handle_t`. |
| REQ-SAFECHANNEL-002 | No dynamic allocation; all storage (`sapi_safechannel_t`, including its opened links and wrapped `sapi_dual_channel_t`/`sapi_channel_t`) is caller-owned and fixed-size, sized to `SAPI_SAFECHANNEL_MAX_LINKS`. |
| REQ-SAFECHANNEL-003 | `sapi_safechannel_send()`/`_receive()` shall behave identically to the caller regardless of `config.type` — a uniform facade over `sapi_dual_channel_t`/`sapi_channel_t`. |

## 3g. Checksum / CRC-64 data integrity — `sapi_checksum.h`

CRC-64 computation and a "vital message" envelope (sequence + sender +
CRC-64) used by `sapi_dual_msgchannel` (REQ-DUAL-MSGCHANNEL-003) and
`sapi_checkpoint` for cross-channel/cross-site data integrity. Depends
only on `sapi_timer` (best-effort message timestamping — see
REQ-CHECKSUM-005's note that a missing timer backend degrades
gracefully, not a hard failure of message creation), not on `sapi_log`
or `sapi_safestate` despite once `#include`-ing both unused.

| ID | Requirement |
|---|---|
| REQ-CHECKSUM-001 | `sapi_checksum_crc64_init()` shall be callable exactly once; a subsequent call before any re-init mechanism exists shall return `SAPI_STATUS_ALREADY_INITIALIZED` and leave the already-selected table/polynomial unchanged. |
| REQ-CHECKSUM-002 | `sapi_checksum_crc64()` shall return 0 — never dereferencing `data` — if the module is not yet initialized, if its lookup table is unset, or if `data` is `NULL` while `size` is nonzero. |
| REQ-CHECKSUM-003 | `sapi_checksum_crc64()` shall be deterministic and O(n) in `size`, using a precomputed 256-entry lookup table. |
| REQ-CHECKSUM-004 | `sapi_checksum_crc64_verify()` shall report `SAPI_STATUS_DATA_CORRUPTION` (not merely a boolean) on mismatch and increment `stats.verification_failures`; on match it shall return `SAPI_STATUS_OK` and increment `stats.verification_passes`. |
| REQ-CHECKSUM-005 | `sapi_checksum_vital_message_create()` shall reject a payload larger than `sizeof(sapi_vital_message_t::payload)` with `SAPI_STATUS_INVALID_PARAM`, incrementing `stats.payload_oversize`, without writing `msg_out`. |
| REQ-CHECKSUM-006 | `sapi_checksum_vital_message_verify()` shall verify the message's CRC-64 before trusting any other field, and report `SAPI_STATUS_DATA_CORRUPTION` — without writing to `payload_out`/`payload_size_out` — on either a CRC mismatch or a `sequence_number` that does not equal the caller-supplied `expected_sequence` (incrementing `stats.sequence_errors` in the latter case). |
| REQ-CHECKSUM-007 | `sapi_checksum_vital_message_verify()` shall reject a decoded `payload_size` exceeding the caller's `payload_max_size` with `SAPI_STATUS_INVALID_PARAM`, incrementing `stats.payload_oversize`, without copying into `payload_out`. |
| REQ-CHECKSUM-008 | `sapi_checksum_get_stats()`/`_reset_stats()` are diagnostics-only (never on a safety-decision path); `_get_stats()` returns `SAPI_STATUS_INVALID_PARAM` for a `NULL stats_out`, otherwise both always return `SAPI_STATUS_OK`. |

## 3h. RBC Train/IL/CTC scenario — `safeAPIRBC2oo2` (ADR-029)

Entirely `safeAPIRBC2oo2`-side (no framework header changes) - kept here
per this document's own cross-repo convention (REQ-APPMANAGER-011 already
set this precedent for a `safeAPIRBC2oo2`-discovered requirement). See
ADR-029 for the full design and the live debugging that produced several
of these.

| ID | Requirement |
|---|---|
| REQ-RBC-001 | **REVISED (this session, wording only — see status note below).** The RBC envelope (`rbc_envelope_t`, `rbc_wire_types.h`) shall carry a real Subset-026-style header — `NID_MESSAGE` (message identity), `L_MESSAGE` (message length in bytes, genuinely variable per message kind), `T_TRAIN` (message timestamp) — and MAY be variable-length per message kind, decoded/encoded via `L_MESSAGE` rather than a single fixed frame size for every kind. This supersedes the prior fixed-28-byte-for-every-kind mandate to allow real Subset-026 message/packet shapes (nested/`N_ITER`-repeated packets, e.g. Packet 15 Movement Authority) that do not fit a flat fixed-size struct — see `TrainRBCSim/src/train/message_catalog.json`'s `_subset026Reference`/`_subset026PacketsReference` for the real field/packet data this now needs to support. **Status: partially implemented (this session).** `rbc_wire_types.h`/`rbc_wire.c` (C) and `SimCore/src/simcore/rbc_wire.py` (Python mirror) now carry a real, always-fully-populated 80-byte envelope (`RBC_ENVELOPE_WIRE_SIZE`/`ENVELOPE_SIZE`) with genuine `NID_MESSAGE`/`L_MESSAGE`/`T_TRAIN` header fields, plus the real flat Subset-026 content fields for message 146 (`t_train_ack`) and message 136 (the ten real Packet 0 fields — `nid_lrbg`, `q_dirlrbg`, `q_dlrbg`, `l_doubtover`, `l_doubtunder`, `q_length`, `v_train`, `q_dirtrain`, `m_mode`, `m_level`). `L_MESSAGE` genuinely varies by kind (`content_size_for_kind()`/`build_envelope()`'s own calculation) and is checked on decode, but the frame itself is NOT actually variable-length on the wire — every kind still occupies the same fixed 80-byte slot, with fields not meaningful for a given kind simply left zero, specifically to avoid the C/Python offset-aliasing bug this approach was chosen to sidestep (see `rbc_wire.c`'s own header comment). Byte-for-byte C/Python encode parity verified for both message 146 and 136. Still NOT done: a genuinely variable-length wire frame (so an unrelated kind doesn't pay for fields it never uses), and the nested/`N_ITER`-repeated packet content (e.g. Packet 15 Movement Authority for message 3) — both remain separately-scoped follow-on work; check `rbc_wire_types.h`'s own header comment for the current exact field layout before relying on either doc. |
| REQ-RBC-002 | `rbc_wire_decode()` shall reject a frame whose leading kind byte is not a recognized `rbc_msg_kind_t` value, returning `false` and leaving `*out_env` unmodified, rather than casting an out-of-range byte into the enum. |
| REQ-RBC-003 | Every link that can carry more than one distinct envelope within a single report cycle (C's Train/IL/A/B links, A/B's own link from C) shall demultiplex arrivals through a fixed-capacity single-producer/single-consumer queue (`rbc_envelope_queue_t`, `RBC_ENVELOPE_QUEUE_CAPACITY`) drained fully every cycle, not a single-slot "latest value only" primitive - a link carrying interleaved multi-train traffic can legitimately receive more than one distinct event before the next drain. |
| REQ-RBC-004 | `rbc_envelope_queue_push()` shall return `false` (dropping the new entry) rather than growing dynamically when the queue is full (`RBC_ENVELOPE_QUEUE_CAPACITY` unread entries already pending); the caller shall log the drop rather than fail silently. |
| REQ-RBC-005 | Any code path that indexes a per-train array (`ctx->sessions[]`, `train_rx_queue[]`, etc.) by a wire-supplied `train_id` shall validate `1 <= train_id <= SAFEAPI_EXAMPLE_MAX_TRAINS` first and drop (logged) an out-of-range value, never indexing out of bounds with unchecked wire input. |
| REQ-RBC-006 | On a STANDBY-to-ONLINE promotion (`channel_ab_negotiate.c`'s `apply_state_transfer()`), the whole train-session table (`ctx->sessions[]`) shall be overwritten from the transferred snapshot unconditionally - unlike the transferred cycle counter (still gated by `SAFEAPI_EXAMPLE_TRANSFER_POLICY_ENV`), there is no operator-configurable "restart" policy for live train sessions: a promoted site refusing to remember an in-flight Movement Authority would be unsafe, not a preference. |
| REQ-RBC-007 | Cross-compare (`channel_ab_crosscompare.c`) shall vote on the whole per-train session state relevant to the RBC's own decisions (`in_use`/`train_id`/`cycle`/`d_lrbg`/`granted_length`/`ma_seq`/`ma_acked`), not a single scalar value - `ma_pending_send` and the CTC-notification-sent flags are local scratch only and shall be excluded, since they carry no cross-compare-relevant decision content. The compared payload shall be the wire-encoded form, not the raw `train_session_t` struct, since `sapi_cross_comparator_execute()` compares via raw `memcmp()` and the struct's mixed-width members admit compiler-inserted padding a raw comparison would treat as significant. |
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
