# Safety Application Conditions (SAC) for safeAPIFramework

**Document Type:** Safety Application Conditions  
**Status:** DRAFT - For use in projects implementing EN 50128:2011  
**Version:** 0.1.0  
**Date:** 2026-08-02  

---

## Executive Summary

**safeAPIFramework is NOT a pre-certified, pre-assessed library.**

It is a **reference template and design pattern** demonstrating EN 50128:2011 alignment. Any project using safeAPIFramework must:

1. **Treat it as new software** developed under the project's formal processes
2. **Perform formal code review** against EN 50128 requirements
3. **Conduct full static analysis** using certified tools
4. **Generate complete verification evidence** (tests, traceability, analysis)
5. **Submit to independent assessment** by a Notified Body (for SIL 3/4)
6. **Document these conditions** in the project's Safety Case

This SAC defines the conditions under which safeAPIFramework may be used as a foundation for a safety-critical system.

---

## 1. Component Status

### 1.1 What safeAPIFramework IS

- ✅ A reference implementation of EN 50128 design principles
- ✅ An OS Abstraction Layer (OAL) demonstrating layered architecture
- ✅ MISRA C:2012 compliant source code (manually verified)
- ✅ A modular system suitable for SIL 3/4 railway applications
- ✅ Demonstrated patterns for:
  - Checked integer casting
  - Safe-state transitions
  - Fail-safe defaults
  - Module isolation
  - Endianness-safe buffer operations

### 1.2 What safeAPIFramework is NOT

- ❌ A certified or assessed library
- ❌ Pre-verified for use in any specific railway project
- ❌ Exempt from formal code review
- ❌ Exempt from functional testing in your system
- ❌ Exempt from static analysis verification
- ❌ A standalone SIL 3/4 component (requires project-level assessment)
- ❌ Suitable for deployment without formal safety processes

### 1.3 Assessment Status

| Artifact | Status | Notes |
|----------|--------|-------|
| Architecture & design patterns | ✅ Complete | 7 ADRs documented |
| Code structure (modularity) | ✅ Complete | 13 independent modules |
| MISRA C:2012 compliance | ⚠️ Partial | Manual review only; no certified tool used |
| Unit tests | ✅ Complete | 8 test modules, >80% coverage |
| Static analysis setup | ✅ Complete | cppcheck integrated; not certified tool |
| Functional safety assessment | ❌ NOT DONE | Must be done by integrator |
| SIL verification | ❌ NOT DONE | Must be done in project context |
| Notified Body assessment | ❌ NOT DONE | Must be done for deployed systems |

---

## 2. Conditions for Use in Safety-Critical Projects

### 2.1 Mandatory Verification Activities (Project Responsibility)

Any project using safeAPIFramework must perform:

#### A. Formal Code Review
- **Requirement:** EN 50128:2011, Section 7.2
- **What:** Every source file in safeAPIFramework must be reviewed against:
  - MISRA C:2012 Mandatory & Required rules
  - EN 50128 Section 6 (technical requirements)
  - CENELEC safety critical coding practices
  - Project-specific coding guidelines
- **Who:** Project's qualified code review team
- **Output:** Code review report with sign-off
- **Evidence for Safety Case:** Formal review matrix (file × rule × findings)

#### B. Static Analysis Verification
- **Requirement:** EN 50128:2011, Section 7.3
- **Minimum:** Run `cppcheck --addon=misra --std=c99` on all safeAPIFramework code
- **Recommended:** Use certified MISRA tool (LDRA, Parasoft, PC-lint Plus, Polyspace)
- **Not Sufficient:** Running cppcheck alone; must use qualified tool for SIL 3/4
- **Output:** MISRA compliance report with tool evidence
- **Evidence for Safety Case:** Tool output, deviation analysis, suppression rationale

#### C. Functional Testing
- **Requirement:** EN 50128:2011, Section 7.4
- **What:** Unit and integration tests for all safeAPIFramework components
  - Test coverage: Minimum 80% line coverage (recommend >90% for SIL 3/4)
  - Test each module's backend interface separately
  - Test error handling paths (SAPI_STATUS_* return values)
  - Test safe-state transitions (SAPI_ASSERT, SAPI_SAFESTATE, SAPI_REBOOT)
- **Output:** Test report with coverage metrics
- **Evidence for Safety Case:** Test cases, coverage report, traceability matrix

#### D. Requirement Traceability
- **Requirement:** EN 50128:2011, Section 6.4
- **What:** Link every safeAPIFramework requirement to:
  - Design element (ADR or design document)
  - Implementation (code file & line)
  - Test case(s)
- **Starting Point:** `docs/requirements/SRS.md` (preliminary)
- **Project's Responsibility:** Adapt SRS to project context, verify traceability
- **Output:** Traceability Matrix (TRM)
- **Evidence for Safety Case:** TRM document, reviewed & signed

#### E. Hazard Analysis
- **Requirement:** EN 50128:2011, Section 5
- **What:** Identify failure modes in safeAPIFramework context:
  - Backend not registered (NULL pointer dereference)
  - Buffer overflow in string operations
  - Race condition in inter-task communication (IPC)
  - Non-volatile memory (NVM) corruption
  - Timer not initialized
  - Resource pool exhaustion
- **Output:** Hazard Register, risk scores, mitigation strategies
- **Evidence for Safety Case:** HARA report

#### F. Backend Implementation Verification
- **Requirement:** Special (ADR-005 backend registration)
- **What:** For each RTOS/OS backend you implement:
  - Formal design review
  - Implementation code review
  - Unit testing against interface contract
  - Integration testing with safeAPIFramework
  - Static analysis (your backend code must also be MISRA compliant)
- **Output:** Backend verification report
- **Evidence for Safety Case:** Backend documentation, test results

### 2.2 Assessment & Certification (Project Responsibility)

#### Before Deployment
1. **Internal Assessment:**
   - Project QA signs off on formal verification
   - All code review, testing, and analysis complete
   - Safety Case draft prepared

2. **Independent Assessment (Notified Body):**
   - Engage CENELEC-certified assessment lab
   - Provide all verification artifacts
   - Participate in assessment interviews
   - Resolve findings
   - Receive assessment report

3. **Railway Authority Certification:**
   - Submit Safety Case + assessment report
   - Obtain approval for deployment
   - (Timeline: 3-6 months typically)

---

## 3. Usage Restrictions & Assumptions

### 3.1 Allowed Uses

✅ **Allowed:**
- Reference implementation in new railway software projects
- Component in systems undergoing formal EN 50128 development
- Backend for distributed systems (where project controls each backend)
- Basis for training on EN 50128 patterns
- Non-deployed research & simulation

❌ **NOT Allowed:**
- Direct deployment without formal assessment
- SIL 3/4 systems without Notified Body involvement
- Systems deployed to revenue service without safety approval
- Reliance on this document as certification evidence (it's not)

### 3.2 Technical Assumptions

#### Architecture Assumptions
- **Assumption:** OS/RTOS layer provides thread-safe backend implementations
- **Project Must Verify:** Backend implementations are thread-safe (if multi-threaded)

- **Assumption:** safeAPIFramework services are called from single or properly-synchronized threads
- **Project Must Verify:** Calling code doesn't violate this assumption

- **Assumption:** Backends are registered before any service is used
- **Project Must Verify:** Initialization order and backend registration in main()

#### Memory Assumptions
- **Assumption:** No dynamic memory allocation post-initialization
- **Project Must Verify:** Callers provide static buffers to all SAPI functions

- **Assumption:** Buffer sizes are statically known
- **Project Must Verify:** No buffer overflow due to untrusted input sizes

#### Type Safety Assumptions
- **Assumption:** All integer conversions use sapi_cast_* functions
- **Project Must Verify:** Static analysis confirms no bare casts in application code

#### Timing Assumptions
- **Assumption:** Timer backends provide sufficient precision for application
- **Project Must Verify:** Timer jitter is within safety requirements

- **Assumption:** NVM operations complete within timeout bounds
- **Project Must Verify:** NVM backend latency is characterized

### 3.3 Known Limitations

| Limitation | Impact | Mitigation |
|-----------|--------|-----------|
| **Manual MISRA review** | Not certified; possible missed violations | Use certified MISRA tool in your project |
| **No formal assessment** | No evidence safeAPIFramework is correct | Formal assessment is your project's responsibility |
| **Limited test coverage** | Reference tests only; not exhaustive | Expand tests in your system context |
| **No real RTOS backends** | Reference backends are stubs | Implement and verify actual RTOS backends |
| **No multi-core verification** | Assumes single-threaded or synchronized | Verify thread safety for your RTOS |
| **No real hardware testing** | Only tested on development machine | Hardware integration testing is your responsibility |

---

## 4. Required Documentation in Your Safety Case

When using safeAPIFramework, your project's Safety Case must include:

### 4.1 Component Architecture & Traceability
```
1. System Architecture Document
   - Role of safeAPIFramework in system
   - How OAL isolates application from OS
   - Backend architecture diagram
   - Integration points with application logic

2. Traceability Matrix
   - Requirements → Design (ADRs)
   - Design → Implementation (source files)
   - Implementation → Tests

3. Requirement Specification (SRS)
   - List all safeAPIFramework requirements
   - Reference to SRS.md + project-specific modifications
   - Verification method for each requirement
```

### 4.2 Design Documentation
```
1. Architecture Decision Records (existing: ADR-001 through ADR-007)
   - Reference the provided ADRs
   - Document any project-specific modifications

2. Backend Design Document (new)
   - Thread safety model
   - Resource management strategy
   - Failure modes and recovery

3. Interface Design (existing: public header comments)
   - Reference Doxygen documentation
   - Document any deviations from contracts
```

### 4.3 Verification Evidence
```
1. Code Review Report
   - Checklist of MISRA rules reviewed
   - Findings (violations vs. deviations)
   - Sign-off by review team

2. Static Analysis Report
   - Tool used (e.g., cppcheck, LDRA)
   - MISRA compliance results
   - Any suppressions and rationale
   - Version of code analyzed

3. Test Report
   - Unit test results (all modules)
   - Code coverage metrics (>80% recommended)
   - Test case traceability to requirements
   - Integration test results (with application)

4. Hazard Analysis Report (HARA)
   - Failure modes identified
   - Risk scores (severity × likelihood)
   - Mitigation measures
   - Residual risk assessment
```

### 4.4 Assessment & Certification
```
1. Notified Body Assessment Report
   - Findings (critical, major, minor)
   - Evidence of resolution
   - Final assessment decision (Pass/Fail)

2. Railway Authority Approval
   - Safety approval letter
   - Deployment authorization
   - Operational constraints (if any)
```

---

## 5. Safety Application Conditions Template

**Use this template for your project's Safety Case:**

```markdown
# safeAPIFramework - Safety Application Conditions

**Project:** [RBC/Signaling System Name]  
**SIL Target:** [2/3/4]  
**Integrator:** [Your Company]  
**Date:** [Date]  

## Usage Declaration

This project uses safeAPIFramework v0.1.0 as the basis for the OS Abstraction Layer (OAL).

safeAPIFramework provides:
- ✅ Layered architecture (application ↔ OAL ↔ RTOS)
- ✅ MISRA C:2012 compliant code patterns
- ✅ 13 modular services (timer, NVM, IPC, etc.)
- ✅ Reference design patterns (ADR-001 through ADR-007)

safeAPIFramework does NOT provide:
- ❌ SIL certification (project's responsibility)
- ❌ Formal assessment evidence (project must generate)
- ❌ OS/RTOS backends (project must implement)
- ❌ Safety Case (project must write)

## Formal Verification Performed

### Code Review
- [ ] MISRA C:2012 review completed
- [ ] CLAUDE.md coding standards review completed
- [ ] EN 50128 Section 6 compliance review completed
- Review Report: [filename]
- Review Sign-off: [Name, Date]

### Static Analysis
- [ ] cppcheck run: [version, date]
- [ ] Certified MISRA tool run: [tool name, version, date]
- Static Analysis Report: [filename]

### Testing
- [ ] Unit test coverage: [%]
- [ ] Integration tests: [number of tests]
- [ ] All tests passed: [Yes/No]
- Test Report: [filename]

### Traceability
- [ ] Requirements traced to design
- [ ] Design traced to implementation
- [ ] Implementation traced to tests
- Traceability Matrix: [filename]

### Hazard Analysis
- [ ] HARA completed
- [ ] Failure modes identified
- [ ] Risk scores assigned
- [ ] Mitigations defined
- HARA Report: [filename]

### Assessment
- [ ] Notified Body assessment scheduled: [date]
- [ ] Assessment completed: [date]
- [ ] Findings resolved: [Yes/No]
- Assessment Report: [filename]

## Assumptions & Constraints

[Document your project-specific assumptions:]

1. Backend implementations are thread-safe
2. Application uses synchronization for multi-threaded access
3. Timer precision is ±[X]ms (acceptable for safety logic)
4. NVM block size is [Y] bytes (application designed for this)
5. [Add more...]

## Scope & Limitations

- safeAPIFramework code is verified per this SAC
- Application code using safeAPIFramework is verified separately
- OS/RTOS layer is verified by [vendor/in-house]
- Hardware is qualified per EN 50128 hardware section

## Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Project Safety Manager | | | |
| QA Lead | | | |
| System Architect | | | |
| Functional Safety Assessor | | | |
```

---

## 6. Formal Code Review Checklist

Use this checklist for your formal review of safeAPIFramework:

### MISRA C:2012 Mandatory Rules
- [ ] Dir 1.1 — Code in C (not C++)
- [ ] Dir 4.1 — Restrictions on declarations
- [ ] Dir 4.3 — No implicit function declarations
- [ ] Dir 4.4 — No include guards
- [ ] Dir 4.9 — Restrict function-like macros
- [Continue for all Mandatory rules...]

### MISRA C:2012 Required Rules
- [ ] Rule 1.2 — Language extensions
- [ ] Rule 1.3 — Fixed-width integer types
- [ ] Rule 2.1 — No unreachable code
- [ ] Rule 2.2 — No dead variables
- [ ] Rule 2.5 — No unused macros
- [ ] Rule 5.1 — Identifier length
- [ ] Rule 8.1 — Functions must have prototypes
- [ ] Rule 8.6 — Functions must have unique names across TUs
- [ ] Rule 8.7 — Objects with internal linkage
- [ ] Rule 10.1-10.8 — Type conversions
- [ ] Rule 11.1-11.9 — Pointer type conversions
- [ ] Rule 13.1 — Constraints on expression operators
- [ ] Rule 14.2 — Use of nullptr
- [ ] Rule 15.5 — Single point of exit (deviation documented)
- [ ] Rule 16.1 — switch must have default
- [ ] Rule 17.2 — No recursion
- [ ] Rule 20.1 — include guards
- [ ] Rule 20.5 — No trigraphs
- [ ] Rule 20.10-20.16 — Library restrictions
- [ ] Rule 21.6 — No stdio.h (strcpy, sprintf, etc.)
- [Continue for all Required rules...]

### EN 50128 Technical Requirements (Section 6)
- [ ] Layered architecture correctly implemented
- [ ] Module interfaces clearly defined
- [ ] State machine for safe-state transitions correct
- [ ] Error handling explicit (no exceptions)
- [ ] Resource allocation static (no dynamic memory)
- [ ] Pointer validation on all entries
- [ ] Type conversions checked and bounded
- [ ] Timing assumptions documented
- [ ] No undefined behavior or implementation-defined behavior
- [ ] Bounded operations (no strcpy, sprintf, etc.)

### Project-Specific Requirements
- [ ] [Add your organization's coding standards]
- [ ] [Add your specific railway safety requirements]
- [ ] [Add your RTOS/platform specific requirements]

---

## 7. Deviation Recording

**Important:** Every MISRA violation found must be documented:

```markdown
## MISRA Deviations

| Rule | File | Line | Violation | Rationale | Risk | Mitigation | Approval |
|------|------|------|-----------|-----------|------|-----------|----------|
| 15.5 | src/status/sapi_status.c | 25 | Multiple return statements | Guard clauses improve readability vs nested else | Low | Code review confirms pattern consistency | [Manager] |
| [Add findings...] | | | | | | | |
```

**Every deviation must be:**
1. Explicitly documented
2. Justified with technical rationale
3. Risk-assessed
4. Mitigated (if needed)
5. Approved by project safety authority

---

## 8. References

- **EN 50128:2011** — Railway applications - Software on board rolling stock - Software safety
- **EN 50129:2018** — Railway applications - Communication, signalling and processing systems - Functional safety and technical safety
- **MISRA C:2012** — Guidelines for the use of the C language in critical systems
- **CENELEC Assessment Procedures** — For Notified Body review criteria

---

## 9. Approval & Revision

| Version | Date | Author | Change | Approval |
|---------|------|--------|--------|----------|
| 0.1.0 | 2026-08-02 | Framework Team | Initial SAC | [Pending] |
| | | | | |

---

## 10. Contact & Questions

For questions about these Safety Application Conditions:

- **Framework Design:** See [docs/architecture/ADR-*.md](architecture/)
- **EN 50128 Alignment:** See [docs/EN50128_ALIGNMENT.md](EN50128_ALIGNMENT.md)
- **Integration Guide:** See [docs/INTEGRATION.md](INTEGRATION.md)
- **Requirements Specification:** See [docs/requirements/SRS.md](requirements/SRS.md)

**Important:** This document is guidance only. Your project's Functional Safety Manager is responsible for defining actual Safety Application Conditions based on your system requirements and regulatory context. All commitments in your Safety Application Conditions must be approved by your railway system's certification authority.
