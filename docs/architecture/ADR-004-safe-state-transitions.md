# ADR-004: Safe-State Transitions and Checked Assertions

Status: Draft
Date: 2026-08-02
Applies to: safeAPIFreamwork, `include/rte/common/rte_safestate.h`,
`include/rte/os/rte_reboot.h`

## 1. Context

CLAUDE.md bans standard library `assert()` in production paths, and CENELEC
EN 50128/50129 requires that a SIL 3/4 application react to a detected fault
by moving to a defined, known-safe condition rather than continuing in an
unknown state or simply crashing. Until now, nothing in the framework
defined what that reaction actually looks like or gave application code a
standard way to say "this invariant just broke."

This ADR defines a small, layer-agnostic **safe-state transition** facility
- `RTE_ASSERT`, `RTE_SAFESTATE`, `RTE_REBOOT` - that every layer (L0
OAL, the future L1, and L2 RBC core) can call into with the same contract.

## 2. Decision

### 2.1 Severity levels, not a single safe state

```c
typedef enum rte_safestate_level_e
{
    RTE_SAFESTATE_LEVEL_DEGRADED = 0,  /* continue, with reduced functionality */
    RTE_SAFESTATE_LEVEL_SAFE     = 1,  /* fail-safe restrictive state; does not return */
    RTE_SAFESTATE_LEVEL_REBOOT   = 2   /* SAFE is insufficient; controlled restart; does not return */
} rte_safestate_level_t;
```

Three levels, not one, because not every detected anomaly warrants the same
response: a redundant sensor mismatch that a voting scheme can absorb is
not the same event as a corrupted invariant inside the RBC core. Only
`DEGRADED` is expected to return control to the caller; `SAFE` and `REBOOT`
are terminal for the calling context (see 2.4).

### 2.2 One handler per level, application-registered

```c
typedef void (*rte_safestate_handler_t)(rte_safestate_level_t level,
                                          rte_safestate_reason_t reason,
                                          const char *file,
                                          int32_t line,
                                          const char *message);

rte_status_t rte_safestate_register_handler(rte_safestate_level_t level,
                                               rte_safestate_handler_t handler);
```

Three static handler slots (one per level), no dynamic allocation,
consistent with every other module in the framework. This is also **how an
integrator plugs in a real implementation**: the framework ships no
built-in reaction beyond a last-resort defensive halt (2.4); the actual
fail-safe action (drive outputs to their safe values, notify a supervisory
channel, trigger a watchdog reset) is supplied by registering a handler for
each level during system initialization.

`rte_safestate_reason_t` is `uint16_t`. Values 0-4095 are reserved for the
framework itself (`rte_safestate_reason.h`-style constants such as
`RTE_SAFESTATE_REASON_ASSERT_FAILED`); application/RBC-specific reason
codes use 4096 and above. This keeps reason codes traceable without forcing
a shared central enum between the framework and every application built on
it.

### 2.3 Why `rte_safestate` does not call `rte_reboot` directly

`RTE_REBOOT` is sugar for entering `RTE_SAFESTATE_LEVEL_REBOOT`. The
*mechanism* that actually restarts the system (a watchdog trigger, a CPU
reset instruction) is inherently OS/hardware-specific and lives in a new
OAL service, `rte_reboot.h` (section 3). `src/common` has no OS dependency
(see ADR-002, section 4) and this ADR does not change that: `rte_safestate.c`
never calls into `src/os`. Instead, the integrator's `REBOOT`-level handler
is expected to call `rte_reboot_request()` itself:

```c
static void my_reboot_handler(rte_safestate_level_t level,
                               rte_safestate_reason_t reason,
                               const char *file, int32_t line,
                               const char *message)
{
    /* safe the outputs, persist a diagnostic record, then: */
    (void)rte_reboot_request(reason);
    /* falls through to rte_safestate's own defensive halt if the backend
       reboot call itself returns instead of resetting the CPU */
}
```

This keeps the dependency direction the same as everywhere else in the
framework (`os` may depend on `common`; `common` never depends on `os`) and
keeps the wiring between "a fault was detected" and "how this specific
target recovers" entirely in integrator-owned code, exactly like every
other OAL backend decision (see the "How will I provide my own
implementation" note added to this ADR's open items, and the forthcoming
backend-selection ADR referenced from ADR-001 section 7).

### 2.4 "Does not return", enforced defensively

C99 has no portable way to guarantee a function never returns. `SAFE` and
`REBOOT` handlers are documented as must-not-return, but `rte_safestate_enter`
does not simply trust that:

1. If a handler is registered for the level, it is called.
2. If the handler returns anyway (a bug, or no handler was registered at
   all), and the level is `SAFE` or `REBOOT`, `rte_safestate_enter` enters
   a defensive infinite loop rather than returning to the caller.

This means the *worst case* for a misbehaving or missing handler is "the
system halts" rather than "the caller continues executing past a
known-unsafe condition" - the correct fail-safe default for a railway
signaling context (absence of movement authority is the safe condition, not
an assumption that everything is fine). `DEGRADED` has no such fallback: a
missing `DEGRADED` handler simply means `rte_safestate_enter` returns
immediately after recording nothing, which is acceptable since `DEGRADED`
is defined as non-terminal.

### 2.5 `RTE_ASSERT` is always active

`RTE_ASSERT(cond)` compiles into every build, including production - it is
not compiled out via an `NDEBUG`-style flag. On failure it calls
`rte_safestate_enter(RTE_SAFESTATE_LEVEL_SAFE, RTE_SAFESTATE_REASON_ASSERT_FAILED,
__FILE__, __LINE__, #cond)`. This is deliberately the same fail-safe path a
manually-written runtime fault check would use - there is no second,
weaker notion of "assertion" in this codebase. This is also *why*
CLAUDE.md's ban on standard `assert()` matters in practice: standard
`assert()` calls `abort()` (or is compiled to nothing under `NDEBUG`, which
is worse - it silently disables the check in release builds, exactly where
SIL 3/4 code needs it most).

### 2.6 Macros

```c
#define RTE_ASSERT(cond) \
    do { if (!(cond)) { rte_safestate_enter(RTE_SAFESTATE_LEVEL_SAFE, \
        RTE_SAFESTATE_REASON_ASSERT_FAILED, __FILE__, (int32_t)__LINE__, #cond); } } while (0)

#define RTE_SAFESTATE(level, reason) \
    rte_safestate_enter((level), (reason), __FILE__, (int32_t)__LINE__, NULL)

#define RTE_REBOOT(reason) \
    rte_safestate_enter(RTE_SAFESTATE_LEVEL_REBOOT, (reason), __FILE__, (int32_t)__LINE__, NULL)
```

Function-like macros here only forward to a real function (`rte_safestate_enter`)
and capture `__FILE__`/`__LINE__` at the call site, which a plain function
call cannot do; they do not duplicate any argument evaluation with side
effects beyond the single `(cond)`/`(level)`/`(reason)` expression each
takes once, consistent with the macro-avoidance rationale in ADR-003
section 2.5.

## 3. New OAL service: `rte_reboot`

```c
rte_status_t rte_reboot_request(uint16_t reason_code);
```

Requests a controlled system restart. Backend-defined mechanism (watchdog
trigger, CPU reset instruction, supervisory processor command). Documented
as not expected to return on success; the skeleton stub returns
`RTE_STATUS_NOT_IMPLEMENTED` like every other OAL stub in this codebase.
This is the 7th OAL service, extending the six defined in ADR-001 section 4.

## 4. Consequences

- Positive: one consistent fault-reaction contract usable from any layer;
  no dynamic allocation; the framework's only built-in behavior (the
  defensive halt) is itself fail-safe even with zero integration effort.
- Positive: `common` stays free of any OS dependency; `os` gains exactly
  one new, small service.
- Negative: because `RTE_ASSERT` is always active, it has a permanent
  runtime cost (a branch) at every call site, and unlike standard `assert`,
  cannot be disabled for a release build. Accepted as correct for SIL 3/4:
  a check that only runs in the environment least like production is not a
  useful safety check.
- Deferred: the OAL backend-selection mechanism referenced in section 2.3
  (how an integrator supplies a real `rte_reboot_request` - and every
  other OAL service - implementation without editing the framework's own
  stub files) is tracked as an open item, same as ADR-001 section 7.

## 5. Location

> **Superseded by ADR-007.** See below for the original paths; the current
> physical layout is `include/rte/safestate/rte_safestate.h` +
> `src/safestate/rte_safestate.c` (target `rte::safestate`) and
> `include/rte/reboot/rte_reboot.h` + `src/reboot/rte_reboot.c`
> (target `rte::reboot`).

`include/rte/common/rte_safestate.h` + `src/common/rte_safestate.c`
(added to `rte_common`, see ADR-002/ADR-003).
`include/rte/os/rte_reboot.h` + `src/os/rte_reboot.c` (added to
`rte_os`, see ADR-001).
