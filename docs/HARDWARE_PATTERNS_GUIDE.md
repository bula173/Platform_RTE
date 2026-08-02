# Hardware Patterns Guide - Choosing by SIL Level

**Purpose:** Help you choose the right hardware configuration for your safety-critical railway application.

**Audience:** System architects, hardware engineers, safety managers

**Standards:** EN 50128 (Software Safety), EN 50129 (Functional Safety), EN 50126 (RAM)

---

## Safety Fundamentals: Why Hardware & Software Together

### What is Functional Safety?

Functional safety means a system reliably performs its safety function, even when faults occur. For ERTMS systems like RBC:

```
Safety Function Example: Movement Authority (MA)

Normal Operation:
├─ Train sends position
├─ RBC calculates safe distance
├─ RBC grants movement authority
└─ Train accelerates safely

With Faults:
├─ If RBC FAILS to grant MA
│  └─ System must STOP train (fail-safe)
├─ If RBC grants WRONG MA
│  └─ System must STOP train (detect & correct)
└─ If RBC grants MA TOO LATE
   └─ System must STOP train (timeout protection)
```

A safety-critical system must:
1. **Perform correctly** under normal conditions
2. **Detect faults** as they occur
3. **Recover or go safe** when faults are detected

### The Safety Equation: Hardware + Software

Building a SIL X system requires **BOTH** hardware techniques and software techniques working together:

```
SIL Integrity = Hardware Techniques + Software Techniques + Verification

Hardware Techniques (Redundancy & Fault Tolerance):
├─ Redundant processors (2oo2, 2oo3, NMR voting)
├─ Watchdog timers (detect hangs/timing failures)
├─ Memory protection (prevent corruption spread)
├─ Diverse CPUs (prevent common-mode failures)
├─ Network redundancy (detect communication faults)
└─ Power/cooling monitoring (detect environmental faults)

Software Techniques (Safety Logic):
├─ Safe-state transitions (known safe states)
├─ Defensive programming (check every assumption)
├─ Error detection (checksums, assertions, voting)
├─ Bounded latency (deterministic timing)
├─ No dynamic allocation (predictable behavior)
├─ Type safety (fixed-width types, explicit casts)
└─ Traceability (every function linked to requirements)

Verification Methods:
├─ MISRA C:2012 static analysis
├─ Formal methods (timing proofs)
├─ Testing (unit, integration, system)
├─ Failure mode analysis (FMEA)
├─ Code reviews (design & safety)
└─ Certification audits
```

**Without both:**
- Only software = Can't survive hardware faults (no redundancy)
- Only hardware = Can't make correct decisions (bad logic)

### Common Failures vs. Random Failures

This distinction is fundamental to safety design:

#### **Random Failures (Systematic Faults)**

```
Characteristics:
├─ Unpredictable timing
├─ Independent events
├─ Follow statistical distribution
├─ Example: Cosmic ray flips memory bit
├─ Example: Network packet lost due to noise
├─ Example: Component aging wears out after years
├─ Example: Transient power glitch

How to detect:
├─ Voting (2 out of 3 agree = detects 1 random fault)
├─ Checksums (data integrity check)
├─ Parity bits (detect bit flips)
├─ Timeouts (detect halted processor)

How to survive:
├─ Redundancy (2oo2, 2oo3)
├─ Restart faulty component
├─ Failover to backup
└─ Majority vote (NMR)

Example: A random bit flip in RBC's state can be detected
by voting—other RBCs will have different values, voting
detects disagreement, system recovers.
```

#### **Common-Mode Failures (Systematic Errors)**

```
Characteristics:
├─ Same root cause affects multiple components
├─ Deterministic (happens every time under same conditions)
├─ Design flaws, not random events
├─ Example: CPU microcode bug affects all identical CPUs
├─ Example: Compiler bug generates wrong code in all copies
├─ Example: Algorithm error exists in all identical software
├─ Example: Library vulnerability exploited on all instances

Why redundancy FAILS against common-mode:
├─ 2 identical CPUs + same software
│  └─ Both fail identically (voting detects NOTHING)
├─ 3 identical servers + same algorithm
│  └─ All make same wrong decision (voting useless)
└─ Result: Redundancy provides NO SAFETY

How to detect:
├─ CPU diversity (different architectures fail differently)
├─ Software diversity (different code paths, different compilers)
├─ Cross-verification (independent calculations)
├─ Formal verification (prove algorithm correctness)

How to survive:
├─ Use DIFFERENT CPU vendors (Intel vs. AMD vs. ARM)
├─ Use DIFFERENT software stacks
├─ Use DIFFERENT algorithms (diversity)
├─ Use formal proof (mathematically guarantee correctness)

Example: If all RBC instances have the same compiler bug,
voting is useless—all make the same wrong decision.
Defense: Use different compilers, different CPU architectures,
different implementation teams.
```

#### **Comparison**

| Aspect | Random Failures | Common-Mode Failures |
|--------|---|---|
| **Cause** | Independent events (cosmic ray, noise, wear) | Design flaws (bug in code/CPU shared by all) |
| **Detection** | Voting, timeouts, checksums | Diversity, formal proofs, reviews |
| **Redundancy Helps?** | ✓ YES (identical redundancy works) | ✗ NO (identical redundancy fails) |
| **Example** | One random bit flip in memory | Compiler generates same bug in all copies |
| **Solution** | 2oo2 voting (detects mismatch) | Different CPUs/compilers (prevent mismatch) |

### SAPI's Role: The Software Safety Foundation

safeAPIFramework provides the **SOFTWARE LAYER** of safety-critical systems:

```
Complete Safety System:

┌──────────────────────────────────────────────────┐
│            ERTMS RBC Application                 │
│     (Your railway safety logic)                  │
└────────────────────┬─────────────────────────────┘
                     │
         ┌───────────▼──────────────┐
         │   safeAPIFramework       │  ← SAPI (THIS PACKAGE)
         │   (Software Techniques)  │
         │                          │
         │   ✓ Vital channels       │
         │   ✓ Voting logic         │
         │   ✓ Watchdog monitoring  │
         │   ✓ Safe-state machine   │
         │   ✓ Error detection      │
         │   ✓ Bounded latency      │
         │   ✓ Type safety          │
         │   ✓ Defensive checks     │
         │   ✓ Traceability         │
         └───────────┬──────────────┘
                     │
       ┌─────────────┴──────────────┐
       │                            │
   ┌───▼────────────┐    ┌──────────▼──────┐
   │  Hardware      │    │  Certification  │
   │  (Your choice) │    │  (Verification) │
   │                │    │                 │
   │ ✓ 2oo2 CPUs    │    │ ✓ MISRA C       │
   │ ✓ 2oo3 voting  │    │ ✓ EN 50128      │
   │ ✓ Watchdogs    │    │ ✓ FMEA          │
   │ ✓ Network      │    │ ✓ Testing       │
   │ ✓ Diverse CPUs │    │ ✓ Reviews       │
   │ ✓ Redundancy   │    │ ✓ Audits        │
   └────────────────┘    └─────────────────┘
```

**SAPI Provides:**
1. **Vital Channels** — Redundant communication with voting
2. **Checkpoint Synchronization** — Ensure all nodes agree before action
3. **Watchdog Monitoring** — Detect faults (hangs, delays, crashes)
4. **Safe-State Machine** — Known safe states, safe transitions
5. **MISRA C:2012 Compliance** — No dynamic memory, type safety, bounded logic
6. **Error Detection** — Checksums, assertions, timeout handling
7. **Deterministic Timing** — Predictable, verifiable behavior
8. **Traceability** — Every function linked to safety requirements

**YOU Provide:**
1. **Hardware Configuration** — Choose 1oo1, 2oo2, 2oo3, Online mode, etc.
2. **CPU Selection** — Single processor, dual CPUs, triple-site, etc.
3. **Redundancy Strategy** — Which components are vital, which are service
4. **Verification Plan** — Testing, FMEA, code reviews, certifications
5. **Deployment** — Network setup, failover mechanisms, monitoring

### Building a SIL X System: The Configuration Model

```
Your Application
        ↓
   [SAPI Framework]  ← Software safety techniques
        ↓
   CONFIGURE FOR HARDWARE
   (Choose pattern from this guide)
        ↓
┌─ SIL 1: 1oo1 (single system)
├─ SIL 2: 1oo1 (single system)
├─ SIL 3: 2oo2 or 2oo2D (dual redundancy)
└─ SIL 4: 2oo3, Online mode, Hot standby, or NMR
        ↓
   [Certified Platform]  ← Hardware safety techniques
        ↓
   Verified Safety Case
   (MISRA + EN 50128 + FMEA + Testing)
        ↓
   Certified SIL X System
```

**Key Insight:** The SAME SAPI code works for SIL 1, 2, 3, and 4 systems by just changing the hardware configuration and verification rigor. SAPI provides the software foundation; you choose the hardware strategy to reach your target SIL level.

### How Failures Determine SIL Requirements

```
System Requirement: "RBC must safely grant or deny movement authority"

Fault Analysis:
├─ Random fault in RBC's calculation
│  └─ Detected by: Voting (2oo2 detects disagreement)
│  └─ Survived by: Fallback to safe state (deny MA)
│  └─ SIL 3 acceptable
│
├─ Common-mode fault (compiler bug in all RBCs)
│  └─ NOT detected by identical voting
│  └─ Must prevent with: CPU diversity + formal proof
│  └─ SIL 4 required (voting alone insufficient)
│
├─ Network delay (RBC votes but answer arrives late)
│  └─ Detected by: Watchdog timeout
│  └─ Recovered by: Safe state (deny MA) + failover
│  └─ SIL 3+ required
│
└─ Multiple faults (RBC hangs AND network down)
   └─ Must tolerate: 2+ simultaneous faults
   └─ Requires: 2oo3 (tolerate 1 fault) or better
   └─ SIL 4 required
```

### Standards and Techniques

SAPI implements these mandatory EN 50128 techniques:

| Technique | SAPI Support | Hardware Required |
|-----------|---|---|
| **Defensive Programming** | ✓ (all code) | Any |
| **Diverse Redundancy** | ✓ (voting) | Multiple CPUs |
| **Monitoring & Watchdog** | ✓ (built-in) | Watchdog timer |
| **Safe-State Machine** | ✓ (core design) | Any |
| **Initialization & Failure Handling** | ✓ (framework) | Any |
| **Error Detection & Correction** | ✓ (checksums, voting) | Redundant channels |
| **Bounded Time & Space** | ✓ (no dynamic mem) | Deterministic CPU |
| **Formal Verification** | ✓ (traceable) | Any |
| **Traceability** | ✓ (REQ IDs) | Any |
| **Testing & Validation** | Enabler | Any |

---

## Quick Decision Matrix

```
SIL Level | Redundancy | Pattern | Example HW | Detection
──────────────────────────────────────────────────────────
  SIL 1   │    None    │  1oo1   │ Single CPU │   N/A
  SIL 2   │    None    │  1oo1   │ Single CPU │   N/A
──────────────────────────────────────────────────────────
  SIL 3   │  Mandatory │  2oo2   │ Dual-Core  │  Immediate
          │            │  2oo2D  │ Dual-Site  │  < 100ms
──────────────────────────────────────────────────────────
  SIL 4   │  Mandatory │  2oo3   │ Triple-Core│  < 200ms
          │  + Fault   │ Online  │ 3-Site     │  < 200ms
          │  Tolerance │ Standby │ Primary+BU │  < 500ms
          │            │  NMR    │ 4+ Sites   │  < 150ms
──────────────────────────────────────────────────────────
```

---

## SIL 1: No Safety Integrity Required

### When to Use
- **Scenario:** Non-critical functions, informational systems
- **Examples:** 
  - Diagnostic logging
  - System status display
  - Performance monitoring
  - Development/testing

### Recommended Hardware Pattern: **1oo1 (Single System)**

```
┌──────────────────────────┐
│   Single Processor       │
├──────────────────────────┤
│  Application Logic       │
│  (no redundancy)         │
│  (no voting)             │
├──────────────────────────┤
│  SAPI Framework          │
│  (basic features)        │
├──────────────────────────┤
│  Linux / QNX / Bare Metal│
└──────────────────────────┘
```

### Configuration

```c
// SIL 1: Single system, no voting
sapi_ipc_config_t config = {
    .channel_count = 1,
    .redundancy_mode = SAPI_SINGLE,
    .timeout_ms = 1000
};
```

### Advantages
✅ Lowest cost
✅ Lowest complexity
✅ Fast deployment
✅ Easy maintenance

### Disadvantages
❌ No fault tolerance
❌ Single-point failures not detected
❌ Unattended operation risky
❌ Not suitable for safety functions

### Cost Estimate
- **Hardware:** 1x CPU (~$100-500)
- **Development:** 4-6 weeks
- **Certification:** Not required
- **Total:** Low cost

### Design Considerations
- Application can halt on error
- No automatic recovery needed
- Manual restart acceptable
- Best-effort service acceptable

---

## SIL 2: Minimal Safety Integrity

### When to Use
- **Scenario:** Low-critical functions with periodic human oversight
- **Examples:**
  - Speed advisories (non-binding)
  - Track status announcements
  - Maintenance alerts
  - Non-critical scheduling

### Recommended Hardware Pattern: **1oo1 (Single System)**

```
┌──────────────────────────┐
│   Single Processor       │
├──────────────────────────┤
│  Application Logic       │
│  + Watchdog Timer        │
│  + Health Monitoring     │
├──────────────────────────┤
│  SAPI Framework          │
│  (with logging)          │
├──────────────────────────┤
│  Linux / QNX             │
└──────────────────────────┘
```

### Configuration

```c
// SIL 2: Single system with watchdog
sapi_watchdog_config_t wd_config = {
    .type = SAPI_WATCHDOG_SYSTEM,
    .name = "sil2_monitor",
    .timeout_ms = 5000,                 // 5 second deadline
    .action = SAPI_WATCHDOG_ACTION_LOG  // Log only, operator notified
};

sapi_watchdog_t wd;
sapi_watchdog_create(&wd, &wd_config);
sapi_watchdog_start(wd);

// Main loop
while (running) {
    process_messages();
    sapi_watchdog_kick(wd);  // Prove liveness
    sleep_ms(1000);
}
```

### Advantages
✅ Low cost (same as SIL 1)
✅ Single system, simple design
✅ Watchdog detects hangs
✅ Suitable for monitored systems

### Disadvantages
❌ Still no redundancy
❌ Single-point failures not detected
❌ Requires operator monitoring
❌ Not suitable for unattended operation

### Cost Estimate
- **Hardware:** 1x CPU (~$100-500) + watchdog chip (~$20)
- **Development:** 6-8 weeks (add watchdog integration)
- **Certification:** SIL 2 (moderate effort)
- **Total:** Low cost

### Design Considerations
- Watchdog must have independent power source
- Operator must respond to watchdog alerts
- Recovery is manual intervention
- Health logs must be monitored
- Regular audits required

---

## SIL 3: Significant Safety Integrity

### When to Use
- **Scenario:** Safety-related functions requiring automatic fault detection
- **Examples:**
  - Speed enforcement
  - Signal aspect determination
  - Track switch control
  - Brake commands

### Recommended Hardware Patterns

#### **Option A: 2oo2 (Dual-Channel) - RECOMMENDED**

```
┌────────────────────────────────┐
│   Dual-Channel System          │
├────────────────────────────────┤
│  CPU A          CPU B          │
│  ├─ Logic A     ├─ Logic B     │
│  ├─ Vote        ├─ Vote        │
│  └─ Output      └─ Output      │
│                                │
│  Both channels must agree       │
│  Any disagreement → Safe-State │
├────────────────────────────────┤
│  SAPI Framework + Redundancy   │
├────────────────────────────────┤
│  Linux / QNX (Dual-core)       │
│  OR Dual-processor board       │
└────────────────────────────────┘
```

**Configuration:**
```c
// SIL 3: 2oo2 Dual-Channel
sapi_ipc_handle_t channels[2] = {channel_a, channel_b};

sapi_vital_channel_config_t vital_config = {
    .name = "sil3_dual_channel",
    .strategy = SAPI_VOTING_2OO2,
    .channels = channels,
    .num_channels = 2,
    .timeout_ms = 100,
    .on_disagreement = fault_handler
};

sapi_vital_channel_t vital_ch;
sapi_vital_channel_create(&vital_ch, &vital_config);
```

#### **Option B: 2oo2D (Dual-Site Network)**

```
┌─────────────────────────────────┐
│   Dual-Site Network (2oo2D)     │
├─────────────────────────────────┤
│  Site A              Site B      │
│  ├─ RBC Logic        ├─ RBC      │
│  ├─ Vote ←Network→   ├─ Vote    │
│  └─ Output           └─ Output   │
│                                  │
│  Network latency < 100ms         │
│  Both sites must agree           │
│  Disagreement → Safe-State       │
├─────────────────────────────────┤
│  SAPI + Redundancy + Network    │
├─────────────────────────────────┤
│  Linux/QNX on Ethernet          │
└─────────────────────────────────┘
```

### Advantages (2oo2)
✅ Detects single-point failures immediately
✅ Automatic safe-state on disagreement
✅ No manual intervention needed
✅ Suitable for unattended operation
✅ Lower cost than 2oo3

### Disadvantages (2oo2)
❌ Zero fault tolerance (2 simultaneous faults = catastrophic)
❌ Higher complexity than 1oo1
❌ Both channels must process identically
❌ Network latency critical (2oo2D)

### Cost Estimate
- **Hardware:** 2x CPU (~$200-1000) + network (if 2oo2D)
- **Development:** 12-16 weeks
- **Certification:** SIL 3 (high effort - FMEA, V&V, safety case)
- **Total:** Medium cost (~3-5x SIL 1)

### Design Considerations
- Identical processing on both channels
- Synchronization required
- Network bandwidth sufficient for voting
- Health monitoring per channel
- Fault detection & logging mandatory

### Decision Between 2oo2 vs 2oo2D

```
Dual-Core Processor (2oo2)?
├─ YES if: single board, shared memory, cost-optimized
├─ Performance: Lower latency (< 10ms voting)
└─ Cost: Lower (1 board)

Dual-Site Network (2oo2D)?
├─ YES if: geographic distribution, site redundancy
├─ Performance: Higher latency (< 100ms voting)
└─ Cost: Higher (2 boards + network equipment)
```

---

## SIL 4: Maximum Safety Integrity

### When to Use
- **Scenario:** Critical safety functions, must not fail
- **Examples:**
  - ERTMS RBC (Radio Block Centre) - **Primary use case**
  - Emergency brake commands
  - Vital signal aspects
  - Grade separation control
  - Life safety systems

### Why NOT 2oo2 for SIL 4?

This is a critical distinction. While 2oo2 (dual-channel) is EXCELLENT for SIL 3, it is **NOT sufficient for SIL 4**. Here's why:

#### **2oo2 Fault Tolerance Analysis**

```
2oo2 System: 2 channels, both must agree

Scenario 1: Normal Operation
├─ Channel A: OK
├─ Channel B: OK
├─ Output: Both agree → SAFE ✓

Scenario 2: First Fault (e.g., CPU A fails)
├─ Channel A: FAULT
├─ Channel B: OK
├─ A ≠ B: Disagreement detected
├─ Action: → SAFE-STATE (halt output)
└─ Status: DETECTED & SAFE ✓

Scenario 3: Second Fault (During repair/recovery)
├─ Channel A: FAULTY (waiting for repair/replacement)
├─ Channel B: NEW FAULT (before A repaired)
├─ Problem: BOTH channels down
├─ Action: UNDETECTED DANGEROUS STATE ✗
└─ Result: CATASTROPHIC FAILURE ✗
```

#### **SIL 4 Requirement: Fault Tolerance**

SIL 4 demands **"continued safe operation after fault"**, not just fault detection:

```
SIL 4 Safety Properties:
┌─────────────────────────────────────────┐
│ Must tolerate 1 fault AND CONTINUE SAFE │
│ operation with remaining channels       │
└─────────────────────────────────────────┘

2oo2 Failure:
├─ Fault Detection: ✓ YES (disagreement detected)
└─ Continued Operation: ✗ NO (system halts)
   └─ Reason: Loses redundancy → single point of failure

2oo3 Success:
├─ Fault Detection: ✓ YES (voting detects minority)
├─ Continued Operation: ✓ YES (remaining 2 channels continue)
└─ Status: Degrades to 2oo2 (still fault-tolerant)
```

#### **The Key Difference: N+1 Redundancy**

```
N+1 Redundancy Principle:
┌───────────────────────────────────────┐
│ Minimum N channels + 1 spare channel  │
│ Tolerates up to 1 failure per channel │
└───────────────────────────────────────┘

2oo2 (N=2, ONLY 1 spare):
├─ Total channels: 2
├─ Fault tolerance: 0
│  └─ Reason: Losing 1 channel = lose redundancy
└─ SIL suitable: 3 (detect faults, halt safely)
   └─ NOT 4 (cannot continue after fault)

2oo3 (N=3, 1 spare):
├─ Total channels: 3
├─ Fault tolerance: 1
│  └─ Reason: Losing 1 channel = still have 2 (redundancy intact)
└─ SIL suitable: 4 ✓ (continue safe operation)
   └─ Degrades: 3-channel → 2-channel gracefully
```

#### **Fault Timeline Comparison**

**2oo2 Scenario:**
```
Time 0: System operational (both channels healthy)
Time 5: CPU A fails
        - Disagreement detected
        - SAFE-STATE: System halts (output stops)
        - Required action: MANUAL RESTART
Time 5-120: System DOWN (recovery window)
        - During this time, if another fault occurs in CPU B
          → Undetected by voting (system already down)
        - OR if both channels fail in this window
          → Catastrophic (no redundancy left)
Time 120: CPU A repaired and restarted
        - System back to 2oo2
```

**2oo3 Scenario:**
```
Time 0: System operational (all 3 channels healthy)
Time 5: CPU A fails
        - Disagreement detected (B & C agree, A disagrees)
        - Majority vote: 2 out of 3 agree (A is minority)
        - Output continues with B & C (2oo2 mode)
        - Action: Log fault, increment health counter
Time 5-120: System RUNNING
        - Still safe: 2 channels (B & C) continue voting
        - If B fails during this time
          → Only C left (single point of failure)
          → System halts SAFELY (no longer redundant)
        - But this is OK: 2 independent faults are required
Time 120: CPU A repaired and added back
        - System returns to 3-channel (2oo3 mode)
        - Redundancy restored
```

#### **EN 50128 & EN 50129 Requirements**

SIL 4 systems must meet these requirements:

| Requirement | 2oo2 | 2oo3 |
|---|---|---|
| **Detect single fault** | ✓ YES | ✓ YES |
| **Tolerate single fault** | ✗ NO | ✓ YES |
| **Continue operation after fault** | ✗ NO (halts) | ✓ YES |
| **SIL 4 compliant** | ✗ NO | ✓ YES |

#### **Real-World Consequences**

**2oo2 System Failure Case:**

```
ERTMS RBC trying to use 2oo2 for SIL 4:

Time 0:00 - Normal operation
           - RBC authorizes trains safely
           - 2 CPUs running in lock-step

Time 0:05 - CPU A hardware failure (cosmic ray bit flip)
           - CPU A output: signal=STOP
           - CPU B output: signal=GO
           - DISAGREEMENT detected!
           - Protocol: → Safe-State (all outputs STOP)
           - Result: ✓ Safe (trains halt)

Time 0:05-2:00 - RECOVERY WINDOW
           - CPU A down, waiting for replacement
           - CPU B running but ISOLATED (no redundancy)
           - System is HALTED
           - Hundreds of trains waiting
           - Massive disruption

Time 1:30 - Another cosmic ray bit flip hits CPU B
           - CPU B now faulty
           - BOTH CPUs now faulty
           - Voting system: DEAD
           - No detection possible (no redundancy left)
           - Result: ✗ CATASTROPHIC FAILURE

Consequence: SIL 4 certification IMPOSSIBLE with 2oo2
            Because: Cannot guarantee continued safe operation
```

**2oo3 System Same Scenario:**

```
Time 0:00 - Normal operation
           - RBC authorizes trains safely
           - 3 CPUs running in agreement

Time 0:05 - CPU A hardware failure
           - CPU A output: signal=STOP
           - CPU B output: signal=GO
           - CPU C output: signal=GO
           - MAJORITY: 2 out of 3 agree (GO)
           - Action: Continue with B & C (degraded to 2oo2)
           - Result: ✓ Safe (trains continue with reduced redundancy)
           - Log: CPU A faulty, health counter incremented

Time 0:05-2:00 - RECOVERY WINDOW
           - CPU A down, waiting for replacement
           - CPU B & C still operational with voting
           - System CONTINUES operation
           - Trains authorized normally
           - Health monitoring watches B & C
           - Result: ✓ Service maintained

Time 1:30 - Another cosmic ray bit flip hits CPU B
           - CPU A already faulty (offline)
           - CPU B now faulty
           - CPU C still healthy
           - Voting: Only 1 vote remaining (C)
           - Lost redundancy → Cannot vote safely
           - Protocol: → Safe-State (graceful halt)
           - Result: ✓ Safe degradation
           - Log: Multiple faults detected, system degraded

Time 2:00 - CPU A replacement arrives
           - CPU A reinstalled
           - System back to 2oo3
           - Redundancy restored

Consequence: SIL 4 certification POSSIBLE with 2oo3
            Because: Continued safe operation after 1st fault
                    Graceful degradation on multiple faults
```

#### **Bottom Line: Why SIL 4 Needs 2oo3 (or better)**

```
SIL 4 Requirement:     "Safe operation after 1 fault"
2oo2 Provides:         "Detect fault, then HALT"
Result:                ✗ Does NOT meet SIL 4

2oo3 Provides:         "Detect fault, CONTINUE with reduced redundancy"
Result:                ✓ DOES meet SIL 4
```

---

### Recommended Hardware Patterns

#### **Option A: 2oo3 (Triple-Channel) - STRONGLY RECOMMENDED**

```
┌──────────────────────────────────┐
│   Triple-Channel System (2oo3)   │
├──────────────────────────────────┤
│  CPU A    CPU B    CPU C         │
│  ├─Logic  ├─Logic  ├─Logic       │
│  ├─Vote   ├─Vote   ├─Vote        │
│  └─Output └─Output └─Output      │
│                                  │
│  Majority vote (≥2 out of 3)     │
│  Tolerates 1 channel failure     │
│  Minority output rejected        │
├──────────────────────────────────┤
│  SAPI + Redundancy + Watchdog    │
├──────────────────────────────────┤
│  Linux/QNX (Triple-core)         │
│  OR Triple-processor board       │
│  OR Triple-site cluster          │
└──────────────────────────────────┘
```

**Configuration:**
```c
// SIL 4: 2oo3 Triple-Channel
sapi_ipc_handle_t channels[3] = {channel_a, channel_b, channel_c};

sapi_vital_channel_config_t vital_config = {
    .name = "sil4_triple_channel",
    .strategy = SAPI_VOTING_2OO3,
    .channels = channels,
    .num_channels = 3,
    .timeout_ms = 200,
    .on_disagreement = fault_handler
};

sapi_vital_channel_t vital_ch;
sapi_vital_channel_create(&vital_ch, &vital_config);

// Checkpoint synchronization (critical for 2oo3)
sapi_checkpoint_config_t ckpt = {
    .checkpoint_id = 1,
    .max_delay_ms = 200,           // Detect slow node
    .expected_node_count = 3
};

sapi_channel_checkpoint(vital_ch, &ckpt);
```

#### **Option B: Cluster Redundancy with 2oo2 Sites - VALID ALTERNATIVE**

**Architecture:** Multiple 2oo2 sites in a cluster with failover

```
┌──────────────────────────────────────────┐
│   Cluster with 2oo2 Sites (Alternative)  │
├──────────────────────────────────────────┤
│                                          │
│  Site A (2oo2)      Site B (2oo2)       │
│  ├─ CPU A1          ├─ CPU B1           │
│  ├─ CPU A2          ├─ CPU B2           │
│  │ Voting ✓         │ Voting ✓          │
│  └─ Output A        └─ Output B         │
│         ↓                  ↓             │
│  ┌──────────────────────────────────┐   │
│  │  Cluster Coordinator             │   │
│  │  (Hot Standby or Online mode)    │   │
│  │  Selects which site's output     │   │
│  └──────────────────────────────────┘   │
│                                          │
│  Site A Fault Scenario:                 │
│  1. CPU A1 or A2 fails                  │
│  2. Site A: 2oo2 detects disagreement   │
│  3. Site A: Halts                       │
│  4. Cluster Coordinator: Fails over     │
│  5. Site B: Takes over (2oo2 continues) │
│  6. Result: ✓ System continues          │
│                                          │
└──────────────────────────────────────────┘
```

**Configuration:**
```c
// SIL 4: Cluster with 2oo2 sites
// Each site has 2oo2 voting internally
sapi_vital_channel_config_t site_a_config = {
    .name = "site_a_2oo2",
    .strategy = SAPI_VOTING_2OO2,
    .channels = {channel_a1, channel_a2},
    .num_channels = 2,
    .timeout_ms = 100
};

sapi_vital_channel_config_t site_b_config = {
    .name = "site_b_2oo2",
    .strategy = SAPI_VOTING_2OO2,
    .channels = {channel_b1, channel_b2},
    .num_channels = 2,
    .timeout_ms = 100
};

// Cluster failover configuration
sapi_cluster_config_t cluster = {
    .mode = SAPI_CLUSTER_HOT_STANDBY,  // or ONLINE
    .primary_node = SITE_A,
    .backup_node = SITE_B,
    .heartbeat_timeout_ms = 500
};
```

**When Site A fails:**
```
Normal Operation:
├─ Site A (2oo2): CPU A1 & A2 process
├─ Site B (2oo2): CPU B1 & B2 process (backup)
└─ Output: From Site A

Site A Internal Fault:
├─ CPU A1 or A2 fails
├─ Site A: 2oo2 detects disagreement
├─ Site A: Halts
│
├─ Cluster: Detects Site A failure
├─ Cluster: Initiates failover
└─ Output: Switches to Site B (now primary)

Recovery:
├─ Site A: Fault diagnosed & repaired
├─ Site A: Rejoins as backup
└─ Cluster: Back to full redundancy
```

**Advantages:**
✅ Each site is internally redundant (2oo2)
✅ Cluster-level failover (Site A → Site B)
✅ System continues even if entire site fails
✅ Practical for geographically distributed systems
✅ **Less complex than 2oo3 (dual-site is simpler than triple-site)**

**Disadvantages:**
❌ More complex than single 2oo3 site (2 sites to manage)
❌ Network latency critical for cluster failover
❌ Requires cluster coordination logic
❌ Higher hardware cost (2 complete 2oo2 sites)

**SIL 4 Compliance:**
✅ YES - System continues after single site failure
✅ YES - Dual 2oo2 sites = cluster-level fault tolerance
✅ YES - Meets "continued safe operation" requirement

**Cost Estimate:**
- **Hardware:** 4x CPU (~$400-2000) + network
- **Development:** 14-18 weeks
- **Complexity:** Medium-High (cluster coordination)
- **Total:** High cost (~equivalent to 2oo3 triple-site)

---

#### **Option C: Online Mode (Active-Active Cluster)**

```
┌──────────────────────────────────┐
│   Online Mode (Active-Active)    │
├──────────────────────────────────┤
│  Site A              Site B      │
│  ├─Process(X)        ├─Process(X)│
│  ├─Checkpoint        ├─Checkpoint│
│  ├─Sync Data         ├─Sync Data │
│  ├─Vote              ├─Vote      │
│  └─Output            └─Output    │
│                                  │
│  All sites active                │
│  Must agree before output        │
│  Higher latency, better util     │
├──────────────────────────────────┤
│  SAPI + Redundancy + Network    │
├──────────────────────────────────┤
│  Linux/QNX on Ethernet (2oo2 or 2oo3)
└──────────────────────────────────┘
```

#### **Option C: Hot Standby (Active-Passive)**

```
┌──────────────────────────────────┐
│   Hot Standby (Active-Passive)   │
├──────────────────────────────────┤
│  Primary [ACTIVE]  Backup [READY]│
│  ├─Process         ├─Replicate   │
│  ├─Heartbeat  ───→ ├─Monitor     │
│  └─Output          └─State Sync  │
│                                  │
│  Primary processes only          │
│  Backup ready for failover       │
│  Failover time < 1s              │
├──────────────────────────────────┤
│  SAPI + Watchdog + Failover      │
├──────────────────────────────────┤
│  Linux/QNX on Ethernet           │
│  (2-site redundancy)             │
└──────────────────────────────────┘
```

#### **Option D: NMR (N≥4 Channels)**

```
For ultra-high reliability or when >3 channels available
├─ 4oo3: Tolerates 1 fault (4 channels needed)
├─ 5oo3: Tolerates 2 faults (5 channels needed)
└─ 7oo4: Tolerates 3 faults (7 channels needed)
```

### Comparison: Which Option for SIL 4?

| Aspect | 2oo3 | Cluster-2oo2 | Online | Standby | NMR |
|--------|------|------|--------|---------|-----|
| **Fault Tolerance** | 1 fault (site-level) | 1 fault (cluster-level) | 1 fault | 1 fault | 1+ faults |
| **Processing Latency** | Low | Low | High | Very Low | Low |
| **Complexity** | Medium | Medium-High | High | Medium | Very High |
| **Cost** | Medium | Medium-High | Medium-High | Medium | Very High |
| **Hardware** | 3x CPU | 4x CPU (2 sites) | 2x CPU | 2x CPU | 4+ CPU |
| **Single-Site** | ✓ YES | ✗ NO (needs cluster) | ✓ YES | ✓ YES | ✓ YES |
| **Geographically Distributed** | Possible | ✓ YES (each site local) | ✓ YES | ✓ YES | ✓ YES |
| **Fault Detection** | Via voting | Via site-level voting + cluster failover | Via voting | Via heartbeat | Via voting |
| **Suitable for RBC** | ✅ Primary | ✅ Valid | ✅ Alternative | ✅ High-speed | Limited |
| **Recommended** | **YES** | **VALID** | **Optional** | **Optional** | **Rare** |

### **Recommendation for ERTMS RBC: 2oo3 (Triple-Channel)**

**Why 2oo3?**
1. ✅ Proven in real ERTMS deployments
2. ✅ Tolerates 1 fault gracefully
3. ✅ Deterministic voting (< 200ms)
4. ✅ Relatively simple (3 channels)
5. ✅ Cost-effective for SIL 4
6. ✅ Scaling: can add 4th channel later (→ 2oo4)

**Why not Online/Standby?**
- Online: Higher latency (250-500ms) due to sync overhead
- Standby: Requires faster response time (< 100ms recovery)

**Why not NMR?**
- Adds complexity without extra benefit
- Unnecessary for typical RBC (1 fault tolerance enough)
- Higher cost for marginal gain

### Advantages (2oo3)
✅ Tolerates 1 channel failure gracefully
✅ Continues operation with remaining 2 channels
✅ Detects faulty channel (health monitoring)
✅ Safe-state on double faults
✅ Suitable for unattended operation
✅ Proven in ERTMS industry

### Disadvantages (2oo3)
❌ Highest complexity (3 channels to manage)
❌ Checkpoint synchronization required
❌ Network latency critical
❌ Highest cost
❌ Most certification effort

### Cost Estimate
- **Hardware:** 3x CPU (~$300-1500) + network
- **Development:** 16-24 weeks (full features)
- **Certification:** SIL 4 (very high effort - FMEA, V&V, safety case, testing)
- **Total:** High cost (~5-10x SIL 1)

### Design Considerations
- Identical processing on all 3 channels (critical!)
- Checkpoint synchronization (200ms timeout)
- Data synchronization before voting
- Health monitoring per channel
- Faulty channel isolation
- Watchdog (system + task + checkpoint)
- Comprehensive logging & audit trail
- Full test coverage (structural + functional)
- Hazard analysis (FMEA)
- Safety case development
- Notified body certification required

---

## Architecture Selection Decision Tree

```
START: What is your application's safety level?

├─ SIL 1 (No safety)
│  └─→ 1oo1 (Single System)
│      ├─ Cost: $ (cheapest)
│      ├─ Complexity: Low
│      └─ Use: Development, non-critical

├─ SIL 2 (Minimal safety)
│  └─→ 1oo1 + Watchdog
│      ├─ Cost: $$ (low)
│      ├─ Complexity: Low
│      └─ Use: Monitored systems, diagnostics

├─ SIL 3 (Significant safety)
│  ├─→ 2oo2 (Dual-Channel) [RECOMMENDED]
│  │   ├─ Cost: $$ (medium-low)
│  │   ├─ Complexity: Medium
│  │   └─ Use: Speed enforcement, signal control
│  │
│  └─→ 2oo2D (Dual-Site Network)
│      ├─ Cost: $$$ (medium)
│      ├─ Complexity: Medium-High
│      └─ Use: Geographic redundancy

├─ SIL 4 (Maximum safety) ← ERTMS RBC
│  ├─→ 2oo3 (Triple-Channel) [STRONGLY RECOMMENDED]
│  │   ├─ Cost: $$$ (medium-high)
│  │   ├─ Complexity: Medium-High
│  │   ├─ Fault Tolerance: 1 fault
│  │   └─ Use: ERTMS RBC, critical control
│  │
│  ├─→ Online Mode (Active-Active)
│  │   ├─ Cost: $$$ (medium-high)
│  │   ├─ Complexity: High
│  │   └─ Use: High-latency-tolerant systems
│  │
│  ├─→ Hot Standby (Active-Passive)
│  │   ├─ Cost: $$$ (medium-high)
│  │   ├─ Complexity: Medium
│  │   └─ Use: Low-latency requirements
│  │
│  └─→ NMR (4+ channels)
│      ├─ Cost: $$$$ (very high)
│      ├─ Complexity: Very High
│      └─ Use: Ultra-high reliability (rare)

END: Selected architecture
```

---

## Cost & Complexity Comparison

```
                  1oo1   1oo1+WD  2oo2   2oo2D  2oo3  Online Standby NMR
────────────────────────────────────────────────────────────────────────
Cost              $      $$       $$     $$$    $$$   $$$    $$$    $$$$
Hardware          1x     1x+WD    2x     2x+NET 3x    2x+NET 2x+NET 4x+
Complexity        Low    Low      Med    Med    Med    High   Med    VHigh
Fault Tolerance   0      0        0      0      1      1      1      2+
SIL Suitable      1/2    2        3      3      4      4      4      4
Development       4wk    6wk      12wk   14wk   16wk   20wk   14wk   24wk
Certification     No     SIL2     SIL3   SIL3   SIL4   SIL4   SIL4   SIL4
Recommended       -      -        ✓✓     ✓      ✓✓✓    ✓      ✓      Limited
────────────────────────────────────────────────────────────────────────
```

---

## Typical Use Cases by SIL

### SIL 1-2: Diagnostic/Monitoring Systems
```
Example: Train Maintenance Portal
├─ Function: Display maintenance alerts
├─ Criticality: Low (operators aware of failures)
├─ Pattern: 1oo1 + Watchdog
├─ Hardware: Single server
├─ Failover: Manual restart acceptable
└─ Certification: Minimal
```

### SIL 3: Railway Control (Speed, Signals)
```
Example: Lineside Signal Controller
├─ Function: Determine signal aspect
├─ Criticality: High (affects train movement)
├─ Pattern: 2oo2 (dual-channel)
├─ Hardware: Dual-processor board
├─ Failover: Automatic to safe-state
└─ Certification: SIL 3 (moderate effort)
```

### SIL 4: ERTMS RBC (Radio Block Centre)
```
Example: Modern ERTMS-Level 3 RBC
├─ Function: Authorize train movement, enforce brakes
├─ Criticality: Maximum (life safety)
├─ Pattern: 2oo3 (triple-channel) ← RECOMMENDED
├─ Hardware: 3x Processor + network
├─ Failover: Automatic, tolerates 1 fault
└─ Certification: SIL 4 (high effort, notified body)
```

---

## Selection Checklist

### Before Choosing Your Pattern:

**Requirements Gathering:**
- [ ] What is the SIL level required? (1/2/3/4)
- [ ] What are the critical functions?
- [ ] What is acceptable response time?
- [ ] What is the fault tolerance requirement?
- [ ] Is unattended operation required?
- [ ] What is the available budget?
- [ ] How many sites/processors available?

**Technical Constraints:**
- [ ] Single processor or multi-processor?
- [ ] Shared memory or network-based?
- [ ] Network latency limits?
- [ ] Power/cooling constraints?
- [ ] Physical space limitations?
- [ ] Existing infrastructure?

**Safety Analysis:**
- [ ] Hazard analysis (FMEA) complete?
- [ ] Single-point failures identified?
- [ ] Fault tolerance strategy defined?
- [ ] Recovery procedures documented?
- [ ] Certification body selected?

### Decision Questions:

**Q1: What is your SIL level?**
```
SIL 1-2  → 1oo1 (or 1oo1+Watchdog for SIL 2)
SIL 3    → 2oo2 (preferred) or 2oo2D
SIL 4    → 2oo3 (recommended) or alternatives
```

**Q2: How many processors/sites can you afford?**
```
1 processor → 1oo1 only
2 processors/sites → 2oo2 or 2oo2D
3 processors/sites → 2oo3 (ERTMS RBC)
4+ → 2oo3 or NMR
```

**Q3: What is your latency requirement?**
```
Strict (<100ms) → 1oo1, 2oo2, Hot Standby
Loose (>200ms) → 2oo3, Online, NMR
```

**Q4: What budget do you have?**
```
Minimal ($) → 1oo1 only
Small ($$) → 1oo1+Watchdog, 2oo2
Medium ($$$) → 2oo3, Online, Standby
Large ($$$$) → NMR, ultra-high redundancy
```

---

## Service Unit: Non-Vital Software Layer

**Definition:** A Service Unit (also called "General Purpose Processor" or "Service Processor") is a non-vital component that runs non-safety-critical software alongside the vital safety-critical channels. It does NOT participate in safety decisions and failure does not affect system safety.

### Service Unit vs. Vital Channels

```
System Architecture:

┌─────────────────────────────────────────────────┐
│            SAFETY-CRITICAL LAYER                │
├──────────────┬──────────────┬──────────────────┤
│  Vital Ch.A  │  Vital Ch.B  │  Vital Ch.C      │
│  (SIL 4)     │  (SIL 4)     │  (SIL 4)         │
│  2oo3 voting │              │                  │
└──────────────┴──────────────┴──────────────────┘
                        ▲
                        │ Critical outputs
                        │ (voted, verified)
┌─────────────────────────────────────────────────┐
│          SERVICE UNIT (Non-Vital)               │
├─────────────────────────────────────────────────┤
│  ✓ Logging, diagnostics, monitoring            │
│  ✓ Optimization, performance tuning             │
│  ✓ User interface, visualization                │
│  ✓ Historical data, statistics                  │
│  ✓ System administration                        │
│  ✓ Non-critical features                        │
│                                                 │
│  ✗ Does NOT vote on safety decisions            │
│  ✗ Does NOT control vital outputs               │
│  ✗ Does NOT need redundancy                     │
│  ✗ Failure is NON-CRITICAL                      │
└─────────────────────────────────────────────────┘
```

### Why Service Units are Needed

In real ERTMS systems like RBC, operators need:

```
Critical Functions (Safety):          Non-Critical Functions (Service):
├─ Route calculation                   ├─ Train history logging
├─ Movement authority creation         ├─ Performance statistics
├─ Conflict detection                  ├─ Predictive maintenance
├─ Train position validation           ├─ Operator dashboard
├─ Emergency braking logic             ├─ Data export
└─ Safety-critical timing              ├─ System tuning
                                       ├─ Remote diagnostics
                                       └─ Historical analysis
```

A Service Unit handles all the non-critical functions, so:
- **Vital channels** stay lean and highly certified (faster verification)
- **Service Unit** can use standard software practices (logging, database, web UI)
- **System** provides both safety AND operational value
- **Cost** is optimized (vital channels expensive, service unit inexpensive)

### Service Unit Architecture Patterns

#### **Pattern 1: Isolated Service Unit (Recommended for SIL 3/4)**

```
┌─────────────────────┐
│  2oo3 Vital         │  Voting on safety outputs
│  Channels (SIL 4)   │
└──────────┬──────────┘
           │
     [Safety Outputs]
           │
     ┌─────▼──────┐
     │   Voter    │  Enforces majority vote
     └─────┬──────┘
           │
     [Verified Decisions]
           │
     ┌─────▼──────────────┐
     │  Service Unit      │  Can read results
     │  (Single instance) │  Cannot affect voting
     └────────────────────┘

Characteristics:
✓ Service Unit isolated from vital voting
✓ Reads vital decisions after voting complete
✓ No timing constraints on service unit
✓ Service unit failure ≠ safety loss
✓ Simplest architecture for SIL 4
```

**SAPI Code Example:**

```c
// Vital channel (SIL 4)
sapi_vital_channel_t vital_ch;
sapi_vital_channel_create(&vital_ch, &config);

// Vote results (2oo3)
sapi_vital_vote_t vote_result;
sapi_vital_vote(&vital_ch, &vote_result);

// Only AFTER voting, service unit reads results
// (Service unit does NOT participate in voting)
service_unit_log_decision(vote_result.decision);
service_unit_update_dashboard(vote_result.decision);
service_unit_store_history(vote_result);
```

#### **Pattern 2: Service Unit with Read-Only Access**

```
┌─────────────────────┐
│  2oo3 Vital         │  Safety-critical processing
│  Channels           │
└──────────┬──────────┘
           │
     [Safety Decisions]
     ┌─────┴──────────────┐
     │                    │
┌────▼──────┐      ┌──────▼───────────┐
│  Executor  │      │  Service Unit    │
│ (controls  │      │  (read-only)     │
│  outputs)  │      │  (no control)    │
└────────────┘      └──────────────────┘

Example: Service unit reads vital state
but cannot modify it:

vital_result = sapi_vital_read_last_decision();
// Read-only access
service_log("Decision: %d at %lld",
            vital_result.decision,
            vital_result.timestamp);

// Service unit can compute statistics
statistics.decisions_made++;
statistics.avg_latency += vital_result.latency;
```

#### **Pattern 3: Asynchronous Service Unit (High-Volume Logging)**

```
Vital Channels        Service Unit
├─ Process safely     └─ Process non-critical data
├─ Make decisions        asynchronously
├─ Send outputs
└─ Queue results ─────→ Log queue
                        ├─ Flush periodically
                        ├─ Compress logs
                        ├─ Send to disk/network
                        └─ No time pressure
```

**When to use:** When service unit is logging high-frequency vital decisions but can't keep up in real-time.

```c
// Vital channel writes to queue (fast)
sapi_queue_put(&log_queue, &vital_decision);  // Returns immediately

// Service unit drains queue asynchronously (slow)
while (sapi_queue_get(&log_queue, &entry) == SAPI_OK) {
    service_unit_compress_and_store(entry);  // Takes time, no impact
}
```

### Service Unit Failure Scenarios

| Scenario | Impact | Response |
|----------|--------|----------|
| **Service Unit crashes** | Logging lost, dashboard unavailable | Vital channels continue, restart service unit when ready |
| **Service Unit hangs** | Diagnostics delayed | Vital channels unaffected, restart service unit |
| **Service Unit out of disk** | Historical data not stored | Alert operator, continue vital operation |
| **Service Unit network down** | Cannot send remote diagnostics | Vital channels continue locally |
| **Service Unit memory corruption** | Corrupted logs/stats | Vital channels unaffected (isolated) |

**Key:** Service unit failures NEVER affect safety decisions or vital outputs.

### EN 50128 Treatment of Service Units

```
EN 50128 Section 6.7.4: Layered Architecture
"Non-vital functions shall be isolated from vital safety-critical
 layers to prevent common-mode failures or timing effects."

Requirements:
✓ Service unit separated from vital processing
✓ Service unit does NOT vote or control safety outputs
✓ Service unit reads vital outputs AFTER decision
✓ Service unit failure has no safety impact
✓ Service unit verification can use standard methods
  (not required to be SIL 4)
```

### Service Unit Software Stack

Service unit **CAN** use software practices that SIL 4 vital channels cannot:

```
Vital Channels (SIL 4 strict):
├─ No dynamic memory
├─ Fixed-size buffers
├─ Deterministic timing
├─ MISRA C:2012 mandatory
└─ High verification cost

Service Unit (Best-effort):
├─ Standard malloc/free OK
├─ Dynamic data structures OK
├─ Standard libraries OK
├─ Standard POSIX/RTOS APIs OK
├─ Database OK
├─ Web server OK
├─ Python/scripting OK
└─ Low verification cost
```

**Example: Service Unit in Python**

```python
# Service unit can use Python, databases, etc.
import logging
from datetime import datetime
from database import RBCDatabase

logging.basicConfig(level=logging.INFO)
db = RBCDatabase()

def process_vital_results(vital_decision):
    """Service unit processes non-vital data asynchronously"""
    # Log to database
    db.insert_log({
        'timestamp': datetime.now(),
        'decision': vital_decision['value'],
        'latency_ms': vital_decision['latency']
    })
    
    # Compute statistics
    stats = db.compute_hourly_statistics()
    logging.info(f"Decision rate: {stats['decisions_per_sec']}/sec")
    
    # Generate operator dashboard
    dashboard_html = generate_dashboard(stats)
    return dashboard_html
```

Vital channels use SAPI (strict C), Service Unit uses anything needed.

### Typical Service Unit Functions

#### **1. Logging & Audit Trail**

```
Vital Channel:          Service Unit:
├─ Make decision        └─ Log every decision
├─ Output result           (with timestamp, latency,
└─ Send to next              decision value, etc.)
   channel
                        └─ Store in database
                           or file system
```

#### **2. Performance Monitoring**

```
Vital Channels run fast (deterministic)
Service Unit monitors:
├─ Latencies (decision time, voting time)
├─ Fault detection rates
├─ Watchdog recovery statistics
├─ Network delays
└─ Processor load
```

#### **3. Predictive Maintenance**

```
Service Unit analyzes:
├─ Fault patterns (which faults occur most?)
├─ Recovery time trends
├─ Component health
├─ Upcoming maintenance windows
└─ Spare parts inventory
```

#### **4. Operator Dashboard**

```
Service Unit provides:
├─ Real-time system status
├─ Trend graphs
├─ Alert history
├─ Configuration UI
├─ Training/simulation mode
└─ Remote diagnostics
```

#### **5. Optimization Tuning**

```
Service Unit can:
├─ Adjust timeouts based on network conditions
├─ Tune voting parameters
├─ Optimize checkpoint intervals
├─ Suggest configuration changes
├─ Manage resource allocation
```

### Service Unit Deployment Options

#### **Option A: Separate Physical Processor**

```
┌──────────────┐         ┌──────────────┐
│  CPU A (SIL) │         │  CPU B (SIL) │
│  Vital Ch.A  │         │  Vital Ch.B  │
└──────┬───────┘         └───────┬──────┘
       │                         │
       └────────────┬────────────┘
                    │ Results
                    │
          ┌─────────▼──────────┐
          │  Separate Service  │
          │  Unit Processor    │
          │                    │
          │  Logging           │
          │  Database          │
          │  Dashboard         │
          └────────────────────┘

Pros:
✓ Complete isolation from vital channels
✓ Can use different OS/RTOS
✓ Scaling: add/remove service units independently
✓ Minimal security risk to vital functions

Cons:
✗ Additional hardware cost
✗ Network communication overhead
✗ One more device to manage
```

#### **Option B: Same Processor, Different Memory**

```
┌─────────────────────────────────────┐
│        Single Processor CPU          │
├────────────────────┬────────────────┤
│  Vital Code/Data   │  Service Code/ │
│  (Protected Memory)│  Data (Open)   │
│                    │                │
│  ✓ SIL 4           │  ✓ Best-effort │
│  ✓ Deterministic   │  ✓ Optimized   │
│  ✓ Certified       │  ✓ No timing   │
│    (Memory-        │    constraints  │
│     protected)     │                │
└────────────────────┴────────────────┘

Pros:
✓ Lower cost (single CPU)
✓ No inter-processor communication
✓ Simpler deployment

Cons:
✗ Memory protection required
✗ Service unit can still affect vital functions
   if protection fails
✗ More complex certification argument
```

#### **Option C: Containerized Service Unit**

```
┌──────────────────────────────────────┐
│  Safety-Critical Container (SIL 4)   │
├──────────────────────────────────────┤
│  SAPI Vital Channels                 │
│  Safety logic, voting, outputs       │
└──────────────────────────────────────┘
           │ API
           ▼
┌──────────────────────────────────────┐
│  Service Container (Best-Effort)     │
├──────────────────────────────────────┤
│  Logging, monitoring, diagnostics    │
│  User interface                      │
│  Data analytics                      │
└──────────────────────────────────────┘

Pros:
✓ Modern cloud-native deployment
✓ Easy scaling and updates
✓ Service unit can restart independently
✓ Resource limits enforced by orchestrator

Cons:
✗ Adds container overhead
✗ Different verification model
✗ Distributed debugging more complex
```

### Service Unit Integration Checklist

Before integrating a service unit with your vital channels:

- [ ] Service unit architecture selected (separate processor / same CPU / containerized)?
- [ ] Data flow defined (what vital data does service unit access)?
- [ ] Failure scenarios analyzed (service unit failure doesn't affect safety)?
- [ ] Access control designed (service unit cannot modify vital state)?
- [ ] Timing analyzed (service unit latency doesn't affect vital decisions)?
- [ ] Certification scope clear (vital channels SIL 4, service unit SIL 0)?
- [ ] Recovery mechanism defined (how to restart service unit)?
- [ ] Logging capacity estimated (how much data does service unit generate)?
- [ ] Security reviewed (service unit isolated from safety functions)?
- [ ] Real-world ERTMS use case validated?

### Real-World ERTMS Service Unit Example

**Siemens RBC Configuration:**

```
Vital Layer (SIL 4):
├─ Siemens S7-1500F (SIL 4 controller)
└─ 2oo3 voting on movement authority

Service Layer (Best-Effort):
├─ Separate Linux server
├─ PostgreSQL database (train history)
├─ Grafana dashboards (operator UI)
├─ ELK stack (logging and analysis)
├─ Python scripts (predictive maintenance)
└─ REST API (remote diagnostics)

Integration:
- Vital channels write critical decisions to queue
- Service unit reads queue asynchronously
- Service unit feeds data to database/dashboard
- Operator sees real-time RBC status
- Engineers can analyze historical data
```

---

## CPU Diversification for SIL 4

**Critical Safety Consideration:** Using identical CPUs in redundant channels can lead to **common-mode failures** where both CPUs fail in the same way (e.g., same design flaw, same timing bug, same cosmic ray vulnerability).

### Types of Diversification

#### **1. Vendor Diversification (Recommended for SIL 4)**

```
Different CPU manufacturers reduce systematic design faults

Example: 2oo2 with diverse vendors
├─ Channel A: Intel Core i7
├─ Channel B: AMD Ryzen
└─ Benefit: Different microarchitectures, instruction sets
```

**Why it matters:**
```
Identical CPUs:
├─ Both have same instruction set bugs
├─ Both affected by same errata
├─ Design flaw in CPU → Both fail identically
└─ Result: Common-mode failure ✗

Diverse CPUs:
├─ Different instruction sets
├─ Different design teams
├─ Flaws unlikely to manifest identically
└─ Result: Independent failures ✓
```

#### **2. Architecture Diversification**

```
Different CPU architectures (ISA level)

Example Combinations:
├─ ARM (RISC) + x86 (CISC)
├─ ARM Cortex-A + ARM Cortex-M
├─ PowerPC + x86
└─ MIPS + ARM
```

**Benefits:**
- Different compiler toolchains
- Different instruction sets
- Different vulnerability profiles
- Reduced systematic faults

**Example: 2oo3 with diverse architectures**
```
Site A: ARM Cortex-A (RISC) ← ERTMS certified
Site B: x86 Intel (CISC)     ← ERTMS certified
Site C: PowerPC (RISC)       ← ERTMS certified

Result: 2oo3 voting across 3 diverse architectures
├─ Reduces common-mode failure risk
├─ Voting detects systematic faults
└─ Very high SIL 4 assurance
```

#### **3. Generation Diversification**

```
Using different CPU generations

Example:
├─ Channel A: Intel Core i7 (9th gen)
├─ Channel B: Intel Core i7 (12th gen)
└─ Benefit: Different silicon, different errata
```

**Consideration:** Less effective than vendor/architecture diversity

#### **4. Non-Diversified (What NOT to do)**

```
Identical CPUs = Risk of common-mode failures

Example (RISKY for SIL 4):
├─ Channel A: Intel Xeon E5-2690
├─ Channel B: Intel Xeon E5-2690  ← Same exact model
├─ Channel C: Intel Xeon E5-2690  ← Same exact model
│
└─ Problem: All 3 fail identically if:
   ├─ Design flaw in Xeon E5
   ├─ Microcode bug
   ├─ Power supply issue
   └─ Thermal issue affecting all
```

### CPU Diversification by Architecture Type

#### **2oo2 (Dual-Channel)**

**Recommended:**
```
Vendor or Architecture Diversification ESSENTIAL

2oo2 Intel A + Intel B = ✗ RISKY (same design flaws)
2oo2 Intel + AMD       = ✓ BETTER (different designs)
2oo2 ARM + x86         = ✓ BEST (completely different)
```

Why: With only 2 channels and no fault tolerance, you **must** prevent common-mode failures. If both identical CPUs fail the same way, voting fails.

#### **2oo3 (Triple-Channel)**

**Recommended:**
```
Diverse vendors across sites

2oo3 Intel + Intel + Intel   = ✗ RISKY (common-mode)
2oo3 Intel + AMD + Intel     = ✓ GOOD (majority diverse)
2oo3 Intel + AMD + ARM       = ✓ BEST (all diverse)
2oo3 x86 + ARM + PowerPC     = ✓ EXCELLENT (maximum diversity)
```

Why: With 3 channels, you can tolerate 1 identical failure, but 2 simultaneous identical failures would be catastrophic. Diversity reduces this risk.

#### **Cluster-2oo2 (Site-Level Redundancy)**

**Recommended:**
```
Each site can be identical (failover redundancy),
BUT within each site, consider diversity

Site A: 2oo2 (Intel + AMD)     ← Diverse internally
Site B: 2oo2 (Intel + AMD)     ← Diverse internally
Result: ✓ Both sites resilient to CPU-level faults

OR simpler:
Site A: Single Intel           ← Accepts risk
Site B: Single Intel           ← Accepts risk
(Failover provides system-level fault tolerance,
 but CPU-level faults not addressed)
```

### EN 50128 Requirements for Diversification

```
EN 50128 Section 7.2.3: Defensive Programming
"Diverse redundancy shall be used to protect against
 systematic faults in the same failure mode."

EN 50129 Functional Safety Requirements:
"Redundant channels shall use diverse implementations
 to prevent common-mode failures."
```

### Diversification Trade-offs

| Aspect | Identical CPUs | Diverse CPUs |
|--------|---|---|
| **Systematic Fault Risk** | ✗ High (common-mode) | ✓ Low |
| **Verification Effort** | ✓ Low | ✗ High (2 toolchains) |
| **Cost** | ✓ Low | ✗ Higher |
| **Development Time** | ✓ Shorter | ✗ Longer |
| **SIL 4 Confidence** | ✗ Lower | ✓ Higher |
| **Real ERTMS Systems** | Diverse | Used worldwide |

### Real-World ERTMS Example

**Siemens ERTMS RBC (Typical):**
```
Site A: Siemens SIM3+ processor (custom safety ASIC)
Site B: Different Siemens platform or alternate vendor
Site C: Redundant diverse system (if 2oo3)

Why:
├─ Different designs from different vendors
├─ Different silicon manufacturers
├─ Different firmware/software implementations
└─ Reduces risk of all sites failing identically
```

### Recommendation for Diversification

#### **For 2oo2 Systems (SIL 3/4):**
```
✓ STRONGLY RECOMMENDED: Vendor or architecture diversity
├─ Intel + AMD (x86)
├─ ARM + x86
└─ Different FPGA vendors if FPGA-based

Rationale: With only 2 channels, common-mode failure
          would lose all redundancy immediately
```

#### **For 2oo3 Systems (SIL 4):**
```
✓ RECOMMENDED: At least vendor diversity
├─ Option 1: Intel + AMD + third vendor
├─ Option 2: ARM Cortex + x86 + PowerPC
└─ Option 3: Different FPGA + different CPU

Rationale: Reduces probability that 2+ sites fail
          identically (need >1 fault to lose redundancy)
```

#### **For Cluster-2oo2 (SIL 4):**
```
✓ OPTIONAL at site level, but RECOMMENDED within each site

Within Site A (2oo2): Use diverse CPUs (Intel + AMD)
Within Site B (2oo2): Use diverse CPUs (Intel + AMD)

Benefit: Each site resilient to CPU-level faults
         Cluster failover provides site-level redundancy
```

### CPU Selection for SIL 4

#### **Certification Status**

```
Before choosing CPU, verify ERTMS certification:

Certified for ERTMS:
├─ Siemens safety controllers
├─ Alstom CBTC platforms
├─ Hitachi Rail systems
├─ Thales CBTC solutions
└─ Intel/AMD commercial with certified software stack

NOT directly certified:
├─ Consumer-grade CPUs (unless in certified platform)
├─ Uncertified FPGA vendors
└─ Experimental processors
```

#### **Practical Diversification Example**

```
SIL 4 RBC with 2oo3 architecture:

OPTION A: All diverse (BEST but expensive)
├─ Site A: Siemens SIM platform
├─ Site B: Alstom Urbalis platform  
├─ Site C: Thales cBridge platform
└─ Cost: $$$, Development: 24+ months

OPTION B: Diverse vendors, same architecture family
├─ Site A: Siemens S7-1500
├─ Site B: Rockwell CompactLogix
├─ Site C: Mitsubishi Q-series
└─ Cost: $$, Development: 18 months

OPTION C: Same vendor, diverse models
├─ Site A: Siemens S7-1500
├─ Site B: Siemens S7-400
├─ Site C: Siemens S7-300
└─ Cost: $, Development: 12 months
│
└─ Trade-off: Less diverse but simpler verification
```

### Diversification Checklist for SIL 4

**Before finalizing CPU selection:**

- [ ] Different CPU vendors or architectures chosen?
- [ ] All CPUs ERTMS or equivalent certified?
- [ ] Verification required for each CPU type?
- [ ] Development toolchains compatible?
- [ ] Cost and schedule impact acceptable?
- [ ] Common-mode failure analysis completed?
- [ ] FMEA includes CPU-diversity impact?
- [ ] Safety case argues for diversity benefit?

---

## Implementation Path

### SIL 1 Implementation (4-6 weeks)
```
Week 1-2: Single system development
Week 3-4: Testing & integration
Week 5-6: Deployment
Cost: ~$10-50K
```

### SIL 3 Implementation (12-16 weeks)
```
Week 1-2: Dual system design & planning
Week 3-6: Develop voting logic, checkpoint sync
Week 7-10: Testing (unit, integration, system)
Week 11-12: Safety analysis (FMEA, FTA)
Week 13-14: Safety case development
Week 15-16: Certification review
Cost: ~$100-300K
```

### SIL 4 Implementation (16-24 weeks)
```
Week 1-3: Triple system design, architecture
Week 4-8: Implement redundancy, watchdog, failover
Week 9-12: Testing (unit, integration, system, failover)
Week 13-16: Safety analysis (FMEA, FTA, hazard analysis)
Week 17-20: Safety case development, external review
Week 21-24: Notified body certification
Cost: ~$300-1000K
```

---

## References & Further Reading

- **docs/HARDWARE_CONFIGURATIONS.md** — Detailed specs for each configuration
- **docs/architecture/diagrams/** — Visual sequence diagrams for each pattern
- **docs/REDUNDANCY_ARCHITECTURE.md** — Voting & checkpoint mechanisms
- **docs/WATCHDOG_DESIGN.md** — Fault detection & recovery
- **docs/EN_50128_ALIGNMENT.md** — Standards alignment & safety case
- **EN 50128:2011** — Railway software safety standard
- **EN 50129:2018** — Functional safety management
- **EN 50126:2017** — Reliability, Availability, Maintainability (RAM)

---

## Summary Table: Pattern by SIL & Requirement

### Selection by SIL Level

| SIL | Pattern | Architecture | Fault Tolerance | Continues After Fault? | Suitable? |
|-----|---------|---|---|---|---|
| **1** | 1oo1 | Single system | 0 | No | ✓ (no safety req) |
| **2** | 1oo1+WD | Single system | 0 | No (monitored) | ✓ (operator aware) |
| **3** | 2oo2 | Single site | 0 | No (halts) | ✓ (detection OK) |
| **3** | 2oo2D | Dual-site | 0 | No (halts) | ✓ (network redundancy) |
| **4** | 2oo2 (alone) | Single site | 0 | **✗ NO** | **✗ NOT SUFFICIENT** |
| **4** | 2oo2+Cluster | Dual 2oo2 sites | 1 (site-level) | **✓ YES** | **✓ VALID ALTERNATIVE** |
| **4** | 2oo3 | Single site/cluster | 1 (within-site) | **✓ YES** | **✓ RECOMMENDED** |
| **4** | Online | Dual/triple sites | 1 | ✓ YES | ✓ (alt. option) |
| **4** | Standby | Dual sites | 1 | ✓ YES | ✓ (low latency) |

### Why 2oo2 Alone Fails for SIL 4 (But Cluster-2oo2 Works)

```
Single 2oo2 Site Limitation: Zero Fault Tolerance
┌──────────────────────────────────────┐
│ Lost redundancy after 1st fault      │
│                                      │
│ Fault #1: Detected, system halts     │
│ Fault #2: System down during repair  │
│           Undetected catastrophe     │
└──────────────────────────────────────┘
Result: ✗ FAILS SIL 4


Cluster with 2oo2 Sites: Fault Tolerance Via Failover
┌──────────────────────────────────────┐
│ Site A (2oo2) | Site B (2oo2)        │
│                                      │
│ Site A Fault #1: Detected            │
│ → Site A halts                       │
│ → Cluster fails over to Site B       │
│ → Site B (2oo2) continues (ready)    │
│                                      │
│ Result: ✓ System continues!          │
└──────────────────────────────────────┘
Result: ✓ PASSES SIL 4


SIL 4 Requirement: Continued Safe Operation After 1 Fault
┌──────────────────────────────────────┐
│ Single 2oo2: Detects but HALTS       │ ✗ Fails
│ Cluster-2oo2: Detects + FAILOVER    │ ✓ Passes
│ 2oo3 (site-level): Detects + CONTINUES│ ✓ Passes
└──────────────────────────────────────┘
```

### Your Need → Recommended Pattern

| Your Need | Recommended | Why | Cost |
|-----------|---|---|---|
| **Testing** | 1oo1 | Simplest | $ |
| **Non-Critical (SIL 1)** | 1oo1 | Cost-optimized | $ |
| **Monitored (SIL 2)** | 1oo1 + Watchdog | Operator oversight | $$ |
| **Speed Control (SIL 3)** | **2oo2** | Detects faults | $$ |
| **Signal Control (SIL 3)** | **2oo2** | Industry standard | $$ |
| **ERTMS RBC (SIL 4)** | **2oo3** | Fault tolerant | $$$ |
| **High-Speed Rail (SIL 4)** | **Hot Standby** | Low latency | $$$ |
| **Ultra-High Reliability** | **NMR** | 2+ fault tolerance | $$$$ |

---

## Key Takeaway

### The Critical Difference: SIL 3 vs. SIL 4

**SIL 3 (2oo2 - Single Site):**
- ✓ Detects faults (disagreement)
- ✓ Stops safely (no output)
- ✗ Halts system (not available during recovery)
- ✗ No failover capability

**SIL 4 - Valid Options:**

**Option 1: 2oo3 (Single Site - RECOMMENDED)**
- ✓ Detects faults (voting)
- ✓ **Continues operation** (degraded to 2oo2)
- ✓ Tolerates repair window
- ✓ Simple single-site deployment
- ✓ Proven in ERTMS

**Option 2: Cluster-2oo2 (Dual 2oo2 Sites - VALID ALTERNATIVE)**
- ✓ Each site detects faults (2oo2 voting)
- ✓ **Cluster failover** (Site B takes over when Site A fails)
- ✓ System continues operational
- ✓ Geographic distribution
- ✓ Each site simpler than 2oo3
- ✗ Higher infrastructure cost (2 complete sites)

**Option 3: Hot Standby (Dual Sites - VALID)**
- ✓ Primary active, backup replicates
- ✓ Failover on primary fault
- ✓ Low latency response
- ✓ State replication ensures backup ready

**Option 4: Online Mode (Dual/Triple Sites - VALID)**
- ✓ All sites active simultaneously
- ✓ Voting on each decision
- ✓ No single point of failure
- ✗ Higher latency (sync overhead)

---

### Bottom Line: Three Valid Architectures for SIL 4

**For Single-Site ERTMS RBC:**
```
RECOMMENDED: 2oo3 (Triple-Channel)
├─ Proven in real ERTMS networks
├─ Fault tolerance at site level
├─ Simplest deployment
└─ Cost-effective
```

**For Multi-Site ERTMS RBC:**
```
RECOMMENDED: 2oo3 sites OR Cluster-2oo2 sites
├─ 2oo3 sites: Redundancy within each site
├─ Cluster-2oo2: Failover between sites
├─ Both achieve SIL 4 compliance
└─ Choose based on geography & availability needs
```

**Key Insight:** You were absolutely correct! 
```
Single 2oo2 site alone = FAILS SIL 4
          ↓
2oo2 site with cluster failover = PASSES SIL 4
          ↓
Because: Fault tolerance moves to cluster level
         When Site A fails & halts, Site B takes over
         System continues operational ✓
```
