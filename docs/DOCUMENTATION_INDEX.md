/**
 * @page documentation_index Documentation Index
 *
 * @section feature_documentation Feature-Specific Documentation
 *
 * Documentation is organized by feature, co-located with headers for easy discovery:
 *
 * ### IPC Module (`include/safeapi/ipc/`)
 *
 * - **IPC_GUIDE.md** - Start here for IPC overview
 *   - Architecture layers, workflow, design principles
 *   - Links to all IPC documentation
 *
 * - **IPC_TRANSPORT_SELECTION.md** - Transport options
 *   - Shared Memory, FIFO, TCP/IP, UDP characteristics
 *   - POSIX vs RTOS backend differences
 *   - Decision tree for choosing transports
 *   - Real-world scenarios (railway, automotive, web service)
 *
 * - **CHANNEL_CONFIGURATION.md** - Configuration framework
 *   - Configuration structures for each transport type
 *   - How users specify IP addresses, ports, paths
 *   - Dispatcher callback pattern for mixed transports
 *   - Online/Standby examples
 *
 * - **sapi_ipc.h** - Base IPC API (code, not documentation)
 *   - Backend vtable abstraction
 *   - OS-agnostic interface
 *
 * - **sapi_ipc_request_reply.h** - Request-Reply pattern (code)
 *   - RPC-style synchronous communication
 *   - Deadlock-free with timeouts
 *
 * - **sapi_ipc_pubsub.h** - Publish-Subscribe pattern (code)
 *   - One-to-many broadcasting
 *   - Asynchronous, decoupled
 *
 * ### Vital Channel Module (`include/safeapi/vital_channel/`)
 *
 * - **ARCHITECTURE.md** - Vital channel design
 *   - Voting logic (2oo2, 2oo3, NMR)
 *   - Transport abstraction via callbacks
 *   - Safe-state trigger on disagreement
 *   - Integration with IPC transports
 *
 * - **CHANNEL_TOPOLOGIES.md** - Redundancy patterns
 *   - Pattern 1: Single-system 2oo2 (A ↔ B → C)
 *   - Pattern 2: Distributed 2oo2 (network)
 *   - Pattern 3: Hot standby with mirroring
 *   - Pattern 4: Triple redundancy (2oo3)
 *   - Decision tree for selecting topology
 *
 * - **sapi_channel.h** - Public API (code)
 *   - init(), send(), receive(), get_health()
 *   - Voting strategies and result codes
 *
 * ### System-Wide Documentation (`docs/`)
 *
 * - **architecture/SYSTEM_OVERVIEW.md** - System layering and common-cause mitigation
 *   - Layered architecture (app manager, vital channels, IPC, OS)
 *   - Channel taxonomy (internal, inter-unit, inter-system)
 *   - Blocking semantics (vital, non-vital, async)
 *   - App Manager responsibilities
 *   - POSIX vs RTOS implementation differences
 *   - Complete railway train control example
 *
 * - **DOCUMENTATION_INDEX.md** - This file
 *   - Documentation organization
 *   - Reading paths for different roles
 *
 * @section reading_paths Reading Paths by Role
 *
 * ### For System Architects
 * 1. Start: docs/architecture/SYSTEM_OVERVIEW.md
 * 2. Then: include/safeapi/vital_channel/ARCHITECTURE.md
 * 3. Then: include/safeapi/vital_channel/CHANNEL_TOPOLOGIES.md
 * 4. Then: include/safeapi/ipc/IPC_GUIDE.md
 *
 * ### For Integrators (Implementing for Your OS)
 * 1. Start: include/safeapi/ipc/IPC_GUIDE.md
 * 2. Then: include/safeapi/ipc/CHANNEL_CONFIGURATION.md
 * 3. Then: include/safeapi/ipc/IPC_TRANSPORT_SELECTION.md
 * 4. Implement: OS-specific backends (TCP, FIFO, SHM, UDP send/recv)
 * 5. Example: examples/channel_configuration_example.c
 *
 * ### For Application Developers
 * 1. Start: include/safeapi/vital_channel/CHANNEL_TOPOLOGIES.md
 *    - Find your topology (2oo2 local, 2oo3 distributed, etc.)
 * 2. Then: include/safeapi/ipc/CHANNEL_CONFIGURATION.md
 *    - Configure your channels (IPs, ports, paths)
 * 3. Then: docs/architecture/SYSTEM_OVERVIEW.md (section on App Manager)
 *    - Implement your main loop
 * 4. Code: See examples/channel_configuration_example.c
 *
 * ### For Safety/Compliance Engineers
 * 1. Start: include/safeapi/vital_channel/ARCHITECTURE.md
 *    - Understand voting logic and safe-state
 * 2. Then: include/safeapi/vital_channel/CHANNEL_TOPOLOGIES.md
 *    - Review redundancy patterns
 * 3. Then: include/safeapi/ipc/IPC_TRANSPORT_SELECTION.md
 *    - Understand transport reliability characteristics
 * 4. Reference: docs/architecture/SYSTEM_OVERVIEW.md
 *    - Understand timeout handling and error propagation
 *
 * @section document_structure Documentation Structure
 *
 * Each module follows this structure:
 *
 * ```
 * include/safeapi/MODULE/
 *   ├─ MODULE.h              (API - code + Doxygen comments)
 *   ├─ ARCHITECTURE.md       (How it works - technical design)
 *   ├─ GUIDE.md              (Overview + navigation)
 *   └─ CONFIG.md             (How to configure)
 * ```
 *
 * This co-location ensures:
 * - ✓ Documentation stays synchronized with code
 * - ✓ Easy discovery (docs live with headers)
 * - ✓ Clear relationships between modules
 * - ✓ Doxygen can link between pages
 *
 * @section quick_links Quick Reference
 *
 * | Question | Document |
 * |----------|----------|
 * | How do channels communicate? | IPC_GUIDE.md |
 * | Which transport should I use? | IPC_TRANSPORT_SELECTION.md |
 * | How do I configure a channel? | CHANNEL_CONFIGURATION.md |
 * | How does voting work? | vital_channel/ARCHITECTURE.md |
 * | What topology fits my system? | vital_channel/CHANNEL_TOPOLOGIES.md |
 * | How do I build the app manager? | architecture/SYSTEM_OVERVIEW.md |
 * | What is POSIX vs RTOS? | architecture/SYSTEM_OVERVIEW.md + IPC_TRANSPORT_SELECTION.md |
 * | Show me working code | examples/channel_configuration_example.c |
 *
 * @section generating_html Generating HTML Documentation
 *
 * All documentation uses Doxygen format. Generate HTML:
 *
 * ```bash
 * doxygen Doxyfile
 * # Output: docs/html/index.html
 * ```
 *
 * This creates:
 * - Full API reference (all .h files)
 * - Architecture pages (all .md files)
 * - Cross-linked navigation
 * - Search functionality
 *
 * @section contributing Contributing Documentation
 *
 * When adding new modules:
 *
 * 1. Create MODULE/ARCHITECTURE.md - Technical design, how it works
 * 2. Create MODULE/GUIDE.md - Navigation and overview
 * 3. Create MODULE/CONFIGURATION.md (if applicable) - Configuration options
 * 4. Add Doxygen comments to MODULE/MODULE.h - API documentation
 * 5. Update this index with links and reading path
 *
 */
