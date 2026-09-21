# Common-Cause-Failure (CCF) Analysis — Dual-Channel Vital Core

Date: 2026-08-05
Status: Draft
Applies to: the RBC core's 2-channel ("2oo2") vital computation, as
supported by the `rte_channel` module (ADR-008).

This document exists because EN 50129 requires an explicit CCF analysis
for any redundant/voted safety architecture. It is not sufficient to
state "there are two channels" and assume independence - independence has
to be argued, and its limits stated. This is that argument, scoped to
what Platform_RTE can affect; the parts of a full CCF case that live outside Platform_RTE
(power, environment, physical layout) are named here as required inputs
to the project's overall safety case, not resolved by this document.

## 1. Stated assumption

Both vital channels (A and B) run on **the same CPU architecture (x86)**.
This is a project constraint accepted by ADR-008, not something this
analysis argues against. The question this document answers is: given
that constraint, what mitigates CCF risk, and what residual risk remains?

## 2. Failure model

| Fault class | Same-architecture risk | Mitigated here? |
|---|---|---|
| Compiler-specific codegen bug (e.g. a miscompilation triggered by a specific optimization pass) | High if both channels use the same compiler/flags | **Yes** — build diversity (ADR-008 section 2.1): channel A and B are compiled by independently-configured toolchains (different compiler where available, different optimization flags always). |
| CPU-microarchitecture corner case tied to one specific instruction sequence appearing in the compiled output | Reduced by build diversity (different codegen is less likely to hit the same corner case), but not eliminated — the CPU family itself is unchanged | **Partially** — reduced likelihood, not eliminated. Not claimed as fully mitigated. |
| CPU-silicon-family-wide errata (a defect present in every unit of that x86 family regardless of how the code was compiled) | High — both channels use the same CPU family | **No.** Build diversity does not change the CPU. This is an accepted residual risk under this ADR; true elimination requires hardware architecture diversity (ADR-008 section 2.1's deferred alternative). |
| Specification/algorithm-level bug (the shared source implements the wrong behavior on purpose or by design error) | High — both channels run the same algorithm regardless of compiler | **No.** `rte_channel_compare()` compares two runs of the *same* algorithm; if that algorithm is wrong, both channels agree on the wrong answer and no mismatch is raised. This is a fundamental limit of 2-channel comparison, not specific to build diversity, and is explicitly out of scope for `rte_channel` (ADR-008 section 3). |
| Shared power/environment fault (e.g. a single power rail or a single point of physical damage affecting both channels) | High if channels share power/enclosure/location | **No — outside Platform_RTE's scope.** Requires independent power supplies, independent clocking, and physical separation at the hardware/installation level; Platform_RTE cannot enforce this and it is not claimed to. |
| Single-channel random hardware fault (e.g. a bit flip or component failure specific to one unit) | Low — this is exactly what channel comparison is designed to catch | **Yes** — `rte_channel_compare()` / `rte_channel_compare_and_enter_safestate()` detect any disagreement between the two channels' results and force `RTE_SAFESTATE_LEVEL_SAFE`, regardless of which channel is at fault or why. |

## 3. What "build diversity" is and is not

Channel A and channel B are compiled from **identical source** using two
independently-configured toolchains (`cmake/toolchain-channel-a.cmake`,
`cmake/toolchain-channel-b.cmake`): a different compiler where one is
available (Clang for channel B vs. GCC for channel A), and in all cases a
different optimization/codegen flag profile.

This is diverse **compilation**, not diverse **design or implementation**
(contrast with N-version programming, where independent teams produce
independently-designed implementations of the same requirement). It is
weaker, and it is chosen here specifically because it is affordable
without qualifying a second hardware platform, given the same-x86-
architecture constraint this analysis starts from — not because it is
being presented as equivalent to hardware or design diversity.

**If the second-compiler toolchain (Clang) is unavailable**, channel B
falls back to GCC with a different flag set than channel A. This fallback
is a further-reduced mitigation, called out explicitly in the toolchain
file's own comments and here: two binaries produced by the same compiler
binary, even with different flags, share more failure modes than two
binaries from genuinely different compilers. Any deployment relying on
this fallback should treat it as an interim state, not a final one, and
track qualifying a real second compiler as follow-up work.

## 4. What `rte_channel` does and does not do

Does:
- Gives each channel binary a compile-time-checked, unambiguous identity
  (`rte_channel_local_id()`), enforced by a build failure
  (`#error`) if the channel isn't declared (REQ-COMMON-CHANNEL-001).
- Compares the local channel's result against the peer's, byte-for-byte,
  with a length mismatch itself counting as a disagreement
  (REQ-COMMON-CHANNEL-002).
- On disagreement, forces `RTE_SAFESTATE_LEVEL_SAFE` via the existing
  safe-state mechanism (ADR-004), which never returns to the caller.

Does not:
- Transport the peer channel's result — that's the caller's job, expected
  to use `rte_ipc` or an equivalent channel-to-channel link (ADR-001
  section 4 service 5), designed in a future ADR.
- Vote or select a "correct" result — with two channels there's no
  majority, so the only sound reaction to disagreement is "trust neither,
  go to SAFE." This is fault *detection* with fail-safe reaction, not
  fault *tolerance* with continued operation.
- Detect a shared algorithmic defect (see section 2's table).
- Detect or protect against hardware/environmental CCF outside compute
  (power, clocking, physical separation) — these remain the responsibility
  of the hardware/installation design and the project's broader safety
  case.

## 5. Residual risk statement

After applying build diversity and channel comparison:

- **Reduced but not eliminated**: compiler/codegen-specific systematic
  faults, and CPU-microarchitecture corner cases tied to specific
  generated instruction sequences.
- **Not mitigated by Platform_RTE at all** (must be addressed elsewhere in the
  safety case): CPU-silicon-family-wide errata; specification/algorithm-
  level defects shared by both channels; shared power, clocking, or
  physical/environmental faults.
- **Fully mitigated**: single-channel random hardware faults, and any
  divergence in computed results regardless of cause, are detected and
  forced to a fail-safe reaction.

This residual risk profile should be carried into the project's overall
safety case (EN 50129) as an explicit input, not treated as fully closed
by this document.
