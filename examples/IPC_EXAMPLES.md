# IPC Communication Patterns - Examples

This directory contains comprehensive examples demonstrating the safeAPIFramework's Inter-Process Communication (IPC) patterns.

---

## Overview

Two fundamental communication patterns for safety-critical systems:

1. **Request-Reply (RPC)** — Synchronous client-server communication
2. **Publish-Subscribe (Events)** — Asynchronous one-to-many broadcasting

---

## Pattern 1: Request-Reply (RPC)

### What It Is

Synchronous, bidirectional communication where a **client** sends a request and **blocks** (with timeout) waiting for a **reply** from the server.

```
┌────────────────────────────────────────────────┐
│ Train Controller (Client)                       │
│                                                │
│ 1. Create RPC client                          │
│ 2. rte_ipc_rr_request() {                    │
│       send query                              │
│       block waiting for reply (timeout=5s)    │
│    }                                           │
│ 3. Receive reply                              │
└────────────────┬────────────────────────────┘
                 │
    ┌────────────┴────────────┐
    │   IPC Channel            │
    │ (Bounded Queue)          │
    └────────────┬────────────┘
                 │
┌────────────────▼────────────────────────────┐
│ Signal Database (Server)                    │
│                                             │
│ 1. Create RPC server                        │
│ 2. rte_ipc_rr_receive_request() {         │
│       block waiting for query (timeout=5s)  │
│    }                                         │
│ 3. Process query (deterministic)            │
│ 4. rte_ipc_rr_send_reply()                 │
│    (sends back to client)                   │
└─────────────────────────────────────────────┘
```

### Example: `ipc-request-reply-example.c`

**Scenario:** Train controller queries signal database

- **Server:** Signal database replies with current signal state and speed limits
- **Client:** Train controller blocks until reply arrives (or timeout)

**Message Types:**
```c
// Query from client
typedef struct {
    uint32_t train_id;      // Which train
    uint32_t location_m;    // Where train is
} signal_query_t;

// Reply from server
typedef struct {
    rte_status_t status;
    uint8_t signal_state;   // RED=0, YELLOW=1, GREEN=2
    uint32_t track_speed_limit;
} signal_reply_t;
```

**Usage:**

```bash
# Terminal 1 (Server)
./ipc-request-reply-example server

# Terminal 2 (Client) - in another terminal
./ipc-request-reply-example client
```

**Safety Properties:**
- ✓ Deadlock-free (timeout prevents indefinite block)
- ✓ No message loss (synchronous, guaranteed delivery)
- ✓ Type-safe (fixed message sizes)
- ✓ Error handling (explicit status codes)

**When to Use Request-Reply:**
- ✓ Commands that need confirmation (e.g., "turn on brake")
- ✓ Queries that need immediate answers (e.g., "what's my speed limit?")
- ✓ Synchronous RPC patterns
- ✓ Critical operations requiring proof of delivery

**When NOT to Use:**
- ✗ One-to-many communication (use Pub-Sub instead)
- ✗ Asynchronous events (use Pub-Sub instead)
- ✗ Broadcast messages (use Pub-Sub instead)

---

## Pattern 2: Publish-Subscribe (Events)

### What It Is

Asynchronous, one-to-many communication where a **publisher** sends messages to **multiple independent subscribers** who all receive the same message.

```
┌──────────────────────────────────┐
│ Track Database (Publisher)        │
│ rte_ipc_pubsub_publish()         │
│ (doesn't wait for replies)        │
└──────────────┬───────────────────┘
               │
    ┌──────────┴──────────┐
    │  Topic: track_status│
    │  (broadcast bus)    │
    └──────┬──────┬──────┬┘
           │      │      │
      ┌────▼──┐  │   ┌──▼────┐
      │ Queue │  │   │ Queue  │
      └────┬──┘  │   └──┬─────┘
           │      │      │
    ┌──────▼──┐  │   ┌──▼─────┐
    │ Signal  │  │   │ Speed   │   ... Route Manager
    │ Manager │  │   │ Manager │
    │ recv()  │  │   │ recv()  │
    └─────────┘  │   └─────────┘
          Subscriber 1    Subscriber 2    Subscriber 3
```

### Example: `ipc-pubsub-example.c`

**Scenario:** Track database publishes status changes; multiple managers react

- **Publisher:** Track database announces status changes
- **Subscribers:** Signal Manager, Speed Manager, Route Manager each react independently

**Message Type:**
```c
typedef struct {
    uint32_t track_id;      // Which track
    uint8_t occupancy;      // EMPTY=0, OCCUPIED=1
    uint32_t speed_limit;   // Current limit
} track_status_t;
```

**Architecture:**
```
Track Status Change
    ↓ publish()
    ├→ Signal Manager: "Set signal based on occupancy"
    ├→ Speed Manager: "Update speed limit"
    └→ Route Manager: "Replan routes"
```

**Usage:**

```bash
./ipc-pubsub-example

# Output shows:
# [Publisher] Publishing track 1 status: Clear
# [Signal Manager] Track 1: Setting signal to GREEN
# [Speed Manager] Track 1: Speed limit = 100 km/h
# [Route Manager] Track 1 available - planning route
```

**Safety Properties:**
- ✓ Decoupled (publisher doesn't know about subscribers)
- ✓ Scalable (add more subscribers without modifying publisher)
- ✓ Event-driven (subscribers wake when message arrives)
- ✓ Asynchronous (publisher doesn't wait)
- ✓ Type-safe (fixed message sizes)

**When to Use Publish-Subscribe:**
- ✓ Broadcasting events (status changes, alarms)
- ✓ One-to-many communication
- ✓ Decoupling components
- ✓ Asynchronous notification
- ✓ Event-driven architecture

**When NOT to Use:**
- ✗ Synchronous operations needing immediate reply (use Request-Reply)
- ✗ Point-to-point communication (use base IPC send/receive)
- ✗ Priority or order-sensitive messages (consider priority queues)

---

## Comparison

| Aspect | Request-Reply | Publish-Subscribe |
|--------|---------------|--------------------|
| **Direction** | Bidirectional | Unidirectional (broadcast) |
| **Synchronization** | Synchronous (blocks) | Asynchronous (non-blocking) |
| **Participants** | 1 client, 1 server | 1 publisher, N subscribers |
| **Coupling** | Tight (client knows server) | Loose (decoupled) |
| **Timeout** | Required (prevents deadlock) | Optional (just wait) |
| **Reply** | Required by protocol | No reply needed |
| **Scalability** | 1:1 only | 1:many naturally |
| **Use Case** | Commands, queries, RPC | Events, notifications, broadcasts |

---

## Building Examples

### Using Build Script

From the project root:

```bash
./examples/build-linux-native.sh
```

This builds the framework and all examples.

### Manual Build

```bash
# Build framework
mkdir -p build/linux
cd build/linux
cmake -S . -B .
cmake --build .
cd ../..

# Build examples (manually)
gcc -I./include -L./build/linux/src/ipc \
    examples/ipc-request-reply-example.c \
    build/linux/src/ipc/rte_ipc_request_reply.c \
    build/linux/src/log/rte_log.c \
    ... (other modules) \
    -lpthread -o ipc-request-reply-example

gcc -I./include -L./build/linux/src/ipc \
    examples/ipc-pubsub-example.c \
    build/linux/src/ipc/rte_ipc_pubsub.c \
    build/linux/src/log/rte_log.c \
    ... (other modules) \
    -lpthread -o ipc-pubsub-example
```

---

## Real-World Railway Scenario

### Architecture

```
┌─────────────────────────────────────────────────────┐
│                    RBC (ERTMS)                      │
├─────────────────────────────────────────────────────┤
│                                                     │
│  Track Manager          Train Controller            │
│  (publishes status)     (queries signals)           │
│      │                         │                    │
│      ├→ publish()              └→ request()         │
│      │   (status change)            (blocks)       │
│      │                               (waits)       │
│      └───────────┬───────────────────┘             │
│                  │                                  │
│         ┌────────┴─────────┐                       │
│         │                  │                        │
│     Signal Manager     Speed Manager                │
│     (subscribe)        (subscribe)                  │
│     recv()             recv()                       │
│     (async)            (async)                      │
│                                                     │
└─────────────────────────────────────────────────────┘
```

### Message Flow

**Scenario:** Train approaches signal; status changes

```
1. Track Manager publishes status update
   "Track 5 is now OCCUPIED, speed limit 40"

2. Signal Manager receives event
   → Updates signal to YELLOW/RED

3. Speed Manager receives same event
   → Updates speed enforcer to 40 km/h

4. Train Controller (at same time)
   → Sends RPC query: "What's my speed limit?"
   → Signal Database replies synchronously: "40 km/h"
   → Train applies brakes
```

**Key Insight:** Pub-Sub for asynchronous notifications, Request-Reply for synchronous queries.

---

## API Reference

### Request-Reply (RPC)

```c
// Server side
rte_ipc_rr_server_create(&server, &config);
rte_ipc_rr_receive_request(&server, &request, timeout_ms);
rte_ipc_rr_send_reply(&server, request.request_id, &reply, size);
rte_ipc_rr_server_destroy(&server);

// Client side
rte_ipc_rr_client_create(&client, &config);
rte_ipc_rr_request(&client, &req, req_size, &reply, reply_size, timeout_ms);
rte_ipc_rr_client_destroy(&client);
```

### Publish-Subscribe

```c
// Setup (initialization time)
rte_ipc_pubsub_topic_create(&topic, &topic_config);

// Subscribers register
rte_ipc_pubsub_subscribe(&sub, &sub_config);

// Publisher publishes
rte_ipc_pubsub_publish(&topic, &message, size);

// Subscribers receive
rte_ipc_pubsub_receive(&subscriber, &msg, size, timeout_ms);

// Cleanup
rte_ipc_pubsub_unsubscribe(&subscriber);
rte_ipc_pubsub_topic_destroy(&topic);
```

---

## Testing Checklist

- [ ] Request-Reply: Client successfully sends request
- [ ] Request-Reply: Server receives and processes request
- [ ] Request-Reply: Server sends reply
- [ ] Request-Reply: Client receives reply with correct data
- [ ] Request-Reply: Timeout works when server doesn't reply
- [ ] Pub-Sub: Publisher creates topic
- [ ] Pub-Sub: Multiple subscribers register
- [ ] Pub-Sub: Publisher sends message
- [ ] Pub-Sub: All subscribers receive same message
- [ ] Pub-Sub: Message order preserved
- [ ] Pub-Sub: Subscriber timeout works
- [ ] Safety: No deadlocks (timeouts prevent hanging)
- [ ] Safety: No message loss (explicit errors on failure)
- [ ] Safety: Type safety (fixed message sizes validated)
- [ ] Performance: Low latency (<10ms in typical case)
- [ ] Performance: No unbounded memory growth

---

## References

- safeapi_ipc_request_reply.h — Full API documentation
- safeapi_ipc_pubsub.h — Full API documentation
- [FEATURE_EXPANSION.md](../docs/FEATURE_EXPANSION.md) — Design details and rationale
- [ROADMAP.md](../docs/ROADMAP.md) — Implementation timeline
