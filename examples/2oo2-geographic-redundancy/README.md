# Multi-Site Geographic Redundancy Example

## Overview

This example demonstrates a **realistic ERTMS railway control architecture** with two geographic sites, active/standby failover, and three-channel operation per site.

```
┌──────────────────────────────────────────┐
│         Two ERTMS Radio Block Centres    │
├──────────────────────────────────────────┤
│                                          │
│  WEST (Madrid) - ONLINE/ACTIVE           │
│  ┌─────────────────────────┐             │
│  │ Vital Channels          │             │
│  │  • Channel A (CPU0)     │             │
│  │  • Channel B (CPU1)     │  2oo2       │
│  │  → Vote on commands     │  Voting     │
│  ├─────────────────────────┤             │
│  │ Service Channel (C)     │             │
│  │  • Printf output        │             │
│  │  • Logging, diagnostics │             │
│  └─────────────────────────┘             │
│          ↕ heartbeat (50ms)              │
│  EAST (Barcelona) - STANDBY              │
│  ┌─────────────────────────┐             │
│  │ Vital Channels (mirrored)│            │
│  │  • Channel A (CPU0)     │             │
│  │  • Channel B (CPU1)     │  Synced     │
│  │  → Vote on commands     │  2oo2       │
│  ├─────────────────────────┤             │
│  │ Service Channel (C)     │             │
│  │  • Printf output        │             │
│  │  • Logging, diagnostics │             │
│  └─────────────────────────┘             │
│                                          │
│  On WEST failure:                        │
│  EAST automatically becomes ONLINE       │
└──────────────────────────────────────────┘
```

## Architecture Details

### Per-Site Channel Organization

Each site has **three channels**:

| Channel | Role | Function | Process |
|---------|------|----------|---------|
| **A** | Vital | Primary computation | CPU0 |
| **B** | Vital | Verification/voting | CPU1 |
| **C** | Service | Output/logging | printf |

### Vital Channels (A & B): 2oo2 Voting

```
CPU0 (Channel A)                  CPU1 (Channel B)
    ↓                                  ↓
    Compute train command             Verify train command
    - Position logic                  - Double-check position
    - Signal rules                    - Validate signals
    - Speed calculation               - Cross-check speeds
    ↓                                  ↓
    ↘──────────┬──────────↙
              VOTER
            2oo2 Comparison
              ↓
           MATCH?  ───→ Execute command
              │
              └─→ MISMATCH → Safe state (RED, speed=0)
```

### Service Channel (C): Output & Logging

Channel C is **not safety-critical** but shows:
- Voting results in human-readable format
- Diagnostics (errors, mismatches, timeouts)
- Performance statistics
- Fault detection messages

```c
/* Service output example */
snprintf(service_output, sizeof(service_output),
         "[SITE %s] MATCH: train=%u, pos=%u, sig=%u, speed=%u km/h",
         site_name, train_id, position, signal, speed_limit);

printf("%s\n", service_output);  /* Printed to user/logging system */
```

## Site States & Failover

### Site State Machine

```
         ┌─────────────────────────────────┐
         │   ONLINE (Active Processing)    │
         │  • Performs 2oo2 voting         │
         │  • Produces control commands    │
         │  • Responds to heartbeats       │
         └──────────┬──────────────────────┘
                    │
                    │ heartbeat loss
                    │ (3 missed heartbeats)
                    ↓
         ┌──────────────────────────────────┐
         │  STANDBY (Backup, Synchronized)  │
         │  • Mirrors vital channels        │
         │  • Performs own 2oo2 voting      │
         │  • Ready to take over            │
         └──────────┬───────────────────────┘
                    │
                    │ promoted on failover
                    ↓
         ┌──────────────────────────────────┐
         │  FAULTY (Disabled, in Recovery)  │
         │  • No voting                     │
         │  • No command production         │
         │  • Awaiting recovery             │
         └──────────────────────────────────┘
```

### Failover Scenario

**Cycle 0-24:** Normal operation
```
WEST (ONLINE) → votes → controls trains ✓
EAST (STANDBY) → mirrors, ready
```

**Cycle 25:** WEST loses heartbeat (simulated)
```
WEST heartbeat timer expires
Heartbeat miss counter: 1
EAST detects: "no response from WEST"
```

**Cycles 26-27:** Countdown continues
```
WEST heartbeat miss counter: 2, 3
EAST continues mirroring
```

**Cycle 28:** FAILOVER TRIGGERED
```
WEST → FAULTY (too many missed heartbeats)
EAST → ONLINE (promoted automatically)

[MULTI-SITE FAILOVER] WEST lost heartbeat, switching to EAST
↓
Trains now controlled by EAST RBC
No human intervention required
```

**Cycles 29-50:** EAST operates as primary
```
EAST (ONLINE) → votes → controls trains ✓
WEST (FAULTY) → offline, in recovery
```

## Channel C: Service Output

### Example Output (ONLINE site)

```
┌─ Cycle  7 (WEST active) ────────────────────────────────────┐
│ WEST: [SITE WEST] MATCH: train=101, pos=535, sig=2, speed=80 km/h
│ WEST: [SITE WEST] MATCH: train=102, pos=2556, sig=1, speed=40 km/h
│ WEST: [SITE WEST] MISMATCH: A(train=103,sig=0,spd=0) vs B(train=103,sig=1,spd=0) → SAFE_STATE
└────────────────────────────────────────────────────────────┘
```

### Example Output (Failover)

```
┌─ Cycle 28 (Failover) ───────────────────────────────────────┐

⚠️  INJECTING FAULT: WEST site lost heartbeat (simulating failure)
   Heartbeat miss counter will trigger automatic failover...

[MULTI-SITE FAILOVER] WEST lost heartbeat (150 ms), switching to EAST

└────────────────────────────────────────────────────────────┘

┌─ Cycle 29 (EAST active) ────────────────────────────────────┐
│ EAST: [SITE EAST] MATCH: train=101, pos=645, sig=2, speed=80 km/h
│ EAST: [SITE EAST] MATCH: train=102, pos=2732, sig=1, speed=40 km/h
│ EAST: [SITE EAST] MATCH: train=103, pos=7590, sig=0, speed=0 km/h
└────────────────────────────────────────────────────────────┘
```

## Building & Running

### Compile

```bash
cd examples/2oo2-geographic-redundancy
make
```

### Run

```bash
make run
```

### Expected Output

```
╔═══════════════════════════════════════════════════════════════╗
║  Multi-Site Geographic Redundancy Example                    ║
║  ERTMS Radio Block Centre with Active/Standby Failover       ║
║                                                               ║
║  Architecture:                                                ║
║    WEST (Madrid):     ONLINE  - Vital channels A,B + Svc C   ║
║    EAST (Barcelona):  STANDBY - Mirrors, ready to failover   ║
║                                                               ║
║  Safety: EN 50128 SIL 4 / EN 50129                            ║
╚═══════════════════════════════════════════════════════════════╝

[MULTI-SITE] Initialized: ERTMS-RBC-West (Madrid) (ONLINE) vs ERTMS-RBC-East (Barcelona) (STANDBY)

┌─ Cycle  1 (WEST active) ─────────────────────────────────────┐
│ WEST: [SITE WEST] MATCH: train=101, pos=505, sig=2, speed=80 km/h
│ WEST: [SITE WEST] MATCH: train=102, pos=2508, sig=1, speed=40 km/h
│ WEST: [SITE WEST] MATCH: train=103, pos=7503, sig=0, speed=0 km/h
└────────────────────────────────────────────────────────────┘

... (cycles 2-24: WEST operates normally) ...

⚠️  INJECTING FAULT: WEST site lost heartbeat (simulating failure)
   Heartbeat miss counter will trigger automatic failover...

... (cycles 25-28: heartbeat misses accumulating) ...

[MULTI-SITE FAILOVER] WEST lost heartbeat (150 ms), switching to EAST

┌─ Cycle 29 (EAST active) ────────────────────────────────────┐
│ EAST: [SITE EAST] MATCH: train=101, pos=645, sig=2, speed=80 km/h
│ EAST: [SITE EAST] MATCH: train=102, pos=2732, sig=1, speed=40 km/h
│ EAST: [SITE EAST] MATCH: train=103, pos=7590, sig=0, speed=0 km/h
└────────────────────────────────────────────────────────────┘

... (cycles 30-50: EAST operates as primary) ...

═══════════════════════════════════════════════════════════════
Multi-Site Voting Statistics
═══════════════════════════════════════════════════════════════

WEST Site (FAULTY):
  Total votes:      75
  Matches:          72 (96.0%)
  Mismatches:       3
  Ch-A errors:      0
  Ch-B errors:      0

EAST Site (ONLINE):
  Total votes:      75
  Matches:          72 (96.0%)
  Mismatches:       3
  Ch-A errors:      0
  Ch-B errors:      0

Active Site: EAST
═══════════════════════════════════════════════════════════════
```

## Real-World Integration

### Heartbeat Mechanism

In production, heartbeat is implemented via:

**QNX Message Passing:**
```c
/* WEST sends heartbeat pulse */
MsgSendPulse(east_coid, priority, PULSE_HEARTBEAT, 0);

/* EAST receives and updates timestamp */
if (pulse.code == PULSE_HEARTBEAT) {
    voter.east.heartbeat_timestamp_ms = current_time_ms();
}
```

**TCP/IP Network:**
```c
/* WEST sends heartbeat TCP packet */
send(east_tcp_socket, "PING", 4, 0);

/* EAST receives and responds */
recv(west_tcp_socket, buf, 4, 0);
voter.west.heartbeat_timestamp_ms = current_time_ms();
```

**Shared Memory (Dual SoC):**
```c
/* WEST writes sequence number */
shared_mem->west_heartbeat_seq = ++seq;

/* EAST reads periodically */
if (shared_mem->west_heartbeat_seq == last_seq) {
    west_heartbeat_miss_count++;
}
last_seq = shared_mem->west_heartbeat_seq;
```

### Manual Failover (for Maintenance)

```c
/* Gracefully switch to EAST for maintenance on WEST */
multi_site_failover_request(&voter, SITE_EAST);

/* Service output indicates switchover */
printf("[MULTI-SITE FAILOVER] Manual request: switching from WEST to EAST\n");

/* WEST transitions to STANDBY, EAST becomes ONLINE */
```

## Configuration Parameters

| Parameter | Value | Meaning |
|-----------|-------|---------|
| `site_heartbeat_interval_ms` | 50 | How often each site should respond |
| `heartbeat_timeout_ms` | 150 | How long to wait before declaring site down |
| `channel_timeout_ms` | 25 | Voting deadline for 2oo2 comparison |
| `max_heartbeat_misses` | 3 | Trigger failover after N consecutive misses |
| `max_voting_errors` | 10 | Shutdown channel if this many errors |

## Safety Properties (EN 50128 SIL 4)

| Property | Implementation |
|----------|---|
| **Fail-safe** | Mismatch → safe state; site failure → failover to backup |
| **Deterministic** | O(1) voting, bounded heartbeat check interval |
| **Redundancy** | Two independent geographic sites |
| **Single-point detection** | Either site failure is immediately detected |
| **Data integrity** | CRC32 checksums on all commands |
| **Traceability** | Every vote logged via service channel |

## Key Files

- `redundancy_multi_site.h` — Multi-site voting API
- `redundancy_multi_site.c` — Active/standby failover logic
- `main.c` — ERTMS simulation with 3 trains and fault injection
- `Makefile` — Build configuration
- `README.md` — This document

## Comparison with Single-Site Example

| Feature | Single-Site | Multi-Site |
|---------|---|---|
| Sites | 1 | 2 (WEST + EAST) |
| Failover | N/A | Automatic on heartbeat loss |
| Service output | Yes | Yes (per active site) |
| State machine | N/A | ONLINE/STANDBY/FAULTY |
| Heartbeat | N/A | Yes, 50ms interval |
| Maintenance | N/A | Can switch manually |
| Complexity | Simple | Realistic railway system |

## Next Steps

1. **Run the example** to observe normal operation and automatic failover
2. **Modify heartbeat timing** to test different failure scenarios
3. **Integrate with real QNX/Linux IPC** for actual railway hardware
4. **Add network redundancy** (TCP replication between sites)
5. **Implement persistent logging** to analyze failover events

## References

- EN 50128:2011 § 6.4.2 (Redundant architectures)
- EN 50129:2018 § 5.4 (Site redundancy)
- ERTMS/ETCS Level 3 architecture specification
- Example: `examples/2oo2-geographic-redundancy/`

---

**Architecture Summary:**
- ✓ Two geographic sites (WEST/EAST)
- ✓ Active/standby with automatic failover
- ✓ 2oo2 vital voting per site (channels A, B)
- ✓ Service channel (C) for output/logging
- ✓ Heartbeat-based fault detection
- ✓ EN 50128 SIL 4 compliant
- ✓ Realistic railway control system reference implementation
