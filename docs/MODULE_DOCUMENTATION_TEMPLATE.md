/**
 * @page module_documentation_template Module Documentation Template
 *
 * @section overview How to Create Module Documentation
 *
 * Each module should have TWO documentation files:
 * 1. **USER_GUIDE.md** - "How do I USE this module?" (practical examples)
 * 2. **ARCHITECTURE.md** - "How does it WORK internally?" (design decisions)
 *
 * Place both files in `include/safeapi/MODULE/`
 *
 * @section user_guide_template USER_GUIDE.md Template
 *
 * ```markdown
 * /**
 *  * @page MODULE_user_guide MODULE - User Guide
 *  *
 *  * @section overview What is the MODULE?
 *  * Brief explanation of what the module does and when to use it.
 *  *
 *  * @section quick_start Quick Start
 *  *
 *  * ### 1. Include Header
 *  * #include "safeapi/MODULE/rte_MODULE.h"
 *  *
 *  * ### 2. Initialize
 *  * Code example of initialization
 *  *
 *  * ### 3. Use
 *  * Code example of typical usage
 *  *
 *  * ### 4. Cleanup
 *  * Code example of cleanup
 *  *
 *  * @section examples Practical Examples
 *  *
 *  * ### Example 1: [Common Use Case]
 *  * Complete working code example
 *  *
 *  * ### Example 2: [Another Use Case]
 *  * Complete working code example
 *  *
 *  * @section common_patterns Common Patterns
 *  *
 *  * Pattern 1: [Describe pattern]
 *  * Pattern 2: [Describe pattern]
 *  *
 *  * @section guidelines Best Practices
 *  *
 *  * 1. [Do this...]
 *  * 2. [Don't do that...]
 *  * 3. [Always remember...]
 *  *
 *  * @section see_also See Also
 *  * - @ref MODULE_architecture for internal design
 *  * - @ref related_module_user_guide
 *  */
 * ```
 *
 * @section architecture_template ARCHITECTURE.md Template
 *
 * ```markdown
 * /**
 *  * @page MODULE_architecture MODULE - Architecture
 *  *
 *  * @section overview Design Overview
 *  * Explain the core design philosophy and key decisions.
 *  *
 *  * @section components Key Components
 *  *
 *  * ### Component A
 *  * What it does and why
 *  *
 *  * ### Component B
 *  * What it does and why
 *  *
 *  * @section state_management State Management
 *  * How does the module manage state?
 *  * What are the state transitions?
 *  * Draw a state diagram if applicable
 *  *
 *  * @section data_structures Data Structures
 *  *
 *  * ### Structure A
 *  * Purpose and field descriptions
 *  *
 *  * ### Structure B
 *  * Purpose and field descriptions
 *  *
 *  * @section algorithms Key Algorithms
 *  *
 *  * ### Algorithm 1: [Name]
 *  * How it works, complexity analysis
 *  *
 *  * ### Algorithm 2: [Name]
 *  * How it works, complexity analysis
 *  *
 *  * @section resource_management Resource Management
 *  * How are resources allocated?
 *  * Are there any limits?
 *  * How is cleanup handled?
 *  *
 *  * @section error_handling Error Handling
 *  * What errors can occur?
 *  * How are they reported?
 *  * How should callers handle errors?
 *  *
 *  * @section performance Performance Characteristics
 *  *
 *  * | Operation | Time | Space | Notes |
 *  * |-----------|------|-------|-------|
 *  * | init() | O(n) | O(1) | Linear in config size |
 *  * | operation() | O(1) | O(1) | Constant time |
 *  *
 *  * @section concurrency Concurrency & Thread-Safety
 *  * Is this module thread-safe?
 *  * What synchronization is used?
 *  * Any lock ordering requirements?
 *  *
 *  * @section misra MISRA C:2012 Compliance
 *  * Which rules are relevant?
 *  * How are they satisfied?
 *  * Any deviations?
 *  *
 *  * @section testing Testing Strategy
 *  * What are the test categories?
 *  * What edge cases are covered?
 *  * Any known limitations in testing?
 *  *
 *  * @section future Future Extensions
 *  * What could be added?
 *  * Any architectural limits?
 *  * Reserved code space for extensions?
 *  */
 * ```
 *
 * @section sections_explained Section Explanations
 *
 * ### USER_GUIDE Sections
 *
 * **overview** - What is this module? When would I use it? One sentence that hooks the reader.
 *
 * **quick_start** - Get me started in 2 minutes. Include: headers, initialization, usage, cleanup.
 *
 * **examples** - Real, working code. At least 2 examples showing different use cases.
 *
 * **common_patterns** - What are the idiomatic ways to use this module?
 *
 * **guidelines** - Do's and don'ts. Pitfalls to avoid. Best practices.
 *
 * ### ARCHITECTURE Sections
 *
 * **overview** - The big picture. Why does this module exist? What problem does it solve?
 *
 * **components** - What are the internal building blocks? How do they fit together?
 *
 * **state_management** - State machines, initialization order, cleanup sequences.
 *
 * **data_structures** - Internal structures, field purposes, invariants.
 *
 * **algorithms** - Core algorithms, complexity analysis, trade-offs.
 *
 * **resource_management** - Memory, file handles, network connections. Limits and cleanup.
 *
 * **error_handling** - What can go wrong? How is it detected? How is it reported?
 *
 * **performance** - Complexity characteristics, benchmarks if available.
 *
 * **concurrency** - Thread-safety, synchronization mechanisms, potential race conditions.
 *
 * **misra** - MISRA compliance, specific rules addressed or deviations.
 *
 * **testing** - Test strategy, coverage, edge cases, known limitations.
 *
 * **future** - Extensibility points, planned additions, reserved code space.
 *
 * @section guidelines Writing Guidelines
 *
 * 1. **Use Doxygen syntax** - All docs use `@page`, `@section`, `@code`, etc.
 *
 * 2. **Concrete examples** - Show working code, not pseudocode.
 *
 * 3. **Explain the WHY** - Not just WHAT. Why did we design it this way?
 *
 * 4. **Be specific** - "Supports 16 timers" is better than "many timers".
 *
 * 5. **Include diagrams** - State machines, architecture blocks, data structures.
 *
 * 6. **Link to related docs** - Use `@ref` for cross-references.
 *
 * 7. **Be honest about limitations** - "Not thread-safe" is better than silence.
 *
 * 8. **Provide rationale** - Why this design choice over alternatives?
 *
 * @section checklist Creation Checklist
 *
 * For each module:
 *
 * ```
 * USER_GUIDE.md:
 * ☐ Overview (what is it, when to use)
 * ☐ Quick start (2-minute introduction)
 * ☐ At least 2 practical examples
 * ☐ Common patterns (idiomatic usage)
 * ☐ Best practices and guidelines
 * ☐ Cross-references to related modules
 *
 * ARCHITECTURE.md:
 * ☐ Design overview (big picture)
 * ☐ Key components (building blocks)
 * ☐ State management (if applicable)
 * ☐ Data structures (internal representations)
 * ☐ Algorithms (core logic)
 * ☐ Resource management (allocation/cleanup)
 * ☐ Error handling (what can fail, how to handle)
 * ☐ Performance characteristics (complexity, benchmarks)
 * ☐ Concurrency & thread-safety
 * ☐ MISRA C:2012 compliance
 * ☐ Testing strategy
 * ☐ Future extensions and limitations
 * ```
 *
 * @section ordering Documentation Order
 *
 * When documenting modules, start with:
 *
 * 1. **Foundation modules** (used by everything):
 *    - status ✓ (done)
 *    - types
 *    - memory
 *
 * 2. **Utility modules** (support code):
 *    - buffer
 *    - string
 *    - cast
 *    - checksum
 *
 * 3. **Service modules** (provide services):
 *    - timer
 *    - log
 *    - nvm
 *    - task
 *
 * 4. **Safety modules** (critical):
 *    - safestate ✓ (should prioritize)
 *    - watchdog
 *    - reboot
 *
 * 5. **High-level modules** (built on others):
 *    - vital_channel ✓ (done)
 *    - appmanager ✓ (done)
 *    - ipc ✓ (done)
 *
 */
