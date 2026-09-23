# ADR-039: What N-channel (2oo3+) topologies need from the redundancy primitives

## Status

Proposed (not implemented; this ADR records a scoping investigation, not a decision to build).

## Context

`rte_redundancy_config_t` (`RTE_REDUNDANCY_TOPOLOGY_2OO2`/`_2OO2_REDUNDANT`/`_2OO3`/`_NMR`,
`replica_count`, `quorum_size`) already models arbitrary N-replica topologies at the config level
(RCA/OCORA Phase 8), and `rte_redundancy_config_load()` already parses and validates a 2oo3/NMR
file correctly. But nothing downstream of that config can actually run one: `RBC_GP`'s own
`ab_gp_supports_redundancy()` (`ab_gp_channel.c`) explicitly rejects anything except
`replica_count == 2` with `2OO2`/`2OO2_REDUNDANT`:

```c
bool supported = (replica_count == 2U) &&
                  ((topology == RTE_REDUNDANCY_TOPOLOGY_2OO2) ||
                   (topology == RTE_REDUNDANCY_TOPOLOGY_2OO2_REDUNDANT));
```

TODO.md carried this as "GP's A/B role still registers a fixed pair of links instead of one per
the loaded redundancy topology," phrased as if it were a mechanical fix (loop the existing
open-a-channel code over `replica_count` instead of hardcoding 2). It is not. This ADR records why,
so a future pass starts from the real shape of the problem instead of re-discovering it.

### The two primitives GP's A/B role is built on are structurally pairwise

- **`rte_dual_negotiator_t`** (ADR-020) tracks exactly `own_state`/`peer_state` and
  `own_id`/`peer_id`. Its whole reason to exist is a two-party ONLINE/STANDBY tie-break
  (older-startup-timestamp wins, `own_id`/`peer_id` as a deterministic fallback on exact ties).
  There is no "peer 2," "peer 3" — the type has no notion of more than one counterpart.
  `ab_gp_channel_negotiate.c`'s entire negotiation cycle (`ab_gp_channel_negotiate_execute()`) is
  built directly on this.
- **`rte_cross_comparator_t`** (ADR-025) takes exactly `local_data`/`peer_data` and reports
  AGREED/DISAGREED between the two. `ab_gp_channel_crosscompare_execute()` is built directly on
  this.
- **`ab_gp_com_s`** (`ab_gp_channel_types.h`) has exactly one `peer_channel` and one
  `crosscompare_channel` field (not an array) — the transport layer underneath both of the above
  is pairwise too.

### The N-way primitive that *does* exist is a different one, for a different axis

`rte_voter_t` (ADR-025, split out of the old `rte_vital_channel_t` specifically to separate "N-way
voting on copies of the same computation" from "cross-comparison between two independent
computations") already does real N-way majority voting — including the "largest mutually-agreeing
group, not just compare-to-channel-0" fix ADR-025's own history describes. `rte_channel_checkpoint()`
already uses it. But GP's A/B role never uses `rte_voter` at all; `rte_voter` is not a drop-in
replacement for what `rte_dual_negotiator` does (electing which ONE of N replicas is active) — it
answers "do these N copies of a value agree," not "which one of us should be ONLINE."

So "N-channel topologies" is really two separate, currently-unsolved problems:

1. **N-way election**: which one of 3+ replicas is ONLINE, replacing `rte_dual_negotiator`'s
   pairwise tie-break with something that elects among N candidates (still deterministic,
   still using the same startup-timestamp-then-id tie-break philosophy, but generalized).
2. **N-way cross-compare**: whether N replicas' session tables agree, which `rte_voter` already
   solves in principle but is not wired into GP's crosscompare path, and needs its own decision
   about what "agree" means for the transferred snapshot data GP's crosscompare payload carries
   (`rte_cross_comparator_execute_buffers()`'s byte-buffer API vs `rte_voter`'s channel-handle
   API are different shapes).

Both changes reach into vital, SIL-4-target code that a large amount of already-verified behavior
depends on (`rte_dual_negotiator` alone backs GP's entire promotion/demotion/reboot-lifecycle
policy — see `RBC_GP/CLAUDE.md`'s "Site negotiation" section, decided 2026-09-23, for how much
already sits on top of it).

### The test environment can't exercise N>2 today either

`RBC_Test_Env` is built around exactly two sites (WEST/EAST) each running an A/B pair — six
processes total (A/B x2 sites + role C's train/il/ctc/status). There is no third-replica process,
no scenario config, and no `smoke.sh` check for anything beyond 2oo2. Even a correct N-way
implementation in Platform_RTE and GP would have nothing to verify against without first
extending the test environment — a third piece of scope this ADR's title doesn't mention but a
real implementation would need before it could be called done, not just built.

## Decision (proposed shape, not yet accepted)

Recommend an **additive** approach over a **modifying** one, for the same reason this session's
own cognitive-complexity sweep repeatedly declined to touch high-blast-radius vital functions
(`rte_appmanager_run`, `negotiator_update_states`) alongside smaller changes: `rte_dual_negotiator`
and `rte_cross_comparator` are both mature, tested, and deeply depended upon for the 2oo2 case that
is the only case actually running today. Rewriting either in place to grow an N-way mode risks the
one topology that works.

1. **Leave `rte_dual_negotiator`/`rte_cross_comparator` untouched.** They keep serving the 2oo2
   case exactly as now.
2. **Add new, additive N-way primitives** to Platform_RTE for the 2oo3/NMR case: an N-way election
   module (working name `rte_election`, shape TBD — likely `own_id` plus an array of
   `{peer_id, state, startup_timestamp_ms}` up to `RTE_REDUNDANCY_MAX_REPLICAS`, generalizing the
   existing tie-break) and a decision on whether N-way cross-compare is `rte_voter` used directly
   (with a buffer-based wrapper matching `rte_cross_comparator_execute_buffers()`'s existing
   shape) or its own thing.
3. **GP selects per configured topology**, since it already carries `replica_count`/`topology` on
   `ctx->redundancy` (`rte_redundancy_config_t`, from the Phase 8 work) — 2oo2 keeps using
   today's pairwise path unchanged; 2oo3/NMR uses the new N-way path once one exists.
4. **Extend `RBC_Test_Env`** with a real third-replica scenario before any of this is considered
   verified, not just built.

None of steps 2-4 are scoped further here (state-machine details, wire layout, test topology) —
that is real design work for its own follow-up ADR once someone is ready to commit to building
this, not something to derive from this scoping pass alone.

## Consequences

- If accepted as the direction: this is a multi-session Platform_RTE + RBC_GP + RBC_Test_Env
  effort, not a TODO.md one-liner. TODO.md's own entry should point here instead of restating
  "register one link per replica" as if that were the whole task.
- If rejected in favor of modifying the existing primitives in place: whoever does that owns
  re-verifying every existing 2oo2 consumer of `rte_dual_negotiator`/`rte_cross_comparator`
  against the generalized version, not just the new N-way path.
- Until either path is actually built, `ab_gp_supports_redundancy()`'s current behavior (honest
  `RTE_STATUS_NOT_SUPPORTED` rejection of anything but 2oo2) is correct and should not be
  loosened to silently accept a topology GP cannot actually run.
