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
sections above were written (`sapi_watchdog`, `sapi_checksum`,
`sapi_vital_channel`, `sapi_appmanager` currently have no REQ-tagged
entries here despite existing in `include/`/`src/` — a pre-existing gap,
not introduced by this section). This section covers the two modules
added by ADR-017.

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
integrator (safeAPIExample's dual-channel A/B link and SITE<->SITE
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

## 3d. `sapi_vital_channel` `channel_count` floor relaxation (ADR-019 addendum)

`sapi_vital_channel` remains one of the modules 3a flagged as not yet
backfilled with `REQ-*` IDs; this note records a behavior change made to
it without introducing a new tag, consistent with that pre-existing gap
rather than adding an isolated one-off ID to an otherwise untagged module.

`sapi_vital_channel_init()` originally required `channel_count >= 2`
(modeling "2+ redundant transport paths to a peer"). ADR-019 §5.1 relaxed
this to `channel_count >= 1`, reachable only via `SAPI_VOTING_NMR` with
`quorum_size == 1`; `SAPI_VOTING_2OO2` and `SAPI_VOTING_2OO3` keep their
existing floors of exactly 2 and exactly 3 channels respectively, so no
existing voting strategy's guarantee is weakened by this change. See
`include/safeapi/vital_channel/sapi_vital_channel.h`'s `@pre channel_count`
doc on `sapi_vital_channel_init()` for the exact, current conditions.

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
