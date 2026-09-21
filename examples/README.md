# RteFramework Examples

This directory contains realistic example applications demonstrating the RteFramework on different platforms.

---

## Overview

### Example Applications

Two complementary railway domain applications demonstrate the framework:

1. **Linux POSIX Application** — `linux-posix-app/`
   - Railway message processor
   - Demonstrates framework usage on standard Linux with POSIX OAL
   - Fast development and testing

2. **QNX RTOS Application** — `qnx-rtos-app/`
   - Railway control server
   - Deterministic real-time operation on QNX RTOS
   - Production-ready pattern for safety-critical systems

Both use the same RteFramework core, demonstrating platform portability through the OAL abstraction.

---

## Linux POSIX Example

### Application: Railway Message Processor

**Use case:** Simple train communication handler

**Functionality:**
- Processes incoming train messages (heartbeats, position updates, commands)
- Generates periodic heartbeats
- Logs all operations with timestamps
- Tracks message and error statistics
- Graceful shutdown on Ctrl+C

**Framework Features Demonstrated:**
- `rte::status` — Return status codes instead of exceptions
- `rte::log` — Structured logging
- `rte::safestate` — Safe assertions and state checks
- `rte::timer` — Simulated periodic callbacks
- `rte::buffer` — Buffer management
- POSIX threading integration

### Building

#### Option 1: Using build script
```bash
./examples/build-linux-native.sh
```

#### Option 2: Using CMake preset
```bash
cmake --preset linux-native
cmake --build --preset linux-native
```

#### Option 3: Manual CMake
```bash
mkdir -p build/linux-examples
cd build/linux-examples
cmake -S ../../ -B . \
  -DCMAKE_TOOLCHAIN_FILE=../../cmake/Toolchain-Linux.cmake \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build .
```

### Running

After building the framework:

```bash
# Build the example
cd examples/linux-posix-app
mkdir build && cd build
cmake -S ../../.. -B . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=../../../build/linux-native

cmake --build .
./railway-processor
```

### Expected Output

```
[INFO] Initializing railway-message-processor v1.0.0
[INFO] Application initialized successfully
[INFO] Starting message processing loop (Ctrl+C to exit)
[DEBUG] Processing message [1 bytes]
[INFO] Received HEARTBEAT from train
[DEBUG] Processing message [5 bytes]
[INFO] Train position update: 1050
...
[INFO] Heartbeat #5 - Messages processed: 50, Errors: 0
...
═══════════════════════════════════════════════════════════════
railway-message-processor Summary
═══════════════════════════════════════════════════════════════
Messages processed:  50
Heartbeats sent:     5
Errors encountered:  0
Final state:         SHUTDOWN
═══════════════════════════════════════════════════════════════
```

---

## QNX RTOS Example

### Application: Railway Control Server

**Use case:** Deterministic train control server

**Functionality:**
- Receives train updates via QNX message passing
- Processes train position and generates control signals
- Sends safety-critical commands back to clients
- Deterministic message processing with guaranteed response time
- Real-time task scheduling
- Production-grade safety patterns

**Framework Features Demonstrated:**
- `rte::status` — Explicit error handling for safety-critical code
- `rte::log` — Structured logging in RTOS environment
- `rte::safestate` — Safe assertions and state verification
- QNX native message passing (`MsgSend`, `MsgReceive`)
- Deterministic scheduling
- Zero-copy message handling

**Safety Features:**
- Explicit state transitions (INIT → LISTENING → PROCESSING → SHUTDOWN)
- Message validation before processing
- Error tracking and reporting
- Safe bounds checking on position data
- Atomic message handling

### Building

#### Prerequisites

QNX Momentics IDE or QNX SDP must be installed:

```bash
# Set QNX environment
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64
```

#### Option 1: Using build script
```bash
export QNX_HOST=/path/to/qnx/host
export QNX_TARGET=/path/to/qnx/target
./examples/build-qnx.sh
```

#### Option 2: Using CMake preset
```bash
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64
cmake --preset qnx
cmake --build --preset qnx
```

#### Option 3: Manual CMake
```bash
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64

mkdir -p build/qnx-examples
cd build/qnx-examples
cmake -S ../../ -B . \
  -DCMAKE_TOOLCHAIN_FILE=../../cmake/Toolchain-QNX.cmake \
  -DQNX_HOST=$QNX_HOST \
  -DQNX_TARGET=$QNX_TARGET \
  -DRTE_BUILD_TESTS=OFF

cmake --build .
```

### Running

#### On QNX Target System

1. **Build framework for QNX** (see Building above)

2. **Deploy to target:**
   ```bash
   scp build/qnx/src/*/*.a target_user@qnx_target:/opt/rbc/lib/
   scp -r ../../include/rte target_user@qnx_target:/opt/rbc/include/
   scp build/qnx/examples/qnx-rtos-app/railway-server target_user@qnx_target:/opt/rbc/bin/
   ```

3. **Run on target:**
   ```bash
   ssh target_user@qnx_target
   /opt/rbc/bin/railway-server
   ```

4. **Test with client (on another terminal on QNX):**
   ```bash
   # Send a train update message
   echo "Train ID 1 at position 500" | /opt/rbc/bin/railway-client
   ```

#### Building a Client Application

Example QNX client to communicate with the server:

```c
#include <stdio.h>
#include <sys/neutrino.h>

int main() {
    int coid = ConnectAttach(ND_LOCAL_NODE, 0, 1, _NTO_SIDE_CHANNEL, 0);
    
    train_message_t msg = {
        .type = MSG_TYPE_TRAIN_UPDATE,
        .train_id = 1,
        .position = 500
    };
    
    cmd_reply_t reply;
    MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));
    
    printf("Signal state: %u, Speed limit: %u\n", 
           reply.signal_state, reply.track_speed_limit);
    
    ConnectDetach(coid);
    return 0;
}
```

### Expected Output (QNX)

```
═══════════════════════════════════════════════════════════════
Railway Control Server (QNX RTOS)
═══════════════════════════════════════════════════════════════
PID: 12345

[INFO] Railway Control Server starting (QNX RTOS)
[INFO] QNX channel created: 0
[INFO] Message channel ready (PID=12345, CHID=0)
[INFO] Starting QNX server loop
[DEBUG] Train update: ID=1, Position=500
[INFO] Train 1: signal=1, speed_limit=80
[INFO] Stats: received=10, processed=10, errors=0
...
[INFO] Received SHUTDOWN request
[INFO] Shutting down railway server

═══════════════════════════════════════════════════════════════
Railway Server Summary
═══════════════════════════════════════════════════════════════
Messages received:   100
Messages processed:  100
Errors:              0
═══════════════════════════════════════════════════════════════
```

---

## Key Differences: Linux vs QNX

| Aspect | Linux POSIX | QNX RTOS |
|--------|-------------|----------|
| **Timing Guarantee** | Soft real-time | Hard real-time deterministic |
| **Message Passing** | Simulated (sleep loops) | Native QNX kernel (zero-copy) |
| **Scheduling** | POSIX threads | QNX task scheduler |
| **Use Case** | Development, testing | Production, safety-critical |
| **Build Time** | Fast | Requires QNX SDK |
| **Testing** | Run on any Linux machine | Requires QNX target or emulator |

---

## Framework Features Used

### Common (Both Platforms)

- **rte::status** — Explicit error codes (no exceptions)
- **rte::log** — Structured logging
- **rte::safestate** — Safe assertions and invariant checking
- **rte::types** — Fixed-width integer types (uint32_t, etc.)

### Linux-Specific

- **rte::timer** — POSIX timers (simulated in example)
- **rte::task** — POSIX thread scheduling
- **pthreads** — Native POSIX threads

### QNX-Specific

- QNX message passing (`MsgSend`, `MsgReceive`)
- QNX channel creation (`ChannelCreate`)
- QNX deterministic scheduler
- Native `errno` handling

---

## Extending the Examples

### Add New Message Types

**Linux example:** Add case in `process_message()` function
**QNX example:** Add case in `handle_message()` function

### Add Real Timer Support

**Linux:**
```c
rte_timer_start(timer_id, 1000, heartbeat_callback, &g_app);
```

**QNX:**
Use QNX `timer_create()` with POSIX API

### Add Network Communication

**Linux:**
- Add socket communication alongside message processing

**QNX:**
- Extend with QNX socket framework
- Use QNX reliable datagram (RDG) protocol

### Add Multi-Task Coordination

**Linux:**
- Use `rte::task` for thread pools
- Implement message queue with `rte::msgqueue` (when available)

**QNX:**
- Leverage QNX priority-based preemptive scheduling
- Use pulse and signal coordination

---

## Testing

### Unit Tests

Each example should have corresponding tests:

```bash
# Build and run tests
cmake --build build --target test
ctest --test-dir build --output-on-failure
```

### Integration Testing

- **Linux:** Run on any development machine
- **QNX:** Deploy to target or use QNX emulator (Neutrino emulation)

### Safety Verification

- MISRA C:2012 compliance check
- Static analysis with `cppcheck`
- Memory safety with ASAN/UBSAN

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

---

## References

- [RteFramework Documentation](../docs/)
- [CROSS_COMPILATION.md](../docs/CROSS_COMPILATION.md)
- [QNX Documentation](https://www.qnx.com/developers/docs/)
- [POSIX Standard](https://pubs.opengroup.org/onlinepubs/9699919799/)
- [Safety Requirements (SRS)](../docs/requirements/SRS.md)

---

## Support

For issues or questions:
1. Check workflow logs: https://github.com/bula173/safeAPIFreamwork/actions
2. Review example source code comments
3. Refer to framework ADRs in `docs/architecture/`
4. Check GitHub Issues for similar problems
