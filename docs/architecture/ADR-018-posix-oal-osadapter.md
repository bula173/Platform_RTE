# ADR-018: A Real POSIX/Linux OAL Backend

> **Terminology update (2026-09):** "backend" is now called **OSAdapter**. `rte_<service>_backend_t` is `rte_osadapter_<service>_t`,
> `rte_<service>_register_backend()` is `rte_osadapter_<service>_register()`, headers moved from `rte_backend/` to
> `rte_osadapter/`, and the POSIX implementation is `rte_posix_osadapter_*` (`Platform_OS_POSIX`). The text below keeps the
> original wording as a historical record.

Status: Superseded (relocated) - Accepted
Date: 2026-08-05
Applies to: the POSIX backend's design, and how it plugs into every
existing OAL service's backend-registration mechanism (ADR-005).

**Relocation note:** the code this ADR describes (`src/posix_osadapter/`,
`include/rte/posix_osadapter/`, `tests/posix_osadapter/`) has been moved
out of this repository into the `safeAPIRBC2oo2` project
(`src/posix_osadapter/` there). This is a location change only, not a
design reversal - the rationale below is unchanged and still describes
that code accurately. The move itself is a direct consequence of this
ADR's own reasoning (ADR-005): an OAL backend is an integrator-supplied
implementation for a specific target, not part of the reusable safety
framework itself. safeAPIFreamwork now ships only the OAL service
interfaces and their validate-then-dispatch layers (`rte_<service>_*()`
+ `rte_<service>_register_backend()`); any concrete backend - POSIX,
an RTOS, bare metal - belongs in the application/integration project
that registers it, which is what `safeAPIRBC2oo2` now demonstrates.

## 1. Context

Every OAL service (`rte_timer`, `rte_ipc`, `rte_task`, `rte_log`,
`rte_nvm`, `rte_reboot`, `rte_memory`) is a pure dispatch layer over a
backend an integrator registers at startup (ADR-005) - none of them do
anything on real hardware by themselves. A survey done for this ADR found
that, despite the framework's maturity, **no real backend implementation
existed anywhere in the repository** for any target: `examples/linux-posix-app/`
looks like a working Linux app but never calls a single
`rte_*_register_backend()`; its timer path is explicitly bypassed with a
comment saying so. Outside of test mocks, the only real backend
registration call in the whole tree was a diagnostic clocksync stub
returning a hardcoded constant. `examples/2oo2-geographic-redundancy/`
and `examples/2oo2-cross-comparison/` - the two examples that look like
they demonstrate 2oo2/online-standby clustering - define their own
standalone types and contain zero `rte_*` calls; the cross-comparison
example's own integration doc says outright that real RTE integration
is still a "next step."

This ADR is that missing piece: a real, working POSIX backend, so a
Linux application can actually run on RTE instead of just linking
against its headers.

## 2. Decision

### 2.1 One module, one file per service, one aggregator

`src/posix_osadapter/rte_posix_osadapter_<service>.c` for each of timer,
ipc, task, log, nvm, reboot, memory, plus
`src/posix_osadapter/rte_posix_osadapter.c` exposing a single
`rte_status_t rte_posix_osadapter_register_all(void)` that calls every
service's `rte_<service>_register_backend()` with this module's
implementation - one call at startup instead of seven, matching the
"single entry point" pattern `rte_appmanager` already established
elsewhere in this codebase. Individual `rte_posix_osadapter_<service>()`
accessor functions are also exposed for callers who want to register
only some services with the real backend and mock/stub the rest (e.g. in
tests).

This is a new top-level feature directory, not nested under any existing
service, because it depends on *all* of them and none of them should
depend on it (ADR-007's per-feature layout is about independent
buildable units; a backend implementation legitimately depends on the
API it implements, so the dependency direction here is
`posix_osadapter -> {timer, ipc, task, log, nvm, reboot, memory}`, never
the reverse).

### 2.2 Fitting inside each service's fixed opaque storage

Every service's public header reserves a small fixed-size byte buffer
for backend state (`RTE_DECLARE_STORAGE`, ADR-001 section 3.4) -
64 bytes for timer/ipc/nvm/memory, 128 for task. This is the main design
constraint on every backend below, and it directly ruled out the
"obvious" POSIX implementation for `rte_ipc`: a `pthread_mutex_t` +
`pthread_cond_t` protecting an in-process ring buffer would already
consume ~88 bytes on glibc x86_64 before storing anything else, blowing
the 64-byte budget. `rte_ipc`'s backend uses a `pipe()` instead - two
file descriptors plus bookkeeping fits in under 32 bytes and sidesteps
the whole synchronization-primitive-size problem by letting the kernel
own the queue.

### 2.3 What "real" means here, and what it deliberately doesn't

- **Timer**: one POSIX thread per timer, sleeping via
  `clock_nanosleep(CLOCK_MONOTONIC, ...)` and invoking the callback
  directly from that thread (documented per REQ-OAL-TIMER-003's
  "backend-defined context" - this backend's context is "its own
  thread," not the caller's thread or a signal handler). A thread per
  timer is not the leanest possible implementation (a single timer
  wheel thread would scale better), but it is far simpler to get
  correct, and correctness matters more than scale for a first real
  backend.
- **IPC**: a `pipe()` per channel, `O_NONBLOCK` + `poll()` to honor
  `timeout_ms` without busy-waiting, looped read/write to handle partial
  transfers. Known simplification, stated plainly: a message larger than
  the pipe's internal buffer that partially transfers before a timeout
  expires leaves a partial message in the pipe for the next
  read - acceptable for this first cut, not something a real deployment
  should rely on without hardening.
- **Task**: one `pthread_t` per task; `period_ms > 0` runs the entry
  point in a loop with a `clock_nanosleep`-paced period, `period_ms == 0`
  runs it once. Priority is applied via `pthread_setschedparam()` with
  `SCHED_FIFO` when the process has permission to do so (typically
  requires `CAP_SYS_NICE` or root); falls back to the default scheduling
  policy with a logged (non-fatal) status otherwise, rather than failing
  task creation outright - most development/CI environments cannot grant
  real-time scheduling privilege, and refusing to create the task at all
  would make this backend unusable there.
- **Log**: non-blocking write to `stderr`, matching REQ-OAL-LOG-001
  ("best-effort, non-blocking, must never affect caller control flow") -
  no queue, no separate thread; `stderr` writes are typically fast enough
  not to violate this in practice for a first cut, but a real deployment
  wanting a hard non-blocking guarantee should route through a lock-free
  ring buffer drained by a dedicated low-priority thread instead.
- **Reboot**: `execv()` re-exec of the current binary (`/proc/self/exe`)
  with its original `argv`, which is the closest a normal (non-root,
  non-embedded) Linux process can get to "the CPU resets" without actual
  hardware reset capability or an external supervisor process. This is
  clearly documented as a stand-in, not a real safety-relevant reboot
  path - a real deployment on real hardware needs a real reset mechanism
  (watchdog-triggered hardware reset, supervisory process, etc.).
- **NVM**: a plain file per region, with a whole-region FNV-1a-64 hash
  trailer recomputed on every write and verified on every read
  (REQ-OAL-NVM-001). Deliberately does **not** reuse `rte_checksum`'s
  CRC-64 - that module's lookup tables are known-incomplete placeholders
  (flagged in ADR-017/the MISRA report); building this backend's
  integrity check on top of a hash known to be broken would just move
  the problem, not solve it. FNV-1a is simple enough to implement
  correctly inline, with no lookup table to get wrong.
- **Memory**: a single fixed-size static byte arena
  (`RTE_POSIX_MEM_ARENA_SIZE`, default 1 MiB, compile-time constant) with
  a per-pool intrusive free list carved out of it at `rte_mem_pool_create()`
  time. This is static partitioning, not `malloc`/`free` - the arena's
  total size is fixed at compile time and every pool's claim against it
  is checked against remaining arena space, returning
  `RTE_STATUS_RESOURCE_EXHAUSTED` rather than growing anything.

### 2.4 Build integration

`src/posix_osadapter/CMakeLists.txt` only builds on POSIX-ish platforms
(`if(UNIX)` - covers Linux and other POSIX systems this backend also
happens to work on, e.g. any pthread/POSIX.1-2008-conformant target) and
links `pthread` plus `rt` where required by the target libc for
`clock_nanosleep`/`mqueue`-adjacent symbols. Added to the top-level
feature loop guarded the same way, so a non-POSIX (e.g. QNX,
bare-metal) configuration simply doesn't build this directory rather than
failing to configure.

## 3. Consequences

- Positive: `examples/linux-posix-app/` finally has a real backend to
  register instead of bypassing the timer path; a genuine 2oo2 or
  online/standby Linux application can now be built on top of actual
  running RTE services instead of a parallel non-SAPI demo.
- Negative / scope limit: this is a first real backend, not a
  SIL-qualified one. Explicitly not addressed here: real-time scheduling
  guarantees without elevated privilege, WCET analysis of any operation,
  a real hardware reset path, and NVM integrity beyond a simple hash
  (see 2.3's per-service notes for the specific gap in each case).
- Negative / accepted risk: one-pthread-per-timer and one-pthread-per-task
  do not scale to a system with hundreds of timers/tasks - acceptable for
  a first working backend and typical application-level channel/task
  counts, flagged here so it isn't mistaken for a scalability-tested
  design.
- Deferred: `examples/linux-posix-app/main.c` needs to be rewritten
  against this real backend (it currently only compiles against the API,
  it doesn't exercise it) - tracked as follow-up in this same change.

## 4. Location

`src/posix_osadapter/rte_posix_osadapter_{timer,ipc,task,log,nvm,reboot,memory}.c`
+ `rte_posix_osadapter.c` (aggregator) + `include/rte/posix_osadapter/rte_posix_osadapter.h`,
target `rte::posix_osadapter`, POSIX-only (`if(UNIX)`), links every OAL
service target plus `pthread`.
