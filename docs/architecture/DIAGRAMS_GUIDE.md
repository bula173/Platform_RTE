# Architecture Diagrams with PlantUML & Doxygen

This guide explains how to create and embed structural and behavioral diagrams in the architecture documentation using PlantUML.

---

## Overview

**PlantUML** is a UML diagram generation tool that converts text descriptions into visual diagrams. Combined with **Doxygen**, it enables:

- ✅ Structural diagrams (component, class, deployment)
- ✅ Behavioral diagrams (sequence, state machine, activity)
- ✅ Embedded in code comments and markdown
- ✅ Auto-generated from source control
- ✅ Version-tracked alongside code

---

## Setup

### 1. Install PlantUML

```bash
# macOS
brew install plantuml

# Linux
apt-get install plantuml

# Verify
plantuml -version
```

### 2. Configure Doxygen

Update `Doxyfile` to enable PlantUML:

```
PLANTUML_JAR_PATH = /usr/local/bin/plantuml
PLANTUML_INCLUDE_PATH = .
HAVE_DOT = YES
DOT_PATH = /usr/local/bin
```

### 3. Generate Docs

```bash
doxygen Doxyfile
open html/index.html
```

---

## Diagram Types & Examples

### 1. Component Diagram (Structural)

Shows module dependencies and interactions.

**File:** `docs/architecture/diagrams/component-diagram.puml`

```plantuml
@startuml component-sapiframework
skinparam componentStyle uml2

package "RteFramework" {
    component [status] as status_mod
    component [types] as types_mod
    component [buffer] as buffer_mod
    component [cast] as cast_mod
    component [safestate] as safestate_mod
    component [string] as string_mod
    
    component [timer] as timer_mod
    component [nvm] as nvm_mod
    component [memory] as memory_mod
    component [task] as task_mod
    component [ipc] as ipc_mod
    component [log] as log_mod
    component [reboot] as reboot_mod
    
    component [redundancy] as redundancy_mod
    component [watchdog] as watchdog_mod
}

package "Application Layer" {
    component [RBC Logic] as rbc_app
}

package "OS Abstraction Layer (OAL)" {
    component [POSIX OSAdapter] as posix_osadapter
    component [QNX OSAdapter] as qnx_osadapter
}

' Dependencies
status_mod --> types_mod : uses
buffer_mod --> types_mod : uses
cast_mod --> types_mod : uses
string_mod --> buffer_mod : uses
safestate_mod --> log_mod : logs to

timer_mod --> status_mod : returns
nvm_mod --> status_mod : returns
memory_mod --> status_mod : returns
task_mod --> status_mod : returns
ipc_mod --> status_mod : returns
log_mod --> status_mod : returns
reboot_mod --> status_mod : returns

redundancy_mod --> ipc_mod : uses
redundancy_mod --> status_mod : returns

watchdog_mod --> timer_mod : uses
watchdog_mod --> safestate_mod : triggers

rbc_app --> redundancy_mod : uses
rbc_app --> watchdog_mod : uses
rbc_app --> ipc_mod : uses

redundancy_mod --> posix_osadapter : OSAdapter
redundancy_mod --> qnx_osadapter : OSAdapter

@enduml
```

**Embed in Doxygen:**

```c
/**
 * @file rte/ipc/rte_ipc.h
 * @brief Inter-Process Communication Module
 * 
 * @startuml component-sapiframework
 * [see diagrams/component-diagram.puml]
 * @enduml
 */
```

---

### 2. Sequence Diagram (Behavioral - IPC)

Shows message flow between components.

**File:** `docs/architecture/diagrams/sequence-ipc-request-reply.puml`

```plantuml
@startuml sequence-ipc-rr
participant "Train Controller\n(Client)" as client
participant "IPC Layer" as ipc
participant "Signal Database\n(Server)" as server

client ->> ipc: rte_ipc_send_request(query, 5s timeout)
note right of client: "What is signal at position 100m?"

ipc ->> server: receive_request()
note right of ipc: Route to server queue

server ->> server: process_signal_query()
note right of server: Compute signal state

server ->> ipc: rte_ipc_send_reply(reply)
note right of server: "Signal is RED, speed limit 40km/h"

ipc ->> client: reply received
note right of client: Client unblocks with reply

client ->> client: apply_brakes()

alt Timeout Scenario
    note over ipc: No reply within 5s
    ipc ->> client: TIMEOUT error
    client ->> client: trigger_safestate()
end

@enduml
```

---

### 3. Sequence Diagram (Behavioral - Redundancy)

Shows 2oo3 voting flow.

**File:** `docs/architecture/diagrams/sequence-2oo3-voting.puml`

```plantuml
@startuml sequence-2oo3-voting
participant "Site A" as siteA
participant "Voting Engine" as voter
participant "Site B" as siteB
participant "Site C" as siteC
participant "Output" as output

siteA ->> siteA: process_signal()
note right of siteA: Signal = GREEN

siteB ->> siteB: process_signal()
note right of siteB: Signal = GREEN

siteC ->> siteC: process_signal()
note right of siteC: Signal = RED (disagreement!)

siteA ->> voter: stage_output(GREEN)
siteB ->> voter: stage_output(GREEN)
siteC ->> voter: stage_output(RED)

voter ->> voter: checkpoint_barrier(200ms)
note right of voter: All sites synchronized

voter ->> voter: sync_outputs()
note right of voter: Exchange and verify

voter ->> voter: voting_2oo3()
note right of voter: 2 GREEN vs 1 RED\n→ Majority: GREEN

voter ->> voter: commit_output(GREEN)
note right of voter: All nodes commit

voter ->> output: send_output(GREEN)

note over siteC: Site C flagged as faulty\n(health monitoring)

@enduml
```

---

### 4. State Machine Diagram (Behavioral)

Shows watchdog state transitions.

**File:** `docs/architecture/diagrams/statemachine-watchdog.puml`

```plantuml
@startuml statemachine-watchdog
[*] --> CREATED

CREATED --> STOPPED: rte_watchdog_create()

STOPPED --> RUNNING: rte_watchdog_start()

RUNNING --> RUNNING: rte_watchdog_kick()\n[countdown reset]

RUNNING --> TIMEOUT: countdown == 0ms
note right of TIMEOUT: Watchdog fires!

TIMEOUT --> RECOVER: apply recovery action

RECOVER --> SAFE_STATE: action=SAFESTATE
RECOVER --> REBOOT: action=REBOOT
RECOVER --> FAILOVER: action=FAILOVER
RECOVER --> LOG_ONLY: action=LOG

SAFE_STATE --> [*]
REBOOT --> [*]
FAILOVER --> [*]
LOG_ONLY --> RUNNING: optional restart

RUNNING --> STOPPED: rte_watchdog_stop()

STOPPED --> [*]: rte_watchdog_destroy()

@enduml
```

---

### 5. Deployment Diagram (Structural)

Shows how Platform_RTE is deployed on hardware.

**File:** `docs/architecture/diagrams/deployment-2oo3-cluster.puml`

```plantuml
@startuml deployment-2oo3
artifact "RteFramework" as rte_lib

node "Site A (CPU 1)" as siteA {
    component [RBC Logic] as rbcA
    component [IPC] as ipcA
    component [Watchdog] as wdA
    component [POSIX OAL] as oalA
    database "Timer\nNVM\nTask" as osal_a
}

node "Site B (CPU 2)" as siteB {
    component [RBC Logic] as rbcB
    component [IPC] as ipcB
    component [Watchdog] as wdB
    component [POSIX OAL] as oalB
    database "Timer\nNVM\nTask" as osal_b
}

node "Site C (CPU 3)" as siteC {
    component [RBC Logic] as rbcC
    component [IPC] as ipcC
    component [Watchdog] as wdC
    component [POSIX OAL] as oalC
    database "Timer\nNVM\nTask" as osal_c
}

node "Voting Engine (Central)" as voter {
    component [Redundancy Manager] as voting
    component [Checkpoint Barrier] as ckpt
}

rte_lib --> siteA
rte_lib --> siteB
rte_lib --> siteC
rte_lib --> voter

ipcA -right-> ipcB: network
ipcB -right-> ipcC: network
ipcA -down-> voter: voting results
ipcB -down-> voter: voting results
ipcC -down-> voter: voting results

wdA -down-> osal_a: heartbeat
wdB -down-> osal_b: heartbeat
wdC -down-> osal_c: heartbeat

@enduml
```

---

### 6. Activity Diagram (Behavioral - Checkpoint Flow)

Shows checkpoint synchronization workflow.

**File:** `docs/architecture/diagrams/activity-checkpoint-barrier.puml`

```plantuml
@startuml activity-checkpoint
start
:All nodes process input
independently until checkpoint;

:Node A reaches checkpoint|
:Node B reaches checkpoint|
:Node C slow, waiting...;

if (Timeout < 200ms?) then (YES)
    :Node C reaches checkpoint;
    :All nodes synchronized ✓;
else (NO - TIMEOUT)
    :Node C faulty!;
    :Isolate Node C;
    :Continue as 2oo2 (A+B only);
endif

:All nodes exchange data;

:Perform voting/comparison;

if (Consensus reached?) then (YES)
    :Commit output;
    :Send output;
    :Success ✓;
else (NO - DISAGREEMENT)
    :Abort output;
    :Trigger safe-state;
    :Failure logged;
endif

stop

@enduml
```

---

## Embedding Diagrams in Doxygen Comments

### In Header Files

```c
/**
 * @file rte/ipc/rte_ipc_request_reply.h
 * @brief Request-Reply (RPC) IPC Pattern
 *
 * @section overview Overview
 * 
 * The request-reply pattern implements synchronous RPC communication:
 *
 * @startuml
 * participant "Client" as c
 * participant "Server" as s
 * c ->> s: send_request(query)
 * s ->> c: send_reply(answer)
 * @enduml
 *
 * @see rte_ipc_request_reply_t
 */
```

### In Markdown Files

```markdown
# Module Architecture

## Structural View

```puml
@startuml component-overview
[Component A]
[Component B]
[Component A] --> [Component B]
@enduml
```

## Behavioral View

```puml
@startuml sequence-example
Alice ->> Bob: Hello
Bob ->> Alice: Hi
@enduml
```
```

### In ADR Documents

```markdown
# ADR-007: Per-Feature Modular Architecture

## Decision

Each feature is an independent module with clear boundaries.

## Diagram

\`\`\`puml
@startuml
package "RteFramework" {
    [Module A] as modA
    [Module B] as modB
}
modA --> modB: optional dependency
\`\`\`

## Rationale

- Enables independent testing
- Reduces verification scope
- Facilitates reuse
```

---

## Diagram Locations

Organize diagrams in the repo:

```
docs/architecture/
├── diagrams/
│   ├── component-diagram.puml
│   ├── sequence-ipc-request-reply.puml
│   ├── sequence-2oo3-voting.puml
│   ├── statemachine-watchdog.puml
│   ├── deployment-2oo3-cluster.puml
│   ├── activity-checkpoint-barrier.puml
│   └── README.md (diagram index)
├── ADR-001-os-abstraction-layer.md
├── ADR-005-oal-osadapter-registration.md
└── ...
```

---

## Build Diagrams Locally

```bash
# Generate PNG from single diagram
plantuml docs/architecture/diagrams/component-diagram.puml

# Generate all diagrams in directory
plantuml docs/architecture/diagrams/*.puml

# Generate as SVG (vector, better quality)
plantuml -tsvg docs/architecture/diagrams/*.puml

# Verify syntax without generating
plantuml -checkonly docs/architecture/diagrams/component-diagram.puml
```

---

## Doxygen Configuration

Add to `Doxyfile`:

```
# PlantUML settings
PLANTUML_JAR_PATH       = /usr/local/bin/plantuml
PLANTUML_INCLUDE_PATH   = ./docs/architecture/diagrams
PLANTUML_CFG_FILE       = ./plantuml.cfg

# Diagram generation
HAVE_DOT                = YES
DOT_PATH                = /usr/local/bin
MSCGEN_PATH             = /usr/local/bin

# Image output format (PNG or SVG)
DOT_IMAGE_FORMAT        = svg
SVG_DYNAMIC_DEPTH       = 0

# Generate diagrams in documentation
GENERATE_LATEX          = YES
LATEX_BATCHMODE         = YES
```

---

## CI/CD Integration

Add to GitHub Actions workflow:

```yaml
name: Build Documentation

on: [push, pull_request]

jobs:
  docs:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies
        run: |
          sudo apt-get install -y doxygen graphviz plantuml
      
      - name: Build diagrams
        run: |
          plantuml docs/architecture/diagrams/*.puml
      
      - name: Build documentation
        run: doxygen Doxyfile
      
      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./html
```

---

## Best Practices

### 1. Diagram Scope

**Keep diagrams focused:**
- ❌ Don't show all modules in one diagram
- ✅ Show one architectural aspect per diagram
- ✅ Use multiple diagrams for complex systems

### 2. Consistency

**Use consistent notation:**
```puml
skinparam componentStyle uml2
skinparam monochrome true
```

### 3. Traceability

**Link diagrams to code:**
```
Component → rte_*.h header file → Implementation
Sequence → Test case → Documented behavior
State Machine → State enum → Code logic
```

### 4. Version Control

**Commit PlantUML source, not generated images:**
```
✅ docs/architecture/diagrams/*.puml (source)
❌ docs/architecture/diagrams/*.png (generated)
```

---

## Common Diagram Patterns for Safety-Critical Systems

### Pattern 1: Fault Tolerance

```puml
@startuml fault-tolerance
participant "Primary" as p
participant "Voter" as v
participant "Backup" as b

p ->> v: result_A
b ->> v: result_B
v ->> v: compare(A, B)
alt Agreement
    v ->> output: send(A)
else Disagreement
    v ->> safestate: trigger()
end
@enduml
```

### Pattern 2: Health Monitoring

```puml
@startuml health-monitoring
participant "Task" as t
participant "Watchdog" as w
t ->> w: kick()
w ->> w: reset_countdown()
t ->> t: do_work()
t ->> w: kick()
alt Timeout
    w ->> w: timeout_fired()
    w ->> recovery: take_action()
end
@enduml
```

### Pattern 3: Layered Architecture

```puml
@startuml layered-arch
package "Application Layer" {
    [RBC Logic]
}
package "Framework Layer (Platform_RTE)" {
    [IPC] [Timer] [Watchdog]
}
package "OAL OSAdapter Layer" {
    [POSIX] [QNX]
}
package "OS/Hardware Layer" {
    [Linux] [RTOS]
}
@enduml
```

---

## References

- **PlantUML:** https://plantuml.com/
- **Doxygen PlantUML Support:** https://doxygen.nl/manual/diagrams.html#msc
- **UML Specification:** https://www.omg.org/spec/UML/
- **SysML:** https://sysml.org/ (for systems engineering)
