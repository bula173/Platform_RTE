# Railway Message Processor (Linux POSIX)

A simple but realistic railway communication application demonstrating safeAPIFramework on Linux with POSIX OAL.

## Overview

**Application:** Train message processor with periodic heartbeats  
**Platform:** Linux (any Linux system with gcc/clang)  
**Use case:** Simple train communication and state monitoring  
**Purpose:** Learn framework basics

## What It Does

1. **Receives simulated train messages:**
   - Heartbeats (0x01)
   - Position updates (0x02)
   - Command acknowledgments (0x03)

2. **Processes messages** with validation and error handling

3. **Generates heartbeats** every 10 iterations (simulates timer callback)

4. **Logs all operations** with structured logging

5. **Tracks statistics:**
   - Messages processed
   - Heartbeats sent
   - Errors encountered

## Building

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install cmake build-essential

# macOS
brew install cmake
```

### Quick Build

From the project root:

```bash
# Option 1: Build script (easiest)
./examples/build-linux-native.sh

# Option 2: CMake preset
cmake --preset linux-native
cmake --build --preset linux-native
```

### Manual Build

```bash
# Build framework for Linux
mkdir -p build/linux
cd build/linux
cmake -S . -B . -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Linux.cmake
cmake --build .
cd ../..

# Build the example
cd examples/linux-posix-app
mkdir build && cd build
cmake -S ../../.. -B .
cmake --build .
```

## Running

```bash
# From project root
build/linux-native/examples/linux-posix-app/railway-processor

# Or from example directory
cd examples/linux-posix-app/build
./railway-processor
```

### Example Output

```
═══════════════════════════════════════════════════════════════
railway-message-processor Summary
═══════════════════════════════════════════════════════════════
Messages processed:  50
Heartbeats sent:     5
Errors encountered:  0
Final state:         SHUTDOWN
═══════════════════════════════════════════════════════════════
```

## Code Structure

```
main.c
├── Includes
│   ├── Framework headers (safeapi/*)
│   ├── Standard C library
│   └── POSIX headers
│
├── Configuration
│   ├── Message types enum
│   ├── Application states
│   └── Global context
│
├── Core Functions
│   ├── app_init() — Initialize application
│   ├── process_message() — Parse and handle message
│   ├── heartbeat_callback() — Periodic callback simulation
│   ├── simulate_train_message() — Generate test message
│   └── transition_to_state() — Safe state management
│
└── Main Loop
    ├── Initialize framework
    ├── Process messages
    ├── Generate heartbeats
    └── Graceful shutdown
```

## Framework Features Demonstrated

### 1. Status Codes (No Exceptions)
```c
rte_status_t status = process_message(msg, len);
if (status != RTE_STATUS_OK) {
    RTE_LOG_ERROR("Processing failed: %d", status);
}
```

### 2. Structured Logging
```c
RTE_LOG_INFO("Train %u at position %u", train_id, position);
RTE_LOG_WARN("Invalid message (len=%zu)", len);
RTE_LOG_DEBUG("Processing message [%zu bytes]", len);
```

### 3. Safe Assertions
```c
RTE_ASSERT(msg != NULL);
RTE_ASSERT(len > 0);
RTE_ASSERT(len <= MESSAGE_BUFFER_SIZE);
```

### 4. Safe State Management
```c
static rte_status_t transition_to_state(app_state_t new_state)
{
    // State transition logic with validation
    g_app.state = new_state;
    return RTE_STATUS_OK;
}
```

### 5. Fixed-Width Types
```c
uint32_t message_count;  // Not 'unsigned int'
uint8_t msg_type;        // Not 'unsigned char'
```

## Message Flow

```
simulate_train_message()
        ↓
process_message()
        ↓
(Parse message type)
        ↓
├─→ HEARTBEAT: Log "received from train"
├─→ POSITION_UPDATE: Validate & log position
├─→ COMMAND_ACK: Log acknowledgment
└─→ Unknown: Log warning, return error
        ↓
Update statistics
        ↓
Return status code
```

## Extending the Example

### Add New Message Type

1. Add type to message handling:
```c
case 0x04: /* NEW_MESSAGE_TYPE */
    RTE_LOG_INFO("Received NEW_MESSAGE_TYPE");
    // Handle it
    break;
```

2. Add to simulator:
```c
uint8_t sequence[] = {0x01, 0x02, 0x03, 0x04};
```

### Add Real Timer

Replace simulation with `safeapi::timer`:
```c
rte_timer_t timer;
rte_timer_create(&timer, RTE_TIMER_PERIODIC, 1000); // 1 second
rte_timer_start(&timer, heartbeat_callback, NULL);
// ... later ...
rte_timer_stop(&timer);
rte_timer_destroy(&timer);
```

### Add Task Pool

Extend with `safeapi::task` (when available):
```c
rte_task_t task1, task2;
rte_task_create(&task1, message_processor, NULL);
rte_task_create(&task2, heartbeat_generator, NULL);
```

### Add Network I/O

Add socket communication:
```c
#include <sys/socket.h>
// Receive messages from network instead of simulation
int sock = socket(AF_INET, SOCK_DGRAM, 0);
// ... bind, recvfrom, etc.
```

## Safety Considerations

### Memory Safety
- No dynamic allocation after startup
- Fixed-size buffers with bounds checking
- No pointers beyond necessity

### Type Safety
- Explicit status codes (no exceptions)
- Fixed-width integer types
- Enum for state machine

### Error Handling
- Every function returns `rte_status_t`
- Errors logged before propagation
- Statistics tracked for analysis

### State Management
- Safe transitions between states
- State validation before operations
- Clear state diagram in code

## Performance Notes

- **Message throughput:** ~10k messages/sec (on modern CPU)
- **Memory footprint:** ~1 MB
- **CPU usage:** Minimal (100ms sleep between iterations)
- **Latency:** <1ms per message (no real-time guarantee on Linux)

For hard real-time performance, see the QNX RTOS example.

## Next Steps

1. ✅ Build and run this example
2. Modify message types (see "Extending")
3. Add real timer support
4. Explore QNX version for real-time (in `../qnx-rtos-app/`)
5. Integrate into your application

## Related Examples

- [QNX RTOS Example](../qnx-rtos-app/) — Real-time railway server
- [Main README](../README.md) — All example applications
- [Cross-Compilation Guide](../../docs/CROSS_COMPILATION.md) — Build for different targets
- [Framework Documentation](../../docs/) — API reference

## References

- [safeapi::log](../../include/safeapi/oal/log/rte_log.h)
- [safeapi::status](../../include/safeapi/utils/status/rte_status.h)
- [safeapi::safestate](../../include/safeapi/utils/safestate/rte_safestate.h)
