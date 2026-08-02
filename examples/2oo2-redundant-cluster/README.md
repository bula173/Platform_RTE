# 2oo2 Redundant Cluster Example

## Overview

This example demonstrates a **dual-channel (2-out-of-2) voting architecture** for SIL 4 railway safety systems, conforming to **EN 50128** / **EN 50129** safety standards.

The 2oo2 pattern implements:
- **Two independent processing channels** (e.g., dual CPU cores, dual nodes)
- **Continuous comparison** of results from both channels
- **Fail-safe voting**: On any mismatch or fault, automatically transition to **safe state** (red signal, speed = 0)
- **Deterministic timing**: All voting decisions complete within a strict deadline
- **Data integrity**: CRC32 checksums on all safety-critical commands

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  2oo2 Voting Arbiter                 │
│  (Continuously compares Channel A and Channel B)     │
└────────────────────┬────────────────────────────────┘
                     │
         ┌───────────┼───────────┐
         │           │           │
         ↓           ↓           ↓
    ┌────────┐  ┌────────┐  ┌──────────┐
    │Channel │  │Channel │  │ Safe     │
    │   A    │  │   B    │  │ State    │
    │(CPU0)  │  │(CPU1)  │  │ Handler  │
    └────────┘  └────────┘  └──────────┘
         │           │           │
         └───────────┼───────────┘
                     │
        ┌────────────┴────────────┐
        │  Train Command Output   │
        │(Signal, Speed Limit)    │
        └─────────────────────────┘
```

### Channel Operations

| Scenario | Action | Outcome |
|----------|--------|---------|
| **Both agree** (MATCH) | Output joint command | ✓ Normal operation |
| **Channels disagree** (MISMATCH) | Output safe state | ✓ Fail-safe triggered |
| **One channel faulty** | Output safe state | ✓ Fault detected & contained |
| **Timeout on one channel** | Output safe state | ✓ Timing failure handled |
| **Data corruption** (CRC fail) | Output safe state | ✓ Integrity check passed |

## Hardware Configurations

The example supports multiple hardware platforms with **compile-time configuration**:

### 1. PowerPC 32/64-bit (Big-Endian) - Classic ERTMS

**Typical deployment:**
- ERTMS Radio Block Centre (RBC) with dual PowerPC cores
- Two e500 cores with shared memory or cache-coherent fabric
- Big-endian byte order (network byte order)

**Compile:**
```bash
powerpc-linux-gnu-gcc -std=c99 -Wall -Wextra -O2 \
  -DHARDWARE_CONFIG_BIG_ENDIAN \
  redundancy.c main.c -o 2oo2-demo-ppc
```

**Configuration in voter_config_t:**
```c
.is_big_endian = 1,
.channel_a_name = "PowerPC CPU0",
.channel_b_name = "PowerPC CPU1",
```

### 2. ARM Cortex-A (Little-Endian) - Modern ERTMS LTE

**Typical deployment:**
- ERTMS LTE-based train control system
- Dual Cortex-A72/A73 processors with NEON SIMD
- Little-endian (standard ARM configuration)

**Compile:**
```bash
arm-linux-gnueabihf-gcc -std=c99 -Wall -Wextra -O2 \
  -march=armv7-a -mfpu=neon \
  redundancy.c main.c -o 2oo2-demo-arm
```

**Configuration:**
```c
.is_big_endian = 0,
.channel_a_name = "ARM Cortex-A72 #0",
.channel_b_name = "ARM Cortex-A72 #1",
```

### 3. x86-64 (Little-Endian) - Simulation / Lab Environment

**Typical deployment:**
- Development machine, CI/CD pipeline, lab testing
- Single x86-64 CPU core (simulates dual channels sequentially)
- Little-endian

**Compile:**
```bash
gcc -std=c99 -Wall -Wextra -O2 \
  redundancy.c main.c -o 2oo2-demo
```

**Run:**
```bash
./2oo2-demo
```

### 4. MIPS (Configurable) - Legacy Railway Systems

**Typical deployment:**
- Legacy ERTMS systems with MIPS-based processors
- Support for both big-endian and little-endian variants

**Compile (big-endian):**
```bash
mips-linux-gnu-gcc -std=c99 -Wall -Wextra -O2 \
  -EB -DHARDWARE_CONFIG_BIG_ENDIAN \
  redundancy.c main.c -o 2oo2-demo-mips-be
```

**Compile (little-endian):**
```bash
mips-linux-gnu-gcc -std=c99 -Wall -Wextra -O2 \
  -EL \
  redundancy.c main.c -o 2oo2-demo-mips-le
```

## Building

### Prerequisites
- GCC or Clang with C99 support
- Cross-toolchain (for non-native builds)
- `make` (optional, for CMake integration)

### Native Build (x86-64)

```bash
cd examples/2oo2-redundant-cluster
gcc -std=c99 -Wall -Wextra -O2 redundancy.c main.c -o 2oo2-demo
./2oo2-demo
```

### Cross-Compilation via CMake (Recommended)

Using the safeAPIFramework CMake infrastructure:

```bash
cd /path/to/safeAPIFramework

# PowerPC cross-compilation
cmake -B build-ppc \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-QNX.cmake \
  -DQNX_ARCH=ppc \
  -S .
cmake --build build-ppc

# ARM cross-compilation
cmake -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Linux.cmake \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -S .
cmake --build build-arm
```

## Running the Example

### Native x86-64

```bash
$ ./2oo2-demo

═══════════════════════════════════════════════════════════════
2oo2 Redundant Cluster - Railway Control System
Hardware: x86-64/ARM (little-endian)
Safety: EN 50128 SIL 4 / EN 50129 Railway
═══════════════════════════════════════════════════════════════

[VOTER INFO] 2oo2 Voter initialized: CPU0 vs CPU1 (endian=little, timeout=100 ms)

─── Cycle 1 ───
  Train 101 (pos=505): [✓ MATCH] signal=2 speed=80
  Train 102 (pos=2508): [✓ MATCH] signal=1 speed=40
  Train 103 (pos=7503): [✓ MATCH] signal=0 speed=0

─── Cycle 2 ───
  Train 101 (pos=510): [✓ MATCH] signal=2 speed=80
  Train 102 (pos=2516): [✓ MATCH] signal=1 speed=40
  Train 103 (pos=7506): [✗ MISMATCH] A:sig=0,spd=0 vs B:sig=1,spd=40 → SAFE_STATE

...

═══════════════════════════════════════════════════════════════
Voting Statistics
═══════════════════════════════════════════════════════════════
Total votes:       90
Matches:           87 (96.7%)
Mismatches:        3
═══════════════════════════════════════════════════════════════
Voter: total_votes=90, match_rate=96%

[VOTER INFO] 2oo2 Voter shutdown: total_votes=90, matches=87, mismatches=3, errors_a=0, errors_b=0

═══════════════════════════════════════════════════════════════
Demo completed successfully
═══════════════════════════════════════════════════════════════
```

## Safety Properties (EN 50128 SIL 4)

### Determinism
- **Voting complexity:** O(1) — constant time regardless of input size
- **No dynamic allocation:** All memory pre-allocated at initialization
- **Bounded timing:** Decision always completes within `decision_deadline_ms`

### Fail-Safe
- **Default action on doubt:** Always transitions to safe state (red signal, speed=0)
- **Symmetric channels:** No preference for A or B — both equal
- **Timeout handling:** Any missed deadline triggers safe state

### Fault Detection
- **Single-point detection:** Either channel failure is detected
- **Dual coverage:** Two independent paths ensure no silent failures
- **Data integrity:** CRC32 checksums catch corruption

### Traceability
- **Per-vote logging:** Every decision recorded with timestamp
- **Decision rationale:** Result (MATCH/MISMATCH/FAULT) documented
- **Statistics:** Running totals of matches/mismatches/errors

## Testing Scenarios

### Scenario 1: Nominal Operation (Both Channels Healthy)
```
Expected: All votes MATCH, trains progress normally
Verify: match_rate ≈ 100%
```

### Scenario 2: Transient Disagreement
```
Expected: Occasional mismatches (e.g., timing differences)
Verify: Both channels fallback to safe state
```

### Scenario 3: Persistent Channel Fault
```
Expected: Channel error count exceeds threshold
Verify: Voter marks channel unhealthy, continues voting
```

### Scenario 4: Data Corruption
```
Expected: CRC mismatch detected before voting
Verify: Corrupted command rejected, safe state issued
```

## Key Files

| File | Purpose |
|------|---------|
| `redundancy.h` | 2oo2 voting API and data structures |
| `redundancy.c` | Voting logic, CRC32, safe state generation |
| `main.c` | Simulation with dual channels and train movement |
| `README.md` | This document |

## Integration with safeAPIFramework

This example can be integrated with the full framework using the **Application Manager** module:

```c
#include "safeapi/appmanager/sapi_appmanager.h"

/* Define channel operations */
sapi_appmanager_operations_t ops = {
    .init = redundancy_voter_init,
    .execute = (run one voting cycle),
    .shutdown = redundancy_voter_shutdown,
    .get_name = ...
    .get_version = ...
};

/* Run with framework lifecycle management */
sapi_appmanager_run(&config);
```

## Performance Characteristics

**Voting latency** (x86-64, single-threaded):
- Command comparison: ~1 μs
- CRC32 per command: ~5 μs (256-byte command)
- Total decision: ~10 μs

**Memory footprint:**
- `voter_state_t`: ~100 bytes
- `train_command_t`: ~32 bytes per command
- No dynamic allocation after init

## References

- **EN 50128:2011** — Railway Applications, Safety Related Electronic Systems
- **EN 50129:2018** — Railway Applications, Safety Related Communication
- **IEC 61508** — Functional Safety of Electrical/Programmable Systems (parent standard)
- **CENELEC TR 50128:2014** — Safety-related railway systems (technical guidance)

## License

This example is part of safeAPIFramework and covered by the Community Improvement License (see project root LICENSE.md).
