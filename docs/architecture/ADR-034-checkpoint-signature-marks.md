# ADR-034: Checkpoint-signature marks and stage-then-commit output

## Status

Accepted

## Context

Debugging a live `safeAPIRBC2oo2GP` reboot loop (A/WEST and B/WEST cycling
reboots roughly every 40 seconds, never stabilizing) traced back to the
built-in checkpoint integration `rte_appmanager_run()` provides (ADR-019):
`rte_appmanager.c` set `checkpoint_cfg.checkpoint_id = g_app_state.iteration_count`
- a process-local counter that resets to 0 on every reboot. Since A and B
can (and, by the design of a 2oo2 redundant pair, routinely do) reboot
independently of each other, their counters diverge the moment either
side reboots alone - REQ-CHECKPOINT-002 then correctly, but uselessly,
rejects every subsequent reply as a "wrong checkpoint_id" mismatch, not
because of any real fault, but because the two sides' IDs are simply
different values with no relationship to each other. Confirmed live with
a temporary diagnostic: results were bimodal (either zero confirmations
even after 40 retry rounds over a full 10-second budget, or an
instant/near-instant match) - the signature of an ID mismatch, not
network jitter.

Cross-checking `docs/REDUNDANCY_ARCHITECTURE.md`'s own illustrative usage
of `rte_channel_checkpoint()` confirms `checkpoint_id` was originally
conceived as a **barrier label** (the doc's own example always uses a
constant `.checkpoint_id = 1`, naming *which* checkpoint, not a per-cycle
sequence number) - the appmanager's `iteration_count` shortcut ("no
separate per-cycle counter is needed") quietly broke that model for any
integration where the two rendezvousing processes' cycle counts are not
guaranteed to stay in lockstep, which a fault-tolerant redundant pair
fundamentally cannot guarantee.

## Decision

`checkpoint_id` is computed from what each side actually *did* this
cycle, not from how many cycles it has run since its last reboot:

1. **Marks.** `RTE_CHECKPOINT_MARK()` (captures `__FILE__`/`__LINE__`)
   or `RTE_CHECKPOINT_MARK_LABEL(label)`, called by application code at
   meaningful decision/preparation points, folds a CRC64 hash of the
   mark's identity into a single running per-cycle signature:
   `signature = crc64(encode_le(signature) || encode_le(mark_hash))` -
   reusing the framework's existing `rte_checksum_crc64()` (no new
   checksum primitive) and the same explicit-little-endian-byte-pack
   convention `rte_checkpoint.c`'s own `build_arrival_message()` already
   uses, so the fold is well-defined across two potentially different CPU
   architectures.
2. **Reset.** The signature resets to a fixed seed
   (`RTE_APPMANAGER_CHECKPOINT_SIGNATURE_SEED`) at the start of every
   `rte_appmanager_run()` cycle, before `pre_execute()` runs.
3. **Stage reorder.** The checkpoint stage moves from first (before
   `pre_execute()`) to last (after `post_execute()`), so it compares THIS
   cycle's own accumulated signature - marks made during a cycle can only
   be compared once that cycle's own stages have actually run. Internally
   `rte_channel_checkpoint()` itself is completely unchanged (still
   exact-ID-match plus CRC verification, still the same bounded-retry-
   loop/watchdog-kick behavior fixed earlier the same session); only the
   value computed as `checkpoint_id` changes.
4. **Stage-then-commit output.** Moving checkpoint to the end enabled a
   genuine safety improvement: `ops->on_checkpoint_result(context,
   committed)`, a new optional hook called once per cycle right after the
   checkpoint stage. An application can queue (stage) its cycle's output
   instead of transmitting it immediately, and only actually send it once
   `committed == true` (checkpoint confirmed both channels agree) -
   discarding it on `committed == false`. Previously a diverged cycle's
   output could already have been transmitted by the time anything
   checked; now it is never sent at all.
5. **GA exposure.** `safeAPIRBC2oo2GP`'s `ga_interface.h` (the existing
   GP<->GA function-pointer handoff, see that project's own ADR) gained a
   `checkpoint_mark` field so `safeAPIRBC2oo2GA`'s own IL/CTC decision
   functions can fold marks too, via a GA-side macro
   (`AB_GA_CHECKPOINT_MARK(iface)`) that captures GA's own call site.

`safeAPIRBC2oo2GP`'s own staging is scoped narrowly: only the two real
transport functions in `ab_gp_channel.c` are wrapped by staging helpers
(`ab_gp_channel_stage_peer()`/`_stage_relay()` in the new
`ab_gp_channel_stage.c`), and only at the call sites that build THIS
cycle's own decision output (the peer `AB_SAMPLE` and the train/CTC/IL
relay messages in `post_execute.c`). Protocol/liveness traffic that must
keep flowing regardless of this cycle's own checkpoint outcome - the
`CHECKPOINT_REPLY` reply to the peer's own request, IL keepalives, the
`SITE_STATE` echo - is deliberately left unstaged, sent immediately as
before.

REQ-APPMANAGER-008 (the pacing workaround for a fast-failing checkpoint
stage starving `pre_execute()`'s own cycle timing) is superseded, not
replaced: since `pre_execute()` now always runs before checkpoint even
has a chance to fail, it already paces every cycle regardless of that
cycle's checkpoint outcome, so the starvation that workaround guarded
against can no longer occur by construction. `rte_appmanager_pace_failed_checkpoint()`
was removed.

See `docs/requirements/SRS.md` section 3c (REQ-APPMANAGER-007/008/011
through 014) for the normative requirements.

## Consequences

- Checkpoint confirmation no longer depends on two independently-
  rebooting channels having coincidentally identical cycle counts -
  confirmed live: the original ~40s reboot cycle no longer reproduces.
- A cycle's own checkpoint failure is now detected only after that
  cycle's `pre_execute()`/`execute()`/`post_execute()` have already run,
  not before - unavoidable, since the signature being compared is built
  FROM that cycle's own work. Stage-then-commit output is what preserves
  the original safety property ("a desynced peer never acts on this
  cycle's data") despite the reordering: the data itself is withheld,
  even though the stages that built it still ran.
- `checkpoint_id` is now a hash-derived value, not a small sequence
  number - the checksum module's collision behavior (accepted as
  sufficient for accidental divergence detection, not an adversarial
  context) is now load-bearing for checkpoint correctness in a way it was
  not before, when the ID was just an incrementing counter.
- New surface: two optional `rte_appmanager_operations_t` hooks
  (`on_checkpoint_result`), a new public mark API in `rte_appmanager.h`,
  and a new `ga_interface.h` field - all additive and optional; an
  existing integrator that does not adopt marks continues to get a
  cycle-with-zero-marks-every-time signature (a fixed constant, since the
  seed's own two 32-bit halves are equal), which still behaves correctly
  as a "both channels reached the end of this cycle" liveness check, just
  without the finer-grained "same program path" guarantee marks add.
