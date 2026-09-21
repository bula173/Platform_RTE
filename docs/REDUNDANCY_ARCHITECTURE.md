# Redundancy architecture

How Platform_RTE supports redundant, voted vital computation: the concepts, what is implemented today, and how the
pieces fit in a 2oo2 deployment. This document merges the earlier `REDUNDANCY_ARCHITECTURE.md` (a design proposal) and
`2OO2_ARCHITECTURE_GUIDE.md` (a generic 2oo2 guide). Both described APIs that were never built (`rte_vital_channel_*`,
`rte_nonvital_*`, `rte_cluster_*`) or used other names for what is now `rte_voter`, `rte_cross_comparator` and
`rte_channel_link`. Their API sketches, the per-CPU hardware walk-throughs and the illustrative timing tables were dropped
because they no longer describe the code; the git history of both files keeps them. Real APIs are in the headers under
`include/safeapi/redundancy/` and authoritative over anything here. Hardware selection guidance is in
[HARDWARE_PATTERNS_GUIDE.md](HARDWARE_PATTERNS_GUIDE.md).

## 1. Terminology

| Term | Meaning here |
|---|---|
| **Vital channel** | A redundant computation whose outputs are compared or voted before they may leave the system |
| **2oo2** | Two channels must agree; any disagreement enters the safe state. Detects a single fault, does not mask it |
| **2oo3 / NMR** | Majority vote among 3 or N channels; masks a minority fault |
| **2oo2 with redundancy** | Two sites, each a 2oo2 pair; one site is ONLINE, the other STANDBY |
| **Hot standby** | The standby computes independently on the same inputs and needs no state transfer |
| **Warm standby** | The standby is partly prepared and receives periodic state; short catch-up on failover |
| **Cold standby** | State is transferred only at promotion |

The older text used "hot standby" for a standby that only replicates state. In the terms above that is warm.

## 2. What is implemented

| Concept | Module | Header |
|---|---|---|
| One redundant link with health tracking | `rte_channel` (channel link) | `redundancy/channel_link/rte_channel.h` |
| N-way voting (2oo2, 2oo3, NMR) across registered links | `rte_voter` | `redundancy/voter/rte_voter.h` |
| 2-way cross-comparison of two peers, channel or buffer form | `rte_cross_comparator` | `redundancy/cross_comparator/rte_cross_comparator.h` |
| Checkpoint rendezvous across channels | `rte_checkpoint` (`rte_channel_checkpoint()`) and the app manager's checkpoint stage | `redundancy/checkpoint/`, `app/appmanager/` |
| Data integrity and the vital message envelope (sequence, sender, CRC-64) | `rte_checksum` | `redundancy/checksum/` |
| Fault detection and recovery actions (log, safe state, reboot, failover, custom) | `rte_watchdog` | `redundancy/watchdog/` |
| Site-level ONLINE/STANDBY negotiation over redundant links | `rte_dual_*` | `redundancy/dual/` |
| State carried across a promotion, declared by the application | `rte_state_transfer` | `redundancy/state_transfer/` |
| Topology, replicas, quorum, `standby_mode`, capability check | `rte_redundancy_config` | `redundancy/config/` |
| Transition to SAFE or REBOOT | `rte_safestate` | `utils/safestate/` |
| Named channels over pluggable backends | `rte_channel_service` | `redundancy/channel_service/` |

Design decisions: ADR-008 (common-cause mitigation), ADR-017 (checkpoint and clock sync), ADR-019 (app manager hooks and
checkpoint), ADR-020 (dual transfer negotiation), ADR-025 (voter and cross-comparator split), ADR-034 (staged sends and
checkpoint signature marks).

**Supported today.** 2oo2 per site, and 2oo2 with redundancy across two sites with cold or warm standby. `rte_voter` votes
N inputs, but registering an N-way mesh of channels between replicas is not implemented, so a real 2oo3 deployment is not
possible yet. Hot standby needs platform-controlled invocation cadence in the app manager and is not implemented.
Which combinations an application accepts is decided by its registered capability callback; the Platform rejects the rest
at load time.

### How a 2oo2 cycle runs in the reference RBC

1. The app manager runs the stages in order. `pre_execute` reads inputs and the peer sample, `execute` cross-compares the
   local and peer results, `post_execute` encodes and sends this cycle's outputs.
2. Sends are staged, not committed. The last stage is the checkpoint rendezvous between the two channels; its result
   commits or discards the staged sends (ADR-034), so no output leaves without both channels having reached the same point.
3. On any disagreement the channel does not send; the Platform enters the safe state. In the reference RBC a genuine
   decision disagreement escalates to REBOOT and a live ONLINE/STANDBY failover between sites.

See `application/RBC_GP/docs/REFERENCE_DESIGN.md` and `docs/architecture/ADR-034-checkpoint-signature-marks.md`.

## 3. Concepts

### 3.1 Voting strategies and safety properties

| Strategy | Rule | Single-fault detection | Fault tolerance | On failure | Typical use |
|---|---|---|---|---|---|
| **2oo2** | Both agree, else fault | Yes | 0 (no masking) | Safe state | Dual channel, dual site pair |
| **2oo3** | At least two agree | Yes | 1 channel | Isolate the minority | Triple channel |
| **NMR** | Majority of N | Yes | up to (N-1)/2 | Isolate minority | Four or more replicas |
| **Non-vital, single** | No voting | No | None | Application decides | Logging, diagnostics |

2oo2 is simple, deterministic and biased to the safe side, and it is a good fit for binary decisions such as granting or
withholding an authority. Its costs: no masking (a fault means a safe state, so availability suffers), the failed channel must
be identified separately, hardware is doubled, and the channels must agree on timing. Use 2oo3 when availability matters more
than the cost of a third channel, or add site redundancy to a 2oo2 pair to recover availability.

### 3.2 Coordination patterns between channels

| Pattern | When it fits | Note |
|---|---|---|
| Shared memory, cache-coherent | Two cores on one die | Lowest jitter; a shared fault domain, so weaker independence |
| Message passing, symmetric | Separate processes or hosts | What the reference RBC uses (`ab-peer`); the transport is a backend |
| Asymmetric, one coordinator | A local channel plus a remote one | The coordinator becomes a single point to argue about |

Whichever is used, the comparison must be over data that both channels derived from the same input at the same logical
point. That is what the checkpoint provides.

### 3.3 Fault modes and reactions

| Mode | Condition | Reaction in Platform_RTE |
|---|---|---|
| Match | Results equal | Continue; outputs are committed at the checkpoint |
| Mismatch | Results differ although both are valid | Safe state; in the reference RBC a REBOOT with `own_faulted` flushed to the counterpart site |
| Timeout | A peer sends nothing for the configured time | Reference RBC: an ONLINE channel drops to single mode and keeps deciding; a checkpoint timeout is treated as a fault; the checkpoint is paused while the link is known down |
| Corruption | CRC-64 mismatch on a peer sample | Safe state (halt); this is a transmission fault, not a logical disagreement |

Common-cause failure limits all of this: identical CPUs and identical builds can fail together. Mitigation and its limits
are in ADR-008 and [safety/CCF_ANALYSIS.md](safety/CCF_ANALYSIS.md).

## 4. Checkpoint synchronization and pre-commit

Before channels compare data they must be at the same logical point, and nothing may leave until that is established.

1. Each channel processes its input independently to a defined checkpoint and stages its candidate output.
2. Channels exchange checkpoint status within a bounded time. A channel that does not reach the checkpoint in time is
   treated as faulty (safe state).
3. Data are exchanged and compared (voting or cross-comparison). A failed comparison aborts the output.
4. Only after checkpoint, comparison and agreement is the staged output committed and sent. Any failure at any stage
   discards it, so no output escapes without consensus.

Properties this gives: temporal consistency (same point in time), logical consistency (same input state), no contradictory
outputs, fail-safe behaviour at every stage, and an audit trail (checkpoint reached, timing, comparison result, commit).
The checkpoint is correct without a shared wall clock (ADR-017); clock synchronization is diagnostic only. In the reference
RBC the checkpoint budget is shorter than a cycle, so a "lookback" rule also accepts a peer that exchanged a checkpoint frame
within the last few cycles, while a silent peer still trips the safe state (see `application/RBC_GP/CLAUDE.md`).

## 5. Site redundancy: online and standby

| Model | Description | In Platform_RTE |
|---|---|---|
| **Online (active-active)** | All nodes process the same inputs and must agree | Within a site this is the 2oo2 pair. Across sites it would be an active-active design, which is not implemented |
| **Standby** | One site is ONLINE and outputs; the other is ready to take over | Implemented as cold and warm. Roles are ONLINE, STANDBY and FAULTED, negotiated with `rte_dual_*` |

Behaviour of the reference implementation:

- **Negotiation.** Each channel negotiates with its counterpart on the other site; the older start time wins the initial
  ONLINE role. A FAULTED channel never promotes itself; a STANDBY channel whose counterpart is FAULTED promotes itself;
  a FAULTED channel whose counterpart is ONLINE recovers to STANDBY; two FAULTED channels re-negotiate.
- **Promotion.** The application declares the state that must survive (`rte_state_transfer`). The reference RBC transfers
  sessions and the route/train database unconditionally on promotion.
- **Standby computes or not.** Both sites run their full A/B group. In the reference RBC the standby observes but does not
  decide. Real hot standby (independent computation on the same inputs) needs platform-controlled invocation cadence.
- **Who decides.** The Platform owns voting, negotiation and safe-state transitions; the application declares what it supports
  and what state to carry. `standby_mode` (hot, warm, cold) is configuration, not application code.

## 6. Best practices

1. Protect every inter-channel and inter-site message with the vital message envelope (sequence, sender, CRC-64).
2. Bound every wait: checkpoint, peer receive, negotiation. An unbounded wait is a hazard.
3. Treat channels symmetrically; do not give one channel a preference except for the documented tie-breaks.
4. Log every mismatch, timeout and corruption with cycle number and channel identity, and investigate.
5. Test each fault mode by injection (mismatch, timeout, corruption, peer loss, both sites faulting).
6. Track statistics; a healthy system agrees on almost every cycle.
7. Argue independence explicitly (compiler diversity, separation, independent verification) rather than assuming it.

## 7. EN 50128 / EN 50129 traceability

| Requirement area | Implementation |
|---|---|
| Fail-safe architecture | 2oo2 enters the safe state on disagreement |
| Periodic and continuous self-checking | CRC-64 on inter-channel data, watchdogs on links |
| Deterministic timing | Bounded comparison and bounded waits; staged commit |
| Fault tolerance and detection | Two channels, single-fault detection, site failover |
| Traceability | Requirement IDs in code; SRS in `docs/requirements/SRS.md` |
| Common-cause failure | ADR-008 and `safety/CCF_ANALYSIS.md` |

Standards: EN 50128:2011 (fail-safe architecture, redundancy patterns), EN 50129:2018 (redundancy and 2oo2 voting),
CENELEC TR 50128.
