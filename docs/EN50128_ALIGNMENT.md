# safeAPIFramework Alignment with EN 50128

This document explains how safeAPIFramework's architecture aligns with EN 50128:2011 (Railway applications - Software on board rolling stock - Software safety) recommendations for safety-critical software organization.

## EN 50128 Context

EN 50128 is the CENELEC (European Committee for Electrotechnical Standardization) standard for railway software safety, typically applying to **SIL 2 through SIL 4** railway systems. The **ERTMS Radio Block Centre (RBC)** — safeAPIFramework's reference application — is a SIL 4 function.

Key EN 50128 principles that shape safeAPIFramework:

1. **Layered Architecture** (EN 50128, Section 6.2.2)
2. **Clear Module Boundaries** (EN 50128, Section 6.2.3)
3. **Traceability** from requirements to code (EN 50128, Section 6.4)
4. **Static Analysis & Code Review** (EN 50128, Section 7)

---

## How safeAPIFramework Implements EN 50128

### 1. Layered Architecture (OS Abstraction Layer)

**EN 50128 requirement**: The standard recommends "layered software architecture" to separate concerns and enable independent verification of each layer.

**safeAPIFramework implementation**:
- **Application Layer** — Your RBC/railway logic (not in this repo)
- **OS Abstraction Layer (OAL)** — safeAPIFramework exports 13 services
- **OS/RTOS Layer** — Implemented by you via backend registration (ADR-005)
- **Hardware Layer** — Device drivers and BSP

This **three-tier layering** allows:
- Application code never directly calls OS APIs
- Each layer can be verified independently
- Easy retargeting to different OS/RTOS (just swap the backend)
- Clear interface contracts (pure C ABI)

See: [docs/architecture/ADR-001-os-abstraction-layer.md](ADR-001-os-abstraction-layer.md)

---

### 2. Clear Module Boundaries

**EN 50128 requirement**: "Modularity shall be used to reduce complexity and increase maintainability."

**safeAPIFramework implementation**:
- **13 independent modules** (status, timer, nvm, task, ipc, log, reboot, etc.)
- **Per-feature directory layout** (include/safeapi/X, src/X, tests/X)
- **One static library per feature** — linkable independently
- **Pure C ABI** — No language-specific coupling

Each module can be:
- Developed independently
- Unit tested in isolation
- Reviewed and verified separately
- Reused in other projects

See: [docs/architecture/ADR-007-per-feature-directory-layout.md](ADR-007-per-feature-directory-layout.md)

---

### 3. Traceability (Requirements ↔ Code)

**EN 50128 requirement**: "All requirements shall be traced to design, implementation, and test artifacts."

**safeAPIFramework implementation**:

**Requirement IDs in code:**
```c
/* REQ-OAL-TIMER-001: Every timer operation returns explicit sapi_status_t */
sapi_status_t sapi_timer_create(const char *name, sapi_timer_handle_t *out_handle) {
    if (!name || !out_handle) return SAPI_STATUS_INVALID_PARAM;
    ...
}
```

**Consolidated SRS** (`docs/requirements/SRS.md`):
- Single source of truth for all requirements
- Each REQ-ID maps to a specific requirement paragraph
- Changes to requirements update the SRS first, then code comments

**Design rationale** (`docs/architecture/ADR-*.md`):
- 7 Architecture Decision Records
- Each ADR explains the "why" behind design choices
- References EN 50128 sections and rationale

This enables:
- Impact analysis: "If we change timer behavior, which code artifacts are affected?"
- Compliance evidence: "Here's the code that implements REQ-OAL-TIMER-001"
- Audit trails: "This design decision maps to EN 50128 Section X"

---

### 4. Static Analysis & Code Review

**EN 50128 requirement**: "Static analysis tools shall be used to detect coding errors and ensure MISRA compliance."

**safeAPIFramework implementation**:

**Automated static analysis:**
- **cppcheck with MISRA C:2012 addon** (free, integrated into CMake)
- **Command:** `cmake --build build --target cppcheck`
- **Output:** MISRA rule violations with line numbers
- **Report:** `docs/MISRA_COMPLIANCE_REPORT.md`

**Mandatory coding conventions** (from CLAUDE.md, EN 50128-aligned):
- ✅ **No dynamic memory** — malloc/free/realloc banned (MISRA Rule Dir 4.12)
- ✅ **Explicit type conversions** — No bare casts (MISRA Rule 10.1-10.8)
- ✅ **Bounded operations** — strcpy → sapi_string_* (MISRA Rule 21.6)
- ✅ **Single assignment paths preferred** — Guard clauses, not deeply nested `else` (MISRA Rule 15.5)
- ✅ **No recursion** — All functions are iterative (MISRA Rule 17.2)
- ✅ **No undefined behavior** — All pointers validated before use (MISRA Rule 20.*)

**Manual code review checklist** (`docs/architecture/code-review-checklist.md`):
- Complements automated analysis
- Verifies design patterns and safety assumptions
- Ensures Doxygen documentation is complete

---

### 5. Defensive Programming & Fail-Safe Defaults

**EN 50128 requirement**: "Systems shall be designed to fail safely. Assertions and defensive checks are expected in safety-critical code."

**safeAPIFramework implementation**:

**Safe-state transitions** (`safeapi/safestate.h`):
```c
/* Enter safe state: triggers either ASSERT (debug) or REBOOT (production) */
SAPI_ASSERT(condition);        /* Fail-fast in debug builds */
SAPI_SAFESTATE(SAFE, message); /* Enter safe state, log reason */
SAPI_REBOOT(WARM);             /* Controlled reboot */
```

**Explicit error handling** (`safeapi/status.h`):
```c
typedef enum {
    SAPI_STATUS_OK,
    SAPI_STATUS_INVALID_PARAM,
    SAPI_STATUS_TIMEOUT,
    SAPI_STATUS_RESOURCE_EXHAUSTED,
    SAPI_STATUS_HARDWARE_FAULT,
    ...
} sapi_status_t;
```

Every function returns an **explicit status code**, never uses exceptions or errno.

---

## Compliance Status: SIL 3/4 Path

safeAPIFramework is designed for SIL 3/4 but requires final **certification activities**:

| Activity | Status | Required For SIL 3/4 |
|----------|--------|----------------------|
| Code structure & layering | ✅ Complete | ✅ Yes |
| Module boundaries | ✅ Complete | ✅ Yes |
| MISRA C:2012 compliance | ⚠️ Manual review | ✅ Yes (tool-driven) |
| Requirement traceability | ✅ Complete | ✅ Yes |
| Design documentation (ADRs) | ✅ Complete | ✅ Yes |
| Unit tests | ✅ 8 test modules | ✅ Yes (coverage%) |
| Static analysis automation | ⚠️ Manual cppcheck | ✅ Yes (CI integration) |
| **Functional safety qualification** | ❌ Not done | ✅ **Yes** (certification body) |

**What's needed to complete SIL 3/4 certification:**
1. Run `cppcheck --addon=misra` on all code (currently manual)
2. Integrate MISRA checking into CI/CD pipeline
3. Achieve target code coverage (% depends on SIL target)
4. Engage a **Notified Body** (CENELEC-certified lab) for functional safety qualification
5. Prepare Safety Case documentation (hazard analysis, FTA, requirements matrix)

See [MISRA_COMPLIANCE_REPORT.md](MISRA_COMPLIANCE_REPORT.md) for current status.

---

## Architecture Decisions That Reflect EN 50128

| Decision | EN 50128 Alignment | Document |
|----------|-------------------|----------|
| OS Abstraction Layer (OAL) | Section 6.2.2 (Layered architecture) | ADR-001 |
| Backend registration (pluggable) | Section 6.2.3 (Module coupling) | ADR-005 |
| Checked integer casting | Section 7 (Static analysis, type safety) | ADR-003 |
| Endian-safe buffer access | Section 6.3 (Portability & embedded concerns) | ADR-002 |
| Safe-state transitions | Section 6.5 (Fail-safe defaults) | ADR-004 |
| Per-feature modules | Section 6.2.3 (Modularity) | ADR-007 |
| Bounded string operations | Section 7 (Buffer overflows) | ADR-006 |

---

## How Downstream Projects Inherit EN 50128 Alignment

When your project **links safeAPIFramework**:

1. **You inherit** the layered architecture
2. **You inherit** the MISRA-compliant module interfaces
3. **You inherit** the requirement traceability structure
4. **You can reuse** the CMake static analysis setup

Your project's responsibilities:
- Implement your application logic using safeAPIFramework services
- Follow the same coding standards (CLAUDE.md)
- Add your own requirements traceability (build on SRS.md)
- Write your own safety case and hazard analysis
- Engage a Notified Body for certification

See [docs/INTEGRATION.md](INTEGRATION.md) for integration examples.

---

## Questions?

- **How do I verify MISRA compliance?**
  - Run: `./scripts/run-cppcheck.sh`
  - Read: `docs/MISRA_COMPLIANCE_REPORT.md`

- **What are the architecture decisions?**
  - Read: `docs/architecture/ADR-*.md`

- **How do I trace requirements to code?**
  - Source: `docs/requirements/SRS.md`
  - Search code for `REQ-ID` tags (e.g., `grep -r "REQ-OAL"`)

- **How do I integrate into my railway project?**
  - Follow: `docs/INTEGRATION.md`
  - Review: `CLAUDE.md` (coding standards)

---

## References

- **EN 50128:2011** — Railway applications - Software on board rolling stock - Software safety (CENELEC)
- **MISRA C:2012** — Guidelines for the use of the C language in critical systems
- **CENELEC EN 50129** — Railway applications - Communication, signalling and processing systems - Safety related electronic systems for signalling (Functional Safety & Technical Safety)
