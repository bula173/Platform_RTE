# Hardware Configurations Guide

Complete reference for all possible hardware setups supported by safeAPIFramework.

**Compliance:** EN 50128 (SIL 4), EN 50129, EN 50126
**Standard:** CENELEC EN 50128:2011, EN 50129:2018

---

## Overview

safeAPIFramework supports configurations from **single-processor systems** to **multi-site redundant clusters**:

```
├─ Single System (Non-Redundant)
│  ├─ Single Core
│  ├─ Multi-Core (Shared Memory)
│  └─ Heterogeneous (Mixed Processors)
│
├─ Dual-Channel (2oo2 - Mandatory Redundancy)
│  ├─ Dual-Core Processor
│  ├─ Dual-Processor Board
│  ├─ Dual-Site Network
│  └─ Asymmetric (Different processors)
│
├─ Triple-Channel (2oo3 - Majority Vote)
│  ├─ Triple-Core Processor
│  ├─ Triple-Board System
│  ├─ Triple-Site Cluster
│  └─ Mixed Topology
│
├─ N-Modular Redundancy (NMR)
│  └─ 4+ Channels (Arbitrary N)
│
└─ Cluster Configurations
   ├─ Online Mode (Active-Active)
   └─ Hot Standby (Active-Passive)
```

---

## Configuration 1: Single System (Non-Redundant)

### Use Case
- **Scenario:** Non-critical applications, SIL 1/2 systems, development/testing
- **Availability:** Single point of failure (no fault tolerance)
- **Example:** Diagnostic logger, simulation, test harness

### Architecture

```
┌─────────────────────────────────────┐
│      Single Processor / Board       │
├─────────────────────────────────────┤
│                                     │
│   ┌──────────────────────────────┐  │
│   │  RBC Application             │  │
│   │  (single-threaded or         │  │
│   │   multi-threaded on 1 core)  │  │
│   └────────────┬─────────────────┘  │
│                │                     │
│         ┌──────▼──────────┐          │
│         │  safeAPIFramework│          │
│         │  (13 modules)   │          │
│         └────────┬────────┘          │
│                  │                    │
│         ┌────────▼─────────┐         │
│         │  POSIX/QNX OAL   │         │
│         │  (backend)       │         │
│         └────────┬─────────┘         │
│                  │                    │
│         ┌────────▼─────────┐         │
│         │  Linux/QNX OS    │         │
│         │  or Bare Metal   │         │
│         └──────────────────┘         │
│                                     │
└─────────────────────────────────────┘
```

### Configuration Details

```c
// Single system, non-redundant
sapi_ipc_config_t config = {
    .channel_count = 1,              // Single channel
    .redundancy_mode = SAPI_SINGLE,  // No voting
    .timeout_ms = 1000
};
```

### Deployment Example

**Linux Development Machine:**
```bash
./build-linux-native.sh
./build/linux/src/appmanager/test_appmanager
```

**QNX Single Node:**
```bash
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64
./build-qnx.sh
qnx-app
```

### Safety Notes
- ❌ No fault tolerance
- ❌ Single-point failures not detected
- ✅ Good for SIL 1/2 or non-critical paths
- ✅ Suitable for testing SAPI modules

---

## Configuration 2: Dual-Channel System (2oo2)

### Use Case
- **Scenario:** SIL 3 systems, dual-core processors, dual-redundant boards
- **Availability:** Detects any single fault immediately
- **Example:** ERTMS signal controller, safety-related device

### Architecture

```
┌────────────────────────────────────┐
│      Dual-Channel System (2oo2)    │
├────────────────────────────────────┤
│                                    │
│  CPU A            CPU B            │
│  ┌──────────────────────────────┐ │
│  │ RBC Logic    │ RBC Logic    │ │
│  │ (identical)  │ (identical)  │ │
│  ├──────────────────────────────┤ │
│  │ SAPI (vital) │ SAPI (vital) │ │
│  │ (2oo2 mode)  │ (2oo2 mode)  │ │
│  └──────┬───────────────┬───────┘ │
│         │ Data Exchange │         │
│         │ (shared mem   │         │
│         │  or network)  │         │
│  ┌──────▼───────────────▼───────┐ │
│  │   Voting Logic                │ │
│  │  (both must agree)            │ │
│  └──────┬─────────────────────────┘ │
│         │                           │
│  ┌──────▼─────────┐                │
│  │  Output Module │                │
│  │  (to field)    │                │
│  └────────────────┘                │
│                                    │
└────────────────────────────────────┘
```

### 2oo2 Voting Rule
```
Input A = X  ┐
Input B = X  ├──→ Output X  (agreement)
             ┘

Input A = X  ┐
Input B = Y  ├──→ FAULT (disagreement)
             ┘      └─→ Safe-State Immediate
```

### Configuration Details

```c
// Dual-channel 2oo2 configuration
sapi_ipc_handle_t channels[2] = {channel_a, channel_b};

sapi_channel_config_t vital_config = {
    .name = "dual_redundant_signal",
    .strategy = SAPI_VOTING_2OO2,      // Dual-channel voting
    .channels = channels,
    .num_channels = 2,
    .timeout_ms = 100,                 // 100ms for disagreement detection
    .on_disagreement = fault_handler
};

sapi_channel_t vital_ch;
sapi_vital_channel_create(&vital_ch, &vital_config);
```

### Hardware Topologies

**Topology A: Dual-Core Processor**
```
┌──────────────────────────┐
│  Single Processor        │
├──────────────────────────┤
│  Core A        Core B    │
│  ┌──────────┐ ┌──────┐  │
│  │ RBC A    │ │ RBC B│  │
│  └──────────┘ └──────┘  │
│      ↕ Shared Memory ↕   │
│  [Voting on Core A/B]    │
└──────────────────────────┘
```

**Topology B: Dual Processor Board**
```
┌──────────────────────────┐
│  Single Board            │
├──────────────────────────┤
│  CPU A         CPU B     │
│  ┌──────────┐ ┌──────┐  │
│  │ RBC A    │ │ RBC B│  │
│  └──────────┘ └──────┘  │
│  ↕ PCB Bus / SPI / I2C ↕ │
│  [Voting on CPU A/B]     │
└──────────────────────────┘
```

**Topology C: Dual-Site Network**
```
┌─────────────────────────────────────┐
│     Redundant Network (2oo2)        │
├─────────────────────────────────────┤
│                                     │
│  Site A              Network         Site B │
│  ┌──────────┐                  ┌──────────┐│
│  │ RBC A    │  Ethernet/CAN   │ RBC B    ││
│  │ SAPI (A) │◄─────────────────►│ SAPI (B) ││
│  └──────────┘  (replicated)     └──────────┘│
│                                     │
│        Voting Decision:             │
│  - Both sites must agree            │
│  - Network latency < 100ms          │
│  - Timeout on network failure       │
│                                     │
└─────────────────────────────────────┘
```

### Deployment Example

**QNX Dual-Core:**
```bash
# Site A on Core 0
taskset -c 0 ./qnx-app &

# Site B on Core 1
taskset -c 1 ./qnx-app &

# Both instances communicate via shared memory
```

### Safety Notes
- ✅ Detects single-point failures immediately
- ✅ No output on disagreement (fail-safe)
- ❌ Tolerates 0 faults (2 simultaneous failures = catastrophic)
- ✅ Suitable for SIL 3 systems

---

## Configuration 3: Triple-Channel System (2oo3)

### Use Case
- **Scenario:** SIL 4 systems, triple-core/triple-processor, triple-redundant
- **Availability:** Tolerates 1 channel failure, continues with 2oo2
- **Example:** ERTMS RBC (typical), safety-critical control systems

### Architecture

```
┌──────────────────────────────────────┐
│   Triple-Channel System (2oo3)       │
├──────────────────────────────────────┤
│                                      │
│  CPU A       CPU B       CPU C       │
│  ┌──────────────────────────────┐   │
│  │ RBC A   │ RBC B   │ RBC C   │   │
│  │(vote)   │(vote)   │(vote)   │   │
│  └────┬────────┬────────┬───────┘   │
│       │        │        │           │
│  ┌────▼────────▼────────▼────────┐  │
│  │  Voting Engine (2oo3)          │  │
│  │  Majority vote logic:          │  │
│  │  - Count votes                 │  │
│  │  - ≥2 same = decision          │  │
│  │  - Minority = fault            │  │
│  └────┬──────────────────────────┘  │
│       │                             │
│  ┌────▼──────────────────────────┐  │
│  │  Output (if consensus)        │  │
│  │  or Safe-State (if fault)    │  │
│  └──────────────────────────────┘  │
│                                      │
└──────────────────────────────────────┘
```

### 2oo3 Voting Rule
```
Outputs: A=X, B=X, C=X  → Output = X  (3-way agreement)
Outputs: A=X, B=X, C=Y  → Output = X  (2oo3: majority wins)
Outputs: A=X, B=Y, C=Y  → Output = Y  (2oo3: majority wins)
Outputs: A=X, B=Y, C=Z  → FAULT      (no consensus)
```

### Configuration Details

```c
// Triple-channel 2oo3 configuration
sapi_ipc_handle_t channels[3] = {channel_a, channel_b, channel_c};

sapi_channel_config_t vital_config = {
    .name = "triple_redundant_signal",
    .strategy = SAPI_VOTING_2OO3,      // Triple-channel voting
    .channels = channels,
    .num_channels = 3,
    .timeout_ms = 100,
    .on_disagreement = fault_handler
};

sapi_channel_t vital_ch;
sapi_vital_channel_create(&vital_ch, &vital_config);
```

### Hardware Topologies

**Topology A: Triple-Core Processor**
```
┌───────────────────────────────┐
│  Single Processor             │
├───────────────────────────────┤
│  Core A    Core B    Core C   │
│  ┌────────┐ ┌────┐ ┌────────┐│
│  │ RBC A  │ │RBC │ │ RBC C ││
│  │        │ │ B  │ │        ││
│  └────────┘ └────┘ └────────┘│
│  ↕ Shared L3 Cache / Memory ↕ │
│  [2oo3 Voting]               │
└───────────────────────────────┘
```

**Topology B: Triple-Processor Board**
```
┌───────────────────────────────┐
│  Single Board                 │
├───────────────────────────────┤
│  CPU A    CPU B    CPU C      │
│  ┌────────┐ ┌───┐ ┌────────┐ │
│  │ RBC A  │ │RBC│ │ RBC C  │ │
│  └────────┘ │ B │ └────────┘ │
│             └───┘            │
│  ↕ PCB Bus / SPI / Switched ↕ │
│  [2oo3 Voting]               │
└───────────────────────────────┘
```

**Topology C: Triple-Site Cluster (ERTMS RBC)**
```
┌──────────────────────────────────────────┐
│     Triple-Redundant Cluster (2oo3)      │
├──────────────────────────────────────────┤
│                                          │
│  Site A        Site B        Site C      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐│
│  │ RBC A    │ │ RBC B    │ │ RBC C    ││
│  │ (Vote)   │ │ (Vote)   │ │ (Vote)   ││
│  └────┬─────┘ └────┬─────┘ └────┬─────┘│
│       │            │            │      │
│  ┌────▼────────────▼────────────▼───┐  │
│  │  Distributed Voting (Gossip)     │  │
│  │  or Centralized Voter            │  │
│  │  Majority decision (2oo3)        │  │
│  └────┬─────────────────────────────┘  │
│       │                                │
│       ├─→ Signaling System             │
│       ├─→ Train Detection              │
│       └─→ Speed Enforcement            │
│                                          │
│  Fault tolerance:                       │
│  - 1 site fails → system continues      │
│  - 2 sites fail → safe-state            │
│  - Network delay < 200ms                │
│                                          │
└──────────────────────────────────────────┘
```

### Deployment Example

**QNX Triple-Core RBC:**
```bash
# Core 0: RBC Logic A
taskset -c 0 ./rbc-app site=A &

# Core 1: RBC Logic B
taskset -c 1 ./rbc-app site=B &

# Core 2: RBC Logic C
taskset -c 2 ./rbc-app site=C &

# All cores communicate via shared memory
# Voting on shared memory region
```

### Safety Notes
- ✅ Tolerates 1 channel failure
- ✅ Continues operation with 2 healthy channels (2oo2 mode)
- ✅ Detects faulty channel via voting
- ✅ Suitable for SIL 4 systems
- ✅ Proven in ERTMS deployments

---

## Configuration 4: N-Modular Redundancy (NMR)

### Use Case
- **Scenario:** High-reliability systems (N ≥ 4), aerospace, critical control
- **Availability:** Tolerates ⌊(N-1)/2⌋ faults
- **Example:** Railway signaling with 4+ redundant sites

### Architecture

```
N=4 (4oo3 - Tolerates 1 fault):
Channels: A=X, B=X, C=X, D=Y
Majority vote: 3 votes for X, 1 for Y → Output = X

N=5 (5oo3 - Tolerates 2 faults):
Channels: A=X, B=X, C=X, D=Y, E=Y
Majority vote: 3 votes for X, 2 for Y → Output = X

N=7 (7oo4 - Tolerates 3 faults):
Channels: A=X, B=X, C=X, D=X, E=Y, F=Y, G=Y
Majority vote: 4 votes for X, 3 for Y → Output = X
```

### Configuration Details

```c
// N-Modular Redundancy (N=5)
sapi_ipc_handle_t channels[5] = {
    channel_a, channel_b, channel_c, channel_d, channel_e
};

sapi_channel_config_t vital_config = {
    .name = "nmr_5channel",
    .strategy = SAPI_VOTING_NMR,       // N-Modular voting
    .channels = channels,
    .num_channels = 5,                 // N = 5
    .timeout_ms = 150,
    .on_disagreement = fault_handler
};

sapi_channel_t vital_ch;
sapi_vital_channel_create(&vital_ch, &vital_config);
```

### Safety Notes
- ✅ Tolerates multiple simultaneous faults
- ✅ Scales to arbitrary N
- ❌ Higher complexity (voting logic more complex)
- ✅ Suitable for ultra-high-reliability systems

---

## Configuration 5: Online Mode (Active-Active Cluster)

### Use Case
- **Scenario:** Multi-site ERTMS RBC with simultaneous processing
- **Availability:** All sites active, must agree before output
- **Example:** Dual-site or triple-site RBC coordination

### Architecture

```
┌─────────────────────────────────────┐
│  Online Mode (Active-Active)        │
│  All sites processing simultaneously│
├─────────────────────────────────────┤
│                                     │
│  ┌─────────┐   ┌─────────┐         │
│  │ Site A  │   │ Site B  │         │
│  │ [Active]│   │[Active] │         │
│  ├─────────┤   ├─────────┤         │
│  │ Process │   │ Process │         │
│  │ Signal  │   │ Signal  │         │
│  │ X       │   │ X       │         │
│  ├─────────┤   ├─────────┤         │
│  │ Result: │   │ Result: │         │
│  │ GREEN   │   │ GREEN   │         │
│  └────┬────┘   └────┬────┘         │
│       │             │               │
│  ┌────▼─────────────▼────┐          │
│  │  Checkpoint Barrier   │          │
│  │  (synchronize timing) │          │
│  └────┬─────────────────┘          │
│       │                             │
│  ┌────▼──────────────────┐          │
│  │  Exchange Results     │          │
│  │  (data sync)          │          │
│  └────┬──────────────────┘          │
│       │                             │
│  ┌────▼──────────────────┐          │
│  │  Voting               │          │
│  │  (both must agree)    │          │
│  └────┬──────────────────┘          │
│       │                             │
│  ┌────▼──────────────────┐          │
│  │  Commit & Output      │          │
│  │  (atomic decision)    │          │
│  └───────────────────────┘          │
│                                     │
│  Properties:                        │
│  - All sites work continuously     │
│  - Higher detection latency         │
│  - Better resource utilization     │
│  - Synchronization required        │
│                                     │
└─────────────────────────────────────┘
```

### Configuration Details

```c
// Online mode configuration
sapi_cluster_config_t cluster = {
    .mode = SAPI_CLUSTER_ONLINE,    // Active-Active
    .node_count = 2,                // 2 sites
    .synchronization_period_ms = 50, // Sync every 50ms
    .checkpoint_timeout_ms = 200    // Checkpoint barrier timeout
};

sapi_cluster_t cluster_hdl;
sapi_cluster_create(&cluster_hdl, &cluster);
```

### Safety Notes
- ✅ High availability (all nodes active)
- ✅ Good utilization (all resources used)
- ❌ Requires synchronization (network latency sensitive)
- ✅ Detects failures quickly (both nodes must respond)

---

## Configuration 6: Hot Standby Mode (Active-Passive Cluster)

### Use Case
- **Scenario:** Multi-site ERTMS RBC with primary-backup pattern
- **Availability:** Primary active, backup replicates state
- **Example:** Primary RBC + Backup RBC with automatic failover

### Architecture

```
┌────────────────────────────────────┐
│  Hot Standby (Active-Passive)      │
│  Primary processing, Backup ready  │
├────────────────────────────────────┤
│                                    │
│  ┌──────────┐      ┌──────────┐   │
│  │ Primary  │      │ Backup   │   │
│  │ (ACTIVE) │      │ (READY)  │   │
│  ├──────────┤      ├──────────┤   │
│  │ Process  │      │ Replicate│   │
│  │ Signal X │      │ State    │   │
│  │ Result:  │      │ received │   │
│  │ GREEN    │      │ Standby  │   │
│  └────┬─────┘      └────┬─────┘   │
│       │ Heartbeat        │ Ack     │
│       ├───────────────→  │         │
│       │ State Update     ←│         │
│       │ (synchronous)    │         │
│       │                  │         │
│  ┌────▼──────────────────▼───┐    │
│  │  Output (from Primary)    │    │
│  │  (only if Backup in sync) │    │
│  └──────────────────────────┘    │
│                                    │
│  If Primary fails:                │
│  ├─ Heartbeat missed              │
│  ├─ Backup detects loss            │
│  ├─ Backup takes over             │
│  └─ Backup becomes new Primary    │
│                                    │
│  Properties:                       │
│  - Lower latency (1 active)       │
│  - Redundant state (both aware)   │
│  - Failover time < 1s              │
│  - Backup ready (hot standby)     │
│                                    │
└────────────────────────────────────┘
```

### Configuration Details

```c
// Hot standby configuration
sapi_cluster_config_t cluster = {
    .mode = SAPI_CLUSTER_HOT_STANDBY,  // Active-Passive
    .primary_node_id = 0,              // Node A is primary
    .backup_node_id = 1,               // Node B is backup
    .heartbeat_period_ms = 100,        // Heartbeat every 100ms
    .heartbeat_timeout_ms = 500        // Failover on 500ms timeout
};

sapi_cluster_t cluster_hdl;
sapi_cluster_create(&cluster_hdl, &cluster);
```

### Failover Sequence

```
State 1: NORMAL
├─ Primary: processing, sending heartbeat
├─ Backup: replicating, receiving heartbeat
└─ Output: from Primary only

State 2: PRIMARY FAILURE DETECTED
├─ Backup: no heartbeat for 500ms
├─ Backup: initiates failover
└─ Output: temporarily halted

State 3: FAILOVER IN PROGRESS
├─ Backup: becomes new Primary
├─ Backup: starts processing
└─ Old Primary: isolated

State 4: NEW NORMAL
├─ Old Backup (now Primary): processing
├─ Old Primary (now Backup): offline or rejoining
└─ Output: from new Primary
```

### Safety Notes
- ✅ Lower latency (primary-only processing)
- ✅ Automatic failover (fast recovery)
- ✅ Backup keeps state (hot standby = ready)
- ❌ Single point of failure during failover (brief gap)
- ✅ Good for SIL 4 systems with strict latency

---

## Configuration 7: Heterogeneous Systems (Mixed Processors)

### Use Case
- **Scenario:** Systems with different processor types, architectures
- **Challenge:** Ensuring equivalent processing across different HW
- **Example:** ARM + x86 dual-channel system

### Architecture

```
┌──────────────────────────────────┐
│  Heterogeneous 2oo2              │
│  (Different Processors)          │
├──────────────────────────────────┤
│                                  │
│  Processor A (ARM)   Processor B (x86)
│  ┌──────────────┐   ┌──────────────┐│
│  │ RBC Logic A  │   │ RBC Logic B  ││
│  │ (ARM binary) │   │ (x86 binary) ││
│  │              │   │              ││
│  │ Process      │   │ Process      ││
│  │ Signal X     │   │ Signal X     ││
│  │              │   │              ││
│  │ Result: 42   │   │ Result: 42   ││
│  └────┬─────────┘   └────┬─────────┘│
│       │ Network          │           │
│       │ Serialized Data  │           │
│       └────┬─────────────┘           │
│            │                         │
│       ┌────▼──────────────┐          │
│       │ Voting            │          │
│       │ (compare values)  │          │
│       └────┬──────────────┘          │
│            │                         │
│       ┌────▼──────────────┐          │
│       │ Output (if agree) │          │
│       └───────────────────┘          │
│                                  │
│  Challenges:                         │
│  ✓ Endianness (SAPI handles it)  │
│  ✓ Floating point (avoid, use int) │
│  ✓ Timing (clock sync needed)    │
│  ✓ Compilation (verify bitwise)  │
│                                  │
└──────────────────────────────────┘
```

### Configuration Details

```c
// Heterogeneous system (ARM + x86)
sapi_ipc_config_t ipc_config = {
    .endianness_aware = true,      // Handle endian conversion
    .serialization = SAPI_BINARY,  // Binary protocol (portable)
    .network_protocol = ETHERNET   // Ethernet communication
};

// Ensure identical logic on both
// Key: Use fixed-width types (uint32_t, not int)
// Key: No floating point (use integer math)
// Key: SAPI's endian-safe buffer access
```

### Safety Notes
- ✅ SAPI handles endianness conversion
- ✅ Network-based communication works
- ❌ Clock synchronization required
- ❌ Compilation verification needed (bitwise comparison)
- ✅ Supported with care and testing

---

## Configuration 8: Voting Topology - Centralized Voter

### Use Case
- **Scenario:** Dedicated voter processor or central coordination
- **Advantage:** Decouples voting from application logic
- **Example:** ERTMS with central voting unit

### Architecture

```
┌──────────────────────────────────────┐
│  Centralized Voter Topology          │
├──────────────────────────────────────┤
│                                      │
│  Application Nodes        Voter Node │
│                                      │
│  ┌──────────┐            ┌────────┐│
│  │ Node A   │            │ Voter  ││
│  │ (RBC)    │            │ (Vote) ││
│  │          │            │        ││
│  │ Result A │────┐       │        ││
│  └──────────┘    │       │        ││
│                  ├──────→│        ││
│  ┌──────────┐    │       │        ││
│  │ Node B   │    │       │ Compare││
│  │ (RBC)    │    │       │ Majority
│  │          │    │       │        ││
│  │ Result B │────┤       │        ││
│  └──────────┘    │       │        ││
│                  ├──────→│        ││
│  ┌──────────┐    │       │        ││
│  │ Node C   │    │       │        ││
│  │ (RBC)    │    │       │ Decide ││
│  │          │    │       │        ││
│  │ Result C │────┘       │        ││
│  └──────────┘            │        ││
│                          │        ││
│                   ┌──────▼─────┐ │
│                   │ Output or  │ │
│                   │ Safe-State │ │
│                   └────────────┘ │
│                                      │
│  Advantages:                         │
│  ✓ Separates voting from processing │
│  ✓ Simpler application logic        │
│  ✓ Centralized policy              │
│  ✓ Easier debugging                 │
│                                      │
│  Disadvantages:                      │
│  ✗ Voter becomes single point       │
│  ✗ Network latency critical        │
│  ✗ Voter must be certified         │
│                                      │
└──────────────────────────────────────┘
```

### Configuration Details

```c
// Centralized voter configuration
sapi_voting_config_t vote_config = {
    .topology = SAPI_VOTER_CENTRALIZED,
    .voter_node_id = VOTER_CPU,
    .application_nodes = {NODE_A, NODE_B, NODE_C},
    .node_count = 3,
    .collection_timeout_ms = 100
};
```

---

## Configuration 9: Voting Topology - Distributed Cross-Comparison

### Use Case
- **Scenario:** Peer-to-peer voting, no central point
- **Advantage:** No single voter, gossip consensus
- **Example:** ERTMS with decentralized RBC nodes

### Architecture

```
┌──────────────────────────────────────┐
│  Distributed Voting (Gossip)         │
│  Peer-to-Peer Consensus             │
├──────────────────────────────────────┤
│                                      │
│  ┌──────────┐  ┌──────────┐         │
│  │ Node A   │  │ Node B   │         │
│  │ (RBC)    │  │ (RBC)    │         │
│  │ Result A │  │ Result B │         │
│  └────┬─────┘  └────┬─────┘         │
│       │             │                │
│       │ Exchange    │                │
│       ├────────────→│                │
│       │             │                │
│  ┌────▼─────┐  ┌────▼─────┐         │
│  │ A knows: │  │ B knows: │         │
│  │ A=X, B=X │  │ A=X, B=X │         │
│  └────┬─────┘  └────┬─────┘         │
│       │             │                │
│  ┌────▼─────────────▼────┐          │
│  │  Node C (latecomer)  │          │
│  │  Result C: Y         │          │
│  └────┬──────────────────┘          │
│       │ Request A,B results         │
│       │                              │
│  ┌────▼──────────────────┐          │
│  │ C learns: A=X, B=X   │          │
│  │ but C=Y (disagree)    │          │
│  │ → C is faulty         │          │
│  └───────────────────────┘          │
│                                      │
│  Message Exchange (Gossip):          │
│  Round 1: A→B (A's result)          │
│  Round 2: B→A (B's result)          │
│  Round 3: C→A,B (C's result)        │
│  Round 4: A,B→C (consensus)         │
│                                      │
│  Advantages:                         │
│  ✓ No single point of failure        │
│  ✓ Peer-to-peer trust               │
│  ✓ Byzantine-fault tolerant         │
│                                      │
│  Disadvantages:                      │
│  ✗ Higher latency (gossip delay)   │
│  ✗ More complex protocol            │
│  ✗ Network bandwidth overhead      │
│                                      │
└──────────────────────────────────────┘
```

### Configuration Details

```c
// Distributed voting configuration
sapi_voting_config_t vote_config = {
    .topology = SAPI_VOTER_DISTRIBUTED,
    .gossip_rounds = 3,            // 3 message rounds
    .gossip_timeout_ms = 150,      // Wait 150ms for convergence
    .byzantine_tolerance = true    // Tolerate Byzantine faults
};
```

---

## Summary: Configuration Decision Tree

```
Starting Point: What is your use case?

├─ Development / Testing
│  └─ Configuration 1: Single System
│     (no redundancy needed)
│
├─ SIL 1-2 (Non-Critical)
│  └─ Configuration 1: Single System
│     (cost-optimized)
│
├─ SIL 3 (Safety-Related)
│  ├─ Dual-channel required
│  └─ Configuration 2: 2oo2
│     (detect single fault)
│
├─ SIL 4 (Safety-Critical)
│  ├─ Triple-channel typical
│  │
│  ├─ Single Site:
│  │  └─ Configuration 3: 2oo3
│  │     (triple-core or triple-board)
│  │
│  ├─ Multi-Site (same processing):
│  │  └─ Configuration 5: Online Mode
│  │     (all sites active, must agree)
│  │     + Voting: Centralized (Config 8)
│  │     or Distributed (Config 9)
│  │
│  └─ Multi-Site (primary+backup):
│     └─ Configuration 6: Hot Standby
│        (primary processes, backup ready)
│        + Automatic Failover
│
└─ Ultra-High Reliability (N≥4)
   └─ Configuration 4: NMR
      (N-modular redundancy)
```

---

## References

- **EN 50128:2011** — Software safety for railway systems
- **EN 50129:2018** — Functional safety management
- **EN 50126:2017** — Reliability, Availability, Maintainability
- **SAPI Architecture:** [docs/architecture/ADR-*.md]
- **Redundancy Design:** [docs/REDUNDANCY_ARCHITECTURE.md]
- **Watchdog Design:** [docs/WATCHDOG_DESIGN.md]
