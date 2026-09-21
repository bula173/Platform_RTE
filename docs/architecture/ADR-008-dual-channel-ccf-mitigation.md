# ADR-008: Dual-Channel Vital Architecture — CCF Mitigation via Build Diversity

Status: Draft
Date: 2026-08-05
Applies to: safeAPIFreamwork, RBC core's dual-channel (2-channel) vital
computation, and the new `rte_channel` module.

## 1. Context

The RBC core is planned to run as two vital channels, A and B, whose
results are compared so a single-channel fault is detected rather than
silently trusted (a 2-out-of-2, "2oo2", pattern). Both channels are
currently planned to run on the **same CPU architecture (x86)**.

That is a real common-cause-failure (CCF) concern, not a detail to leave
implicit. If both channels run identical hardware and identical compiled
code, a systematic fault - a CPU-family-specific errata, a compiler
codegen bug, a timing-dependent race triggered by one specific instruction
sequence - can hit both channels the same way at the same moment, which
defeats the reason a second channel exists. EN 50129 expects an explicit
CCF analysis for any redundant/voted architecture; this ADR states the
same-architecture assumption plainly and defines what RTE does to
mitigate it, rather than letting "two channels" silently imply
independence it doesn't actually have.

Only the SAPI-relevant slice of a full 2oo2 vital computer design is in
scope here: giving channel-level code (1) a query for its own channel
identity, (2) a bounds-checked way to compare its computed result against
the peer channel's result, and (3) a defined fail-safe reaction on
disagreement. The physical transport that carries the peer channel's
result to the local channel (serial link, dual-port RAM, network) is
expected to build on `rte_ipc` (ADR-001 section 4, service 5) and is
**not** designed by this ADR.

## 2. Decision

### 2.1 Primary mitigation: diverse compilation, not diverse hardware

Both channels stay on x86 - that constraint is accepted here, not
re-litigated by this ADR. CCF mitigation is via **build diversity**:
channel A and channel B are compiled from identical RBC_Template/application
source but with two independently-configured toolchains (different
compiler, and/or different optimization level and code-generation flags).

This is weaker than the N-version programming / hardware diversity
techniques described for SIL3/4 architecture (independently *designed*
implementations, or a genuinely different CPU family) - it does not
protect against a specification-level or algorithmic defect, since both
binaries still implement the same algorithm and will compute the same
wrong answer given the same wrong logic. It does reduce the chance that a
single compiler-specific codegen fault, or a CPU-microarchitecture corner
case triggered by one particular instruction sequence, hits both channels
identically. It is the mitigation chosen here because it's affordable
without qualifying a second hardware platform, given the project's
same-architecture constraint - not because it's presented as equivalent to
hardware diversity.

This is not a dead end if stronger mitigation is ever needed: the OAL's
backend-registration design (ADR-001 section 3.5, ADR-005) already means
channel-specific code never calls the OS/CPU directly, so moving channel B
to a different CPU family later is a backend swap, not an application
rewrite.

### 2.2 New feature: `rte_channel` (comparator + identity)

A new, OS-independent module (like `buffer`/`cast`/`safestate`/`string` -
pure data/logic, no backend):

- `rte_channel_id_t` (`RTE_CHANNEL_ID_A` / `RTE_CHANNEL_ID_B`), resolved
  at **compile time** from a build-supplied macro
  (`RTE_CHANNEL_BUILD_A` / `RTE_CHANNEL_BUILD_B`), enforced with a
  preprocessor `#error` if neither or both are defined. A channel binary
  that doesn't unambiguously know which channel it is is itself a defect
  that must be caught at compile time, not left to build-script
  convention.
- `rte_channel_local_id()` - query for the local channel identity.
- `rte_channel_id_to_string()` - diagnostic string, matching the
  `rte_status_to_string()` pattern (ADR-001 section 3.6); logging/
  diagnostics only.
- `rte_channel_compare()` - bounds-checked, byte-for-byte comparison of
  the local channel's computed result (`rte_const_buffer_t`, reusing
  ADR-002's buffer view) against the peer channel's result buffer
  (received via whatever transport - e.g. `rte_ipc`), reporting
  `RTE_CHANNEL_COMPARE_MATCH` / `RTE_CHANNEL_COMPARE_MISMATCH`. A length
  mismatch is itself reported as `MISMATCH`, not treated as an error.
- `rte_channel_compare_and_enter_safestate()` - convenience wrapper:
  compares, and on `MISMATCH` calls
  `rte_safestate_enter(RTE_SAFESTATE_LEVEL_SAFE, reason, ...)` (never
  returns on mismatch, per REQ-COMMON-SAFESTATE-002). A dedicated reserved
  reason code, `RTE_SAFESTATE_REASON_CHANNEL_MISMATCH`, is added to
  `rte_safestate.h`.

This is deliberately a **comparator**, not a voter: with exactly two
channels there is no majority to take, so the only sound reaction to
disagreement is "neither channel is trusted, enter SAFE." A 2oo2
architecture is a fault *detector* with a fail-safe reaction, not a fault
*masker* with continued operation - achieving continued-operation fault
tolerance would need a third channel (2oo3) or channel-level recovery
logic, both out of this ADR's scope.

### 2.3 CCF analysis is documented, not implied

`docs/safety/CCF_ANALYSIS.md` records: the same-architecture assumption;
what diverse compilation catches (compiler/codegen-specific systematic
faults) and explicitly does **not** catch (CPU-silicon-family-wide errata,
shared power/environment faults, and any specification-level bug common
to both channels, since they still share one algorithm); and the
compensating measures expected outside RTE's scope (independent power
supplies, independent clocking, physical separation) that a full safety
case will still need to state.

## 3. Consequences

- Positive: cheap to adopt - no second hardware platform to qualify;
  reuses the existing `rte_safestate` primitive instead of inventing a
  new fault-reaction path; keeps the OAL's future hardware-diversity
  option open without committing to it now.
- Negative / accepted risk: diverse compilation is real but weaker
  diversity than hardware or N-version programming - documented
  explicitly as a residual risk in the CCF analysis rather than presented
  as full protection. A specification/algorithm-level bug present in the
  shared source is **not** caught by this design (both channels compute
  the same wrong answer and agree with each other) - this is a
  fundamental limit of comparing two runs of the same algorithm, not a
  defect in the `rte_channel` implementation.
- Deferred: the actual cross-channel transport carrying the peer's result
  (link protocol, timing budget, EN 50159-style message integrity if
  channels run on separate physical nodes) is out of scope here; expected
  to build on `rte_ipc` in a future ADR once the transport is designed.
  **Update:** ADR-017 (`rte_checkpoint`, `rte_clocksync`) is that future
  ADR - built on `rte_channel` rather than `rte_channel`, since by
  the time ADR-017 was written `rte_channel` had become the more
  complete, actively-developed voting implementation (2oo2/2oo3/NMR vs.
  this ADR's fixed 2-channel comparator). `rte_channel`'s own fate
  (retire vs. keep as a lighter-weight alternative) is still an open
  decision - see ADR-017 section 3.
- Deferred: the build-diversity CMake scaffolding (toolchain files for
  channel A vs. channel B) is provided as a starting skeleton only;
  qualifying a second real compiler toolchain for the actual target
  hardware is project follow-up work this ADR cannot complete in the
  abstract.

## 4. Location

`include/safeapi/channel/rte_channel.h` + `src/channel/rte_channel.c`
(target `safeapi::channel`, links `safeapi::buffer` and
`safeapi::safestate`), per the per-feature layout (ADR-007).
Build-diversity toolchain skeletons at `cmake/toolchain-channel-a.cmake` /
`cmake/toolchain-channel-b.cmake`. CCF analysis at
`docs/safety/CCF_ANALYSIS.md`.
