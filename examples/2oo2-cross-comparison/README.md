# 2oo2 Cross-Comparison Architecture

## Overview

This example demonstrates **peer-to-peer 2oo2 voting** without a central voter process. Channels A and B communicate directly for cross-comparison, making decisions independently.

```
┌────────────────────────────────────────────────────┐
│  WEST Site - Cross-Comparison Pattern              │
├────────────────────────────────────────────────────┤
│                                                    │
│  ┌──────────────┐          ┌──────────────┐      │
│  │ Process A    │          │ Process B    │      │
│  │ (CPU0)       │          │ (CPU1)       │      │
│  │              │          │              │      │
│  │ Compute      │          │ Compute      │      │
│  │ command_a    │          │ command_b    │      │
│  │              │◄────────►│              │      │
│  │ ┌──────────┐ │ Cross    │ ┌──────────┐ │      │
│  │ │Compare? ─┼─┤Compare  ├─┤Compare? │ │      │
│  │ │MATCH?   │ │ (direct) │ │MATCH?   │ │      │
│  │ └──────────┘ │  comm   │ └──────────┘ │      │
│  │              │          │              │      │
│  │ ┌──────────┐ │          │ ┌──────────┐ │      │
│  │ │Output    │ │          │ │Output    │ │      │
│  │ │result    │ │          │ │result    │ │      │
│  │ └──────────┘ │          │ └──────────┘ │      │
│  └──────────────┘          └──────────────┘      │
│        │                          │              │
│        └──────────┬───────────────┘              │
│                   │                              │
│            ┌──────▼──────┐                      │
│            │ Process C   │                      │
│            │ (Service)   │                      │
│            │ Printf      │                      │
│            │ Results     │                      │
│            └─────────────┘                      │
│                                                    │
└────────────────────────────────────────────────────┘
```

## Architecture

### Cross-Comparison Pattern

Instead of a central voter, channels A and B:

1. **Compute** independently
2. **Exchange** results via IPC (message passing, pipes, or shared memory)
3. **Compare** each other's results
4. **Decide** together:
   - Both agree → Output command
   - Disagree → Both output safe state
5. **Notify** Process C with results

### Process Responsibilities

| Process | Role | Decision Power | Safety |
|---------|------|---|---|
| **A** | Compute + Compare | Co-decides with B | Vital |
| **B** | Compute + Compare | Co-decides with A | Vital |
| **C** | Display results | None (read-only) | Non-vital |

## Communication Flow (25ms Cycle)

```
T=0ms:   Process A & B start computation
         ┌─────────────────────────────┐
         │ Parallel computation phase  │
         │ (no communication)          │
         └─────────────────────────────┘

T=5ms:   Process A writes cmd_a to queue
         Process B writes cmd_b to queue
         ┌─────────────────────────────┐
         │ Exchange phase              │
         │ A → B (via message/shared)  │
         │ B → A (via message/shared)  │
         └─────────────────────────────┘

T=10ms:  Process A reads B's command
         Process B reads A's command
         ┌─────────────────────────────┐
         │ Comparison & decision phase │
         │ Both compare locally        │
         │ Both decide independently   │
         │ (should reach same decision)│
         └─────────────────────────────┘

T=15ms:  Both write result to shared memory
         (Should be identical if working correctly)
         ┌─────────────────────────────┐
         │ Output phase                │
         │ cmd_a_out = cmd_b_out ?     │
         │ If not → ALARM              │
         └─────────────────────────────┘

T=20ms:  Process C reads voted results
         Displays to user via printf
         ┌─────────────────────────────┐
         │ Service phase               │
         │ (non-deterministic)         │
         └─────────────────────────────┘

T=25ms:  ╔═════════════════════════════╗
         ║ Cycle complete, repeat      ║
         ╚═════════════════════════════╝
```

## Shared Memory Layout

```c
typedef struct {
    /* COMPUTATION OUTPUTS */
    train_command_t cmd_a;           /* Process A's computation */
    train_command_t cmd_b;           /* Process B's computation */
    
    /* CROSS-COMPARISON */
    train_command_t cmd_a_out;       /* A's output (after comparing with B) */
    train_command_t cmd_b_out;       /* B's output (after comparing with A) */
    
    /* SYNCHRONIZATION */
    volatile uint32_t a_ready;       /* A has computed */
    volatile uint32_t b_ready;       /* B has computed */
    volatile uint32_t a_compared;    /* A has compared with B */
    volatile uint32_t b_compared;    /* B has compared with A */
    
    /* DIAGNOSTIC */
    volatile uint8_t outputs_match;  /* A's output == B's output? */
    uint32_t mismatch_count;         /* Disagreements detected */
    
    /* SEQUENCE */
    uint32_t sequence;
    uint32_t timestamp_ms;
} shared_state_t;
```

## Decision Logic

Both A and B execute identical logic:

```c
void process_compare_and_decide(train_command_t *my_cmd,
                                 train_command_t *other_cmd,
                                 train_command_t *my_output)
{
    /* Cross-compare */
    if (my_cmd->signal_state == other_cmd->signal_state &&
        my_cmd->speed_limit == other_cmd->speed_limit &&
        my_cmd->train_id == other_cmd->train_id) {
        
        /* MATCH: Output joint command */
        *my_output = *my_cmd;  /* Both agree, use it */
        
    } else {
        
        /* MISMATCH: Both output safe state */
        *my_output = safe_state_command();
    }
}
```

**Key Property**: Both processes execute this **identical logic** independently. If they received the same input (my_cmd and other_cmd), they must produce the same output.

## Advantages Over Central Voter

| Aspect | Central Voter | Cross-Comparison |
|--------|---|---|
| **Processes** | 3 + voter (4 total) | 3 (A, B, C) |
| **Synchronization** | Simple (voter decides) | Peer agreement |
| **Single Point** | Voter can fail | No voter to fail |
| **Latency** | Voter adds delay | Direct comparison |
| **Complexity** | Simpler logic | Both A & B verify |
| **Realism** | Reference arch | Peer-to-peer systems |

## Building

```bash
cd examples/2oo2-cross-comparison
make
```

## Running

```bash
# Terminal 1: Process A
./west_process_a

# Terminal 2: Process B
./west_process_b

# Terminal 3: Process C (service)
./west_process_c
```

## Expected Output

### Process A
```
[WEST:A] Cycle 10: Computed train=101, pos=535, sig=2, speed=80
[WEST:A] Received B's command: train=101, pos=535, sig=2, speed=80
[WEST:A] Comparison: MATCH ✓
[WEST:A] Output: train=101, sig=2, speed=80
```

### Process B
```
[WEST:B] Cycle 10: Computed train=101, pos=535, sig=2, speed=80
[WEST:B] Received A's command: train=101, pos=535, sig=2, speed=80
[WEST:B] Comparison: MATCH ✓
[WEST:B] Output: train=101, sig=2, speed=80
```

### Process C (Service)
```
[WEST:C] Cycle 10: A output matches B output ✓
         Train 101: Signal=2, Speed=80 km/h
         
[WEST:C] Statistics:
         Total cycles: 10
         Matches: 10 (100%)
         Mismatches: 0
         A health: OK, B health: OK
```

## Mismatch Scenario

### When A and B Disagree

```
[WEST:A] Computed: train=101, sig=2, speed=80
[WEST:A] Received B's: train=101, sig=1, speed=40
[WEST:A] Comparison: MISMATCH ✗
[WEST:A] Output: train=101, sig=0 (RED), speed=0 (STOP) - SAFE STATE

[WEST:B] Computed: train=101, sig=1, speed=40
[WEST:B] Received A's: train=101, sig=2, speed=80
[WEST:B] Comparison: MISMATCH ✗
[WEST:B] Output: train=101, sig=0 (RED), speed=0 (STOP) - SAFE STATE

[WEST:C] Cycle 10: ALERT - Outputs match but differ from inputs!
         A computed: sig=2, speed=80
         B computed: sig=1, speed=40
         Joint output: sig=0 (SAFE_STATE), speed=0
         
         Mismatch detected & contained ✓
```

## Real-World Use Cases

### 1. Dual-Core SoC (Shared Memory)
```c
/* Core 0 (Process A) and Core 1 (Process B) on same chip */
volatile shared_state_t *shared = mmap(shared_memory_file);

/* Fast cross-comparison via cache coherency */
shared->cmd_a = my_command;
__sync_synchronize();
other_cmd = shared->cmd_b;
```

### 2. Dual-Processor System (Message Passing)
```c
/* Processor 0 (WEST) and Processor 1 (EAST) via network */
send_message(other_proc_id, my_command, sizeof(my_command));
rc = recv_message(&other_cmd, sizeof(other_cmd), timeout_ms);
```

### 3. Multi-Node Network (Distributed)
```c
/* Node A and Node B over Ethernet with redundant links */
send_via_channel1(my_command);  /* Primary link */
send_via_channel2(my_command);  /* Backup link */
other_cmd = recv_with_timeout(25ms);
```

## Safety Properties (EN 50128 SIL 4)

✓ **No Single Point of Failure**: Voter process eliminated  
✓ **Peer Verification**: Both A & B verify each other  
✓ **Deterministic Decision**: O(1) comparison, bounded timing  
✓ **Fail-Safe**: Mismatch → both output safe state  
✓ **Symmetric**: No preference for A or B  
✓ **Detectable**: Output mismatch triggers alarm  

## Cross-Comparison Logic Verification

**Theorem**: If both A and B execute identical comparison logic and receive the same commands, they **must** produce the same output.

```
Process A:
  Input: cmd_a, cmd_b
  Logic: if (cmd_a == cmd_b) output = cmd_a else output = safe
  Output: out_a

Process B:
  Input: cmd_b, cmd_a  (same, different order)
  Logic: if (cmd_b == cmd_a) output = cmd_b else output = safe
  Output: out_b

Proof:
  Case 1: cmd_a == cmd_b
    A outputs: cmd_a
    B outputs: cmd_b = cmd_a  (same)
  
  Case 2: cmd_a != cmd_b
    A outputs: safe
    B outputs: safe  (same)
  
  ∴ out_a == out_b always (if comparison works correctly)
```

## Files

- `west/process_a.c` - Computation + comparison
- `west/process_b.c` - Computation + comparison
- `west/process_c.c` - Service channel
- `Makefile` - Build configuration
- `README.md` - This document

## Next Steps

1. **Run the example** with 3 processes in separate terminals
2. **Observe cross-comparison** working deterministically
3. **Inject faults** to see safe-state activation
4. **Compare with central-voter** architecture
5. **Integrate with real IPC** (QNX msgpass, pipes, TCP)

---

**Summary**: Peer-to-peer 2oo2 without central voter:
- ✓ Reduced processes (A and B decide directly)
- ✓ Higher reliability (no voter single point of failure)
- ✓ Deterministic cross-comparison
- ✓ SIL 4 compliant voting logic
- ✓ Realistic distributed system pattern
