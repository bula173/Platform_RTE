# 2oo2 Redundancy Architecture Guide

## Overview

2oo2 (2-out-of-2) voting is a fundamental safety architecture for SIL 3/4 railway systems. Two independent channels execute the same computation, and a voting arbiter compares results. Any disagreement triggers automatic transition to a **safe state**.

This guide explains:
- When to use 2oo2 vs. other redundancy patterns
- How to implement 2oo2 using safeAPIFramework
- Hardware-specific deployment considerations
- Integration with EN 50128 / EN 50129 safety standards

## When to Use 2oo2

### Advantages
✓ **Simple voting**: No threshold analysis, just pairwise comparison  
✓ **Deterministic**: O(1) decision time, bounded delay  
✓ **Fail-safe-biased**: On any doubt, vote for safe state  
✓ **Good for binary decisions**: Go/no-go, green/red, run/stop  

### Disadvantages
✗ **No masking**: Cannot hide single faults (unlike 2oo3)  
✗ **Fault isolation required**: Must detect which channel failed  
✗ **Double hardware cost**: Requires two complete implementations  
✗ **Synchronization complexity**: Channels must agree on timing  

### Use Cases
| Application | Why 2oo2 Works |
|-------------|---|
| **Train safety brakes** | Binary decision: apply or don't apply |
| **Signal aspect logic** | Red/yellow/green determined by dual channels |
| **Speed command** | Both channels compute same limit independently |
| **ERTMS RBC** | Classical dual-processor railway control center |
| **Grade crossing gates** | Go/no-go based on train detection from two sensors |

### When NOT to Use 2oo2
- **Non-safety-critical** features (diagnostics, telemetry) → use simplex
- **High availability needed** (cannot afford safe state) → use 2oo3 (voting majority)
- **Complex decisions** (many thresholds, calibration) → add 3rd channel (2oo3)

## Hardware Architectures

### 1. PowerPC Dual-Core (Big-Endian) - Classic ERTMS RBC

**Typical System:**
```
┌──────────────────────────────────────┐
│  ERTMS Radio Block Centre (RBC)      │
├──────────────────────────────────────┤
│  Processor: PowerPC e500 / e5500      │ (AltiVec SIMD capable)
│  Cores: 2 (dual-core on single die)   │
│  Memory: Shared L3 cache, cache-coherent │
│  Interconnect: FSB (Front Side Bus)   │
│  Endianness: Big-endian (network order) │
│  Clock: 1.5-2.5 GHz                  │
└──────────────────────────────────────┘
```

**Safety Properties:**
- Low-jitter inter-core communication (shared cache coherency)
- Deterministic timing (real-time OS like QNX)
- Hardware watchdog per core
- Memory-mapped I/O for train sensors

**Deployment Example:**

```c
voter_config_t config = {
    .channel_timeout_ms = 50,
    .max_age_ms = 100,
    .channel_a_name = "PowerPC CPU0 (Core 0)",
    .channel_b_name = "PowerPC CPU1 (Core 1)",
    .is_big_endian = 1,
};
```

**Build:**
```bash
# QNX cross-compilation
cmake -B build-qnx \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-QNX.cmake \
  -DQNX_ARCH=ppc \
  -S .
```

**Integration via Messaging:**
```c
/* Channel A (CPU0) computes command, sends via shared memory */
train_command_t cmd_a = channel_a_compute(train);
memcpy(shared_mem->channel_a_cmd, &cmd_a, sizeof(cmd_a));

/* Channel B (CPU1) computes command, sends via shared memory */
train_command_t cmd_b = channel_b_compute(train);
memcpy(shared_mem->channel_b_cmd, &cmd_b, sizeof(cmd_b));

/* Arbiter (on both cores, dual-execution) reads and votes */
vote_result_t result;
redundancy_vote(&voter, cmd_a, cmd_b, &result, &safe_cmd);
```

### 2. ARM Cortex-A Dual-Core (Little-Endian) - Modern ERTMS LTE

**Typical System:**
```
┌──────────────────────────────────────┐
│  ERTMS LTE-Based Train Control Unit   │
├──────────────────────────────────────┤
│  Processor: ARM Cortex-A72/A73        │ (ARMv8 64-bit)
│  Cores: 2 (with optional clustering)  │
│  Memory: L1/L2 per core, shared L3    │
│  Interconnect: AMBA AXI               │
│  Endianness: Little-endian (ARM default) │
│  Clock: 2.0-3.0 GHz                  │
│  SIMD: NEON (128-bit vectors)         │
└──────────────────────────────────────┘
```

**Safety Properties:**
- ARM TrustZone for secure isolation
- Generic timers (ARM CNTV) for deterministic scheduling
- Memory Protection Unit (MPU) for fault isolation
- Hardware Performance Monitor (PMU) for analysis

**Deployment Example:**

```c
voter_config_t config = {
    .channel_timeout_ms = 25,    /* Faster processors */
    .max_age_ms = 50,
    .channel_a_name = "ARM Cortex-A72 #0",
    .channel_b_name = "ARM Cortex-A72 #1",
    .is_big_endian = 0,
};
```

**Build:**
```bash
make ARCH=arm
# or cross-compile with CMake:
cmake -B build-arm \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -DCMAKE_C_FLAGS="-march=armv7-a -mfpu=neon" \
  -S .
```

**Integration via IPC:**
```c
/* Use ARM generic timers for deadline enforcement */
uint64_t deadline_ns = get_arm_timer_ns() + 25000000;  /* 25ms */

/* Send channel A result via message queue */
mq_send(queue_a, (char *)&cmd_a, sizeof(cmd_a), 0);

/* Wait for channel B (with timeout) */
struct timespec timeout = ns_to_timespec(deadline_ns);
ssize_t n = mq_timedreceive(queue_b, buf, sizeof(buf), NULL, &timeout);

if (n < 0 && errno == ETIMEDOUT) {
    result = VOTE_TIMEOUT;
} else {
    redundancy_vote(&voter, &cmd_a, (train_command_t *)buf, &result, &safe_cmd);
}
```

### 3. x86-64 (Little-Endian) - Simulation / Development

**Typical System:**
```
┌──────────────────────────────────────┐
│  Development Workstation / CI Pipeline │
├──────────────────────────────────────┤
│  Processor: Intel/AMD x86-64          │
│  Cores: 2+ (many cores available)     │
│  Memory: Coherent cache hierarchy     │
│  Interconnect: QPI/Infinity Fabric    │
│  Endianness: Little-endian            │
│  Clock: 3.0-5.0 GHz                  │
│  Ext: AVX2/AVX-512 (not used for SIL) │
└──────────────────────────────────────┘
```

**Purpose:**
- **Algorithm validation** before real-time deployment
- **Unit testing** of voting logic
- **CI/CD pipeline** for regression testing
- **Teaching/learning** dual-channel concepts
- **Hardware-agnostic reference** implementation

**Build:**
```bash
make              # Default: x86-64 native
./2oo2-demo      # Immediate results
```

**Note:** x86 voting is sequential (single process), not true dual-channel parallelism. For realistic timing, use threading:

```c
pthread_t threads[2];
train_command_t cmd_a, cmd_b;

/* Channel A thread */
pthread_create(&threads[0], NULL, (void *)&channel_a_compute, (void *)train);

/* Channel B thread */
pthread_create(&threads[1], NULL, (void *)&channel_b_compute, (void *)train);

/* Wait for both (with timeout) */
struct timespec deadline;
clock_gettime(CLOCK_REALTIME, &deadline);
deadline.tv_nsec += 25000000;  /* 25ms voting deadline */

pthread_join(threads[0], NULL);
pthread_join(threads[1], NULL);

/* Vote */
redundancy_vote(&voter, &cmd_a, &cmd_b, &result, &safe_cmd);
```

### 4. MIPS (Configurable Endianness) - Legacy Systems

**Typical System:**
```
┌──────────────────────────────────────┐
│  Legacy ERTMS RBC (pre-2010)          │
├──────────────────────────────────────┤
│  Processor: MIPS R10000 / R12000      │
│  Cores: 2 (dual-processor module)     │
│  Memory: Separate L2 per processor    │
│  Interconnect: HyperTransport / Custom │
│  Endianness: Big-endian OR little-    │
│  Clock: 0.5-1.5 GHz (older)           │
└──────────────────────────────────────┘
```

**Build (Big-Endian):**
```bash
make ARCH=mips-be
```

**Build (Little-Endian):**
```bash
make ARCH=mips
```

**Caution:** MIPS systems may have longer memory latency. Consider increasing voting deadline:

```c
voter_config_t config = {
    .channel_timeout_ms = 100,  /* More conservative for older systems */
    .decision_deadline_ms = 75,
    ...
};
```

## Voting Coordination Patterns

### Pattern 1: Shared Memory (Cache-Coherent)

**Use Case:** Single die dual-core (PowerPC, ARM with shared L3)

```
┌──────────────────────────────────────┐
│ CPU0                      CPU1        │
│ ┌─────────────┐        ┌─────────────┐│
│ │  L1 I-cache │        │  L1 I-cache ││
│ └─────────────┘        └─────────────┘│
│ ┌─────────────┐        ┌─────────────┐│
│ │  L1 D-cache │        │  L1 D-cache ││
│ └──────┬──────┘        └──────┬──────┘│
│        └────────┬─────────────┘       │
│                 │ (cache coherency)   │
│          ┌──────▼──────┐              │
│          │  L3 Cache   │              │
│          └─────────────┘              │
│          ┌──────────────┐             │
│          │ Shared Mem   │             │
│          │ (Commands)   │             │
│          └──────────────┘             │
└──────────────────────────────────────┘
```

**Code:**
```c
/* Global shared memory (cache-coherent) */
typedef struct {
    train_command_t cmd_a;
    train_command_t cmd_b;
    volatile uint32_t ready_a, ready_b;
} shared_channel_state_t;

__attribute__((aligned(64)))  /* Avoid false sharing */
shared_channel_state_t state = {0};

/* CPU0: compute and write */
state.cmd_a = channel_a_compute(train);
__sync_synchronize();  /* Memory fence */
state.ready_a = 1;

/* CPU1: compute and write */
state.cmd_b = channel_b_compute(train);
__sync_synchronize();
state.ready_b = 1;

/* Arbiter (either CPU): read and vote */
while (!state.ready_a || !state.ready_b) {
    if (timeout_expired()) return VOTE_TIMEOUT;
}
redundancy_vote(&voter, &state.cmd_a, &state.cmd_b, &result, &safe_cmd);
```

### Pattern 2: Message Passing (Symmetric)

**Use Case:** Distributed system, network redundancy, IPC via queues

```
┌──────────────┐
│   Channel A  │  Compute command
└──────┬───────┘
       │ MsgSend
       │ (TCP/UDP / QNX msgpass / RMA)
       │
    ┌──▼──────────────┐
    │   Voter Arbiter │  Compare & vote
    └──┬──────────────┘
       │
       │ MsgSend
┌──────▼───────┐
│   Channel B  │  Verify result
└──────────────┘
```

**Code:**
```c
/* Channel A task */
train_command_t cmd_a = channel_a_compute(train);
send_to_voter(voter_coid, &cmd_a);

/* Channel B task */
train_command_t cmd_b = channel_b_compute(train);
send_to_voter(voter_coid, &cmd_b);

/* Voter task (waits for both, votes) */
train_command_t cmd_a_rx = {0};
train_command_t cmd_b_rx = {0};

rcvid = MsgReceive(voter_chid, &cmd_a_rx, sizeof(cmd_a_rx), NULL);
rcvid = MsgReceive(voter_chid, &cmd_b_rx, sizeof(cmd_b_rx), NULL);

vote_result_t result;
redundancy_vote(&voter, &cmd_a_rx, &cmd_b_rx, &result, &safe_cmd);
```

### Pattern 3: Asymmetric (One Coordinator)

**Use Case:** Single voter coordinates dual channels (e.g., ECU + remote sensor)

```
┌──────────────────────────────┐
│   Voter Coordinator          │
├──────────┬───────────────────┤
│          │ Send task         │
│ Compute  ├──────────→ Channel│ B
│ Channel  │                   │ (external)
│ A        ← recv response ─────┤
│          │                   │
└──────────┴───────────────────┘
```

**Code:**
```c
/* Voter (on Host ECU) computes Channel A locally */
train_command_t cmd_a = channel_a_compute(train);

/* Request Channel B opinion via RPC */
channel_b_request_t req = { .train_id = train->id, ... };
send_rpc(channel_b_rpc_server, &req, sizeof(req));

/* Wait for response (with timeout) */
ssize_t n = recv_rpc_response(buf, sizeof(buf), timeout_ms);
if (n < 0) return VOTE_TIMEOUT;

train_command_t cmd_b = *(train_command_t *)buf;

/* Vote locally */
redundancy_vote(&voter, &cmd_a, &cmd_b, &result, &safe_cmd);
```

## Timing Analysis

### PowerPC Dual-Core Example

```
┌─────────────────────────────────────────┐ 
│ Cycle T (50ms deadline)                 │
├──────────┬──────────┬────────┬──────────┤
│ T+0      │ T+15ms   │ T+30ms │ T+45ms   │
├──────────┼──────────┼────────┼──────────┤
│ Compute  │ Write    │ Arbiter│ Vote     │
│ A & B    │ Results  │ Reads  │ Decision │
│ (10ms)   │ (2ms)    │ (5ms)  │ (3ms)    │
└──────────┴──────────┴────────┴──────────┘
Total: ~30ms (with 20ms slack)
```

### ARM Cortex-A (Higher Clock)

```
┌──────────────────────────────────┐ 
│ Cycle T (25ms deadline)           │
├─────────┬────────┬──────┬────────┤
│ T+0     │ T+8ms  │ T+15ms│ T+22ms│
├─────────┼────────┼───────┼───────┤
│ Compute │ Write  │ Read  │ Vote  │
│ A & B   │        │       │       │
│ (5ms)   │ (1ms)  │(3ms)  │(2ms)  │
└─────────┴────────┴───────┴───────┘
Total: ~20ms (with 5ms slack)
```

## Fault Modes and Recovery

### Mode 1: MATCH (Nominal)
- **Condition:** cmd_a == cmd_b (byte-identical)
- **Action:** Execute joint command
- **Recovery:** None needed

### Mode 2: MISMATCH (Disagreement)
- **Condition:** cmd_a != cmd_b
- **Action:** Execute safe_state command
- **Root Cause:** Timing skew, instruction decode difference, I-cache miss
- **Recovery:** Log event, increment mismatch counter, continue

### Mode 3: TIMEOUT (One Channel Stalled)
- **Condition:** Channel B did not respond within deadline
- **Action:** Execute safe_state command
- **Root Cause:** Deadlock, ISR overhead, task priority inversion
- **Recovery:** Isolate faulty channel, switch to simplex (graceful degradation)

### Mode 4: CORRUPTION (Data Integrity Failure)
- **Condition:** CRC32 mismatch on received command
- **Action:** Reject command, execute safe_state
- **Root Cause:** Memory fault, bit flip (soft error), transmission error
- **Recovery:** Log memory location, monitor for patterns

## EN 50128 Traceability

| EN 50128 Section | Requirement | Implementation |
|---|---|---|
| 6.4.2.1 | Fail-safe architecture | 2oo2 → safe state on mismatch |
| 6.4.2.4 | Periodic self-testing | CRC32 integrity checks |
| 7.2 | Deterministic timing | O(1) voting, bounded deadline |
| 7.3.3 | Fault tolerance | Dual channels, single-point detection |
| 8.2.2 | Traceability | `REQ-2OO2-VOTE-001` tags in code |

## Best Practices

1. **Always use CRC32** on safety-critical data (enables fault detection)
2. **Bounded voting deadline** (max 50ms for railway typical; arm faster)
3. **Symmetric channels** (treat A and B equally; no preference)
4. **Log every mismatch** (investigate root cause later)
5. **Test error injection** (MISMATCH, TIMEOUT, CORRUPTION scenarios)
6. **Monitor statistics** (match_rate should be >99% on healthy system)

## References

- **EN 50128:2011** § 6.4.2 (Fail-safe architecture)
- **EN 50129:2018** § 5.4.2 (2oo2 voting)
- **CENELEC TR 50128:2014** § E.5 (Redundancy patterns)
- Example: `examples/2oo2-redundant-cluster/`

---

**Next Steps:**
1. Choose your hardware platform (PowerPC / ARM / x86)
2. Build the 2oo2 example: `make ARCH=ppc32`
3. Integrate voting into your train control application
4. Test with fault injection (mismatch, timeout, corruption scenarios)
5. Collect statistics to validate SIL 4 assumptions
