# RteFramework Architecture Diagrams

This directory contains PlantUML diagrams for the RteFramework architecture.

## Diagram Index

### Structural Diagrams

1. **component-diagram.puml** — Module dependencies and architecture overview
   - Shows 13+ core modules and their relationships
   - Application → Framework → OAL → OS/Hardware layers

2. **deployment-2oo3-cluster.puml** — Hardware deployment (2oo3 redundancy)
   - Three RBC sites with voting engine
   - Network connections between sites
   - Watchdog and health monitoring

### Behavioral Diagrams - Configuration Sequences

**Single & Dual-Channel:**
3. **sequence-1oo1-single.puml** — Single System (Non-Redundant)
   - No redundancy, no voting
   - Fast, low overhead
   - SIL 1/2 suitable

4. **sequence-2oo2-dual.puml** — Dual-Channel (2oo2)
   - Both channels must agree
   - Any disagreement = immediate safe-state
   - Zero fault tolerance
   - SIL 3 suitable

5. **sequence-2oo2d-network.puml** — Dual-Site Network (2oo2D)
   - Network-based 2oo2 communication
   - Result exchange over network
   - Latency-sensitive (< 100ms)
   - SIL 3 suitable

**Triple-Channel & Multi-Site:**
6. **sequence-2oo3-voting.puml** — Triple-Channel (2oo3) ← [Already exists]
   - Majority vote (≥2 out of 3)
   - Tolerates 1 fault
   - Minority site flagged as faulty
   - SIL 4 suitable

7. **sequence-online-mode.puml** — Online Mode (Active-Active Cluster)
   - All sites process simultaneously
   - Checkpoint barrier synchronization
   - Data synchronization required
   - Voting before output
   - Better resource utilization
   - Higher detection latency
   - SIL 4 suitable

8. **sequence-hot-standby.puml** — Hot Standby (Active-Passive)
   - Primary active, Backup replicates state
   - Heartbeat for liveness
   - Backup ready for immediate failover
   - Lower processing latency
   - SIL 4 suitable

9. **sequence-hot-standby-failover.puml** — Hot Standby Failover
   - Primary failure detection (500ms timeout)
   - Automatic backup promotion
   - State transfer to rejoining primary
   - Failover time < 1 second
   - No output gap (backup had state)

**Scalable & Advanced:**
10. **sequence-nmr.puml** — N-Modular Redundancy (NMR)
    - 4+ channels with majority vote
    - Example: 4oo3 tolerates 1 fault
    - Example: 5oo3 tolerates 2 faults
    - Ultra-high reliability
    - SIL 4+ suitable

**Voting Topologies:**
11. **sequence-centralized-voter.puml** — Centralized Voter
    - Dedicated voter CPU/node
    - Collects results from all nodes
    - Central majority voting
    - Simpler application logic
    - Voter becomes single point of failure

12. **sequence-distributed-gossip.puml** — Distributed Gossip Voting
    - Peer-to-peer consensus
    - No central voter
    - Byzantine-fault tolerant
    - 3 gossip rounds to consensus
    - Higher latency, no single point

**Other:**
13. **statemachine-watchdog.puml** — Watchdog state machine
    - Creation, starting, running states
    - Timeout → Recovery transitions
    - Recovery actions (safe-state, reboot, failover)
    - Cleanup and destruction

14. **activity-checkpoint-barrier.puml** — Checkpoint synchronization workflow
    - All nodes reaching checkpoint with timeout
    - Timeout handling (fault detection)
    - Data exchange and voting
    - Output commit and transmission

## Building Diagrams Locally

### Generate all diagrams as PNG:
```bash
plantuml -c ../plantuml.cfg *.puml
```

### Generate all diagrams as SVG (vector):
```bash
plantuml -tsvg -c ../plantuml.cfg *.puml
```

### Generate single diagram:
```bash
plantuml -c ../plantuml.cfg component-diagram.puml
```

### Verify syntax without generating:
```bash
plantuml -checkonly -c ../plantuml.cfg component-diagram.puml
```

## Using in Doxygen

All diagrams are embedded in:
- ADR documents (docs/architecture/ADR-*.md)
- Header files (include/rte/*/rte_*.h)
- Module documentation

See [DIAGRAMS_GUIDE.dox](../DIAGRAMS_GUIDE.dox) for embedding examples.

## Updating Diagrams

1. Edit `.puml` file
2. Run `plantuml` to generate image
3. Commit both `.puml` (source) and generated image
4. Doxygen will automatically include in documentation

## PlantUML Resources

- **PlantUML Docs:** https://plantuml.com/
- **Component Diagram:** https://plantuml.com/component-diagram
- **Sequence Diagram:** https://plantuml.com/sequence-diagram
- **State Machine:** https://plantuml.com/state-diagram
- **Activity Diagram:** https://plantuml.com/activity-diagram
- **Deployment Diagram:** https://plantuml.com/deployment-diagram
