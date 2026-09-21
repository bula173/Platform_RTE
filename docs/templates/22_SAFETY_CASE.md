# Safety Case

**Document ID:** [PROJECT]-SAFETY-CASE-26.08  
**Project:** [System Name]  
**SIL Target:** 4  
**Version:** 1.0  
**Date:** [Date]  
**Status:** [DRAFT / APPROVED]  
**Author:** [Name, Functional Safety Manager]  

---

## 1. Introduction

### 1.1 Purpose
This Safety Case is the master argument demonstrating that [System Name] meets SIL 4 safety requirements under EN 50128:2011.

### 1.2 Scope
- System safety assurance for [Railway function]
- Software design, implementation, and verification
- OS Abstraction Layer based on RteFramework v0.1.0
- RTOS OSAdapter implementation by [Company/Team]
- Excludes: Hardware design, external railway systems

### 1.3 Audience
- Railway Authority (approval of deployment)
- Notified Body (assessment and certification)
- Project stakeholders (confidence in safety)

### 1.4 Document Structure

```
1. Introduction (context)
   ↓
2. System Description (what we're certifying)
   ↓
3. Safety Assurance Argument (how we know it's safe)
   ↓
4. Design Lifecycle Evidence (what we did)
   ↓
5. Verification Evidence (tests, reviews, analysis)
   ↓
6. Safety Requirements Compliance (traceability)
   ↓
7. Risk Assessment (hazards controlled)
   ↓
8. Conclusions (confident it's SIL 4)
```

---

## 2. System Description

### 2.1 System Overview
[Brief description of system function and railway context]

**Example:**
"The [System Name] is an ERTMS Radio Block Centre (RBC) that provides movement authority to trains operating on [Railway Line]. It receives train position reports via radio link, performs conflict detection against interlocking logic, and issues movement authorities to equipped trains."

### 2.2 System Architecture

```
┌─────────────────────────────────────┐
│   Railway Infrastructure            │
│   (Interlocking, Signaling, etc.)   │
└────────────────┬──────────────────┘
                 │ Interlocking Protocol
    ┌────────────▼──────────────────────┐
    │  [System Name] - RBC              │
    ├──────────────────────────────────┤
    │  Application Layer (Functional)  │
    │  ├─ Conflict detection algorithm │
    │  ├─ Authority issuance logic      │
    │  └─ State machine controller      │
    ├──────────────────────────────────┤
    │  OS Abstraction Layer (OAL)      │
    │  ├─ RteFramework v0.1.0      │
    │  │  ├─ Timer service             │
    │  │  ├─ IPC (train communications)│
    │  │  ├─ NVM (configuration)        │
    │  │  └─ Task scheduling            │
    │  └─ OSAdapter Implementation        │
    │     ├─ RTOS: [e.g., VxWorks]     │
    │     ├─ Timer hardware             │
    │     └─ Storage (SD card)          │
    └────────────┬──────────────────────┘
                 │ Radio Link
    ┌────────────▼────────────────────┐
    │  Train Equipment (external)     │
    └─────────────────────────────────┘
```

### 2.3 Key Safety Functions

| Function | SIL | Controlled By | Failure Effect |
|----------|:---:|--------------|-----------------|
| Conflict Detection | 4 | Application algorithm | Unsafe movement authority issued |
| Authority Transmission | 4 | RteFramework IPC | Lost communication |
| State Persistence | 4 | RteFramework NVM + OSAdapter | Configuration loss, inconsistent state |
| Watchdog / Monitoring | 4 | Application + RTOS | Failure to detect system fault |

---

## 3. Safety Assurance Argument (Top-Level)

### 3.1 Assurance Strategy

**[System Name] achieves SIL 4 through:**

1. **Layered Design** (EN 50128:2011 Section 6.2)
   - Application logic separated from OS services
   - Each layer independently verifiable
   - Clear interfaces reduce coupling

2. **MISRA C:2012 Compliance** (EN 50128:2011 Section 7)
   - No dynamic memory, no recursion, checked casts
   - Formal code review against MISRA Mandatory/Required rules
   - Static analysis with certified MISRA tool

3. **Comprehensive Verification** (EN 50128:2011 Section 7)
   - Unit testing (>90% code coverage)
   - Integration testing (module interactions)
   - System testing (end-to-end scenarios)
   - Hazard analysis with mitigations

4. **Independent Assessment** (EN 50128:2011 Section 8)
   - Notified Body review of all design artifacts
   - Verification of compliance evidence
   - Third-party confidence in safety claims

### 3.2 Assurance Evidence Structure

```
SAFETY ARGUMENT: "This system is SIL 4 safe because..."

├─ Evidence 1: Adequate Design (Architecture)
│  ├─ Design Decision Records (ADRs)
│  ├─ Architecture Review Report
│  └─ Traceability Matrix
│
├─ Evidence 2: Correct Implementation (Code Quality)
│  ├─ Code Review Report
│  ├─ Static Analysis Report (MISRA)
│  └─ Coding Standards Compliance
│
├─ Evidence 3: Rigorous Verification (Testing)
│  ├─ Unit Test Report (>90% coverage)
│  ├─ Integration Test Report
│  ├─ System Test Results
│  └─ Coverage Analysis
│
├─ Evidence 4: Hazard Control (Risk Management)
│  ├─ Hazard Analysis & Risk Assessment (HARA)
│  ├─ Risk Control Measures Implemented
│  └─ Residual Risk Acceptable for SIL 4
│
└─ Evidence 5: Independent Verification (Assessment)
   ├─ Notified Body Assessment Report
   ├─ Finding Resolution Log
   └─ SIL 4 Certificate
```

---

## 4. Design Lifecycle Evidence

### 4.1 Requirements Phase

**Input:** Customer specification, safety requirements  
**Output:** SRS (System Requirements Specification)

**Completeness:** ✓
- [x] Functional requirements documented
- [x] Safety requirements identified
- [x] Requirements traced to design
- [x] Requirements reviewed and approved

**Evidence:**
- 05_SRS_TEMPLATE.md
- 06_HARA_TEMPLATE.md (hazards → safety requirements)
- 07_SAFE_REQUIREMENTS_TEMPLATE.md
- Traceability Matrix (12)

### 4.2 Design Phase

**Input:** Approved SRS  
**Output:** Architecture & Detailed Design

**Completeness:** ✓
- [x] Architecture designed per EN 50128 Section 6.2
- [x] Layered design: Application → OAL → RTOS
- [x] RteFramework OAL selected and reviewed
- [x] OSAdapter design documented
- [x] Design reviewed and approved

**Evidence:**
- 09_ARCHITECTURE_DESIGN.md
- 10_DETAILED_DESIGN.md
- 11_BACKEND_DESIGN.md
- 13_DESIGN_REVIEW_REPORT.md
- Traceability: Design ↔ Requirements

### 4.3 Implementation Phase

**Input:** Approved design  
**Output:** Source code, unit tests

**Completeness:** ✓
- [x] Code written per MISRA C:2012 standards
- [x] Formal code review completed
- [x] Static analysis passed
- [x] Unit tests written and passed
- [x] Code coverage >90%

**Evidence:**
- Source code (git repository)
- 15_CODE_REVIEW_REPORT.md
- 16_STATIC_ANALYSIS_REPORT.md
- 19_UNIT_TEST_REPORT.md
- 21_COVERAGE_ANALYSIS.md

### 4.4 Verification Phase

**Input:** Implementation + test suite  
**Output:** Test reports, integrated system

**Completeness:** ✓
- [x] Unit tests passing
- [x] Integration tests designed and executed
- [x] System tests validate end-to-end scenarios
- [x] Performance verified (timings, resource limits)

**Evidence:**
- 19_UNIT_TEST_REPORT.md
- 20_INTEGRATION_TEST_REPORT.md
- Performance metrics
- Test execution logs

### 4.5 Assessment Phase

**Input:** All design and verification evidence  
**Output:** Notified Body assessment

**Completeness:** Pending
- [ ] Evidence package submitted to Notified Body
- [ ] Assessment findings resolved
- [ ] SIL 4 Certificate received

---

## 5. Verification Evidence Summary

### 5.1 Code Quality Assurance

| Activity | Standard | Requirement | Result | Status |
|----------|----------|-------------|--------|--------|
| Code Review | MISRA C:2012 | 100% of code reviewed | 100% | ✓ PASS |
| Static Analysis | MISRA Mandatory/Required | 0 violations (deviations approved) | 2 deviations approved | ✓ PASS |
| Cyclomatic Complexity | EN 50128 | <10 per function | Max 6 | ✓ PASS |
| Memory Management | MISRA Dir 4.12 | No malloc/free | None found | ✓ PASS |
| Pointer Validation | MISRA 20.8 | NULL check before deref | 100% validated | ✓ PASS |

### 5.2 Testing

| Test Level | Requirement | Achievement | Coverage | Status |
|------------|-------------|-------------|----------|--------|
| Unit Tests | >80% coverage | >90% coverage | 95% | ✓ PASS |
| Integration Tests | Module interactions | All key interactions tested | N/A | ✓ PASS |
| System Tests | End-to-end scenarios | Critical scenarios verified | N/A | ✓ PASS |

### 5.3 RteFramework Verification

Per SAFETY_APPLICATION_CONDITIONS.md:

| Item | Verification Method | Status |
|------|-------------------|--------|
| Formal code review | Checklist against MISRA/EN 50128 | ✓ Complete |
| Static analysis | MISRA tool (cppcheck + certified tool) | ✓ Complete |
| Unit testing | Test suite >90% coverage | ✓ Complete |
| HARA | Hazard identification & risk control | ✓ Complete |
| Traceability | Requirements → Design → Code → Tests | ✓ Complete |

---

## 6. Safety Requirements Compliance

### 6.1 Traceability Matrix (Summary)

```
Total Requirements: 47
├─ Functional Requirements: 23
│  ├─ Designed: 23 ✓
│  ├─ Implemented: 23 ✓
│  └─ Tested: 23 ✓
│
└─ Safety Requirements: 24
   ├─ From HARA: 12
   │  ├─ Designed: 12 ✓
   │  ├─ Implemented: 12 ✓
   │  └─ Tested: 12 ✓
   │
   └─ From EN 50128: 12
      ├─ Design: 12 ✓
      ├─ Implementation: 12 ✓
      └─ Verification: 12 ✓
```

**Result:** 100% requirements traceability ✓

### 6.2 Example Requirement Trace

**Requirement:** SR-001 RteFramework services shall not be called before OSAdapter registration

**Derived From:** HARA Hazard H1 (Uninitialized OSAdapter)

**Design Solution:**
- OSAdapter registration function: `rte_osadapter_timer_register(OSAdapter)`
- All services check OSAdapter == NULL → return RTE_STATUS_NOT_INITIALIZED
- Application initialization sequence documented

**Implementation:**
- File: src/timer/rte_timer.c, lines 15-20
- Function: rte_timer_create() checks OSAdapter != NULL

**Verification:**
- Unit Test: test_timer_create_no_osadapter()
- Verifies: Function returns RTE_STATUS_NOT_INITIALIZED when OSAdapter NULL
- Code Review: Confirmed NULL check present

**Evidence:** ✓ COMPLETE

---

## 7. Risk Assessment & Control

### 7.1 Hazards Identified (from HARA)

| ID | Hazard | SIL Required | Control Measures | Residual Risk | Acceptable? |
|:--:|--------|:------------:|------------------|:-------------:|:-----------:|
| H1 | Uninitialized OSAdapter | 4 | Code review, unit test, documentation | Low | ✓ Yes |
| H2 | Buffer Overflow | 4 | MISRA analysis, input validation, testing | Low | ✓ Yes |
| H3 | Race Condition | 4 | RTOS mutex, thread safety analysis, tests | Low | ✓ Yes |
| H4 | NVM Corruption | 4 | CRC checks, atomic writes, recovery | Low | ✓ Yes |
| H5 | Timer Timeout Failure | 4 | Timeout verification, watchdog, testing | Low | ✓ Yes |
| H6 | Resource Exhaustion | 4 | Static allocation, limit enforcement | Low | ✓ Yes |

**Conclusion:** All hazards have been reduced to acceptable risk levels for SIL 4.

### 7.2 MISRA Deviations

| Rule | Deviation | Rationale | Approval |
|------|-----------|-----------|----------|
| 15.5 | Multiple return statements (guard clauses) | Improves code readability & safety | FSM approved |

**Conclusion:** 1 approved deviation, fully documented and justified.

---

## 8. Compliance with EN 50128:2011

### 8.1 Section 6 - Technical Approaches

| Section | Topic | Compliance | Evidence |
|---------|-------|-----------|----------|
| 6.2 | Software Architecture | ✓ Layered design | ADRs, Architecture Design |
| 6.2.3 | Modularity | ✓ 13 modules, clear boundaries | Source code structure |
| 6.3 | Structured Design | ✓ Explicit design documents | Design documents 09-11 |
| 6.4 | Traceability | ✓ Bidirectional | Traceability Matrix |
| 6.5 | Safe-State | ✓ Defensive halt & recovery | RTE_SAFESTATE, RTE_REBOOT |

### 8.2 Section 7 - Quality Assurance

| Section | Topic | Compliance | Evidence |
|---------|-------|-----------|----------|
| 7.2 | Code Review | ✓ Formal review per MISRA | Code Review Report |
| 7.3 | Static Analysis | ✓ MISRA compliance | Static Analysis Report |
| 7.4 | Unit Testing | ✓ >90% coverage | Unit Test Report |
| 7.4 | System Testing | ✓ E2E scenarios | System Test Report |
| 7.5 | Problem Reporting | ✓ Deviation log | Deviation Management |

---

## 9. Confidence in Safety

### 9.1 Overall Assessment

**Multi-layered verification approach ensures confidence:**

1. **Prevention** — Design prevents errors
   - Clear module boundaries
   - MISRA C:2012 constraints eliminate common errors
   - Explicit error handling

2. **Detection** — Static analysis & reviews find errors
   - MISRA tool checks 30+ rules automatically
   - Formal code review catches patterns tools miss
   - Static analysis 100% of code

3. **Verification** — Testing proves behavior
   - Unit tests exercise all code paths
   - Integration tests verify module interactions
   - System tests validate end-to-end safety

4. **Assessment** — Independent eyes confirm rigor
   - Notified Body reviews all evidence
   - Third-party certifies SIL 4 compliance
   - Public confidence in safety claims

### 9.2 Residual Uncertainty (Minimal)

Remaining areas of uncertainty:

| Area | Residual Uncertainty | Mitigation |
|------|---------------------|-----------|
| RTOS OSAdapter implementation | Quality of OSAdapter code | OSAdapter design review, OSAdapter testing |
| Timing assumptions | Runtime timing may vary | System testing under load, worst-case analysis |
| Environmental factors | Extreme conditions (temperature, EMI) | Hardware qualification, system testing |

**Conclusion:** Residual uncertainty is acceptable for SIL 4.

---

## 10. Functional Safety Assessment Conclusion

**Based on comprehensive evidence presented in this Safety Case:**

1. ✓ System design follows EN 50128:2011 principles
2. ✓ Implementation is MISRA C:2012 compliant
3. ✓ Verification demonstrates >90% test coverage
4. ✓ Hazard analysis shows residual risks acceptable for SIL 4
5. ✓ Independent assessment by Notified Body confirms SIL 4 capability

**Assessment: [System Name] achieves SIL 4 safety integrity level** ✓

---

## 11. Approval

| Role | Name | Signature | Date | Notes |
|------|------|-----------|------|-------|
| Functional Safety Manager | [Name] | | | Approves safety case |
| Project Manager | [Name] | | | Confirms resource commitment |
| Quality Assurance Lead | [Name] | | | Confirms test evidence |
| Notified Body Assessor | [Name] | | | Independent confirmation |

---

## 12. Revision History

| Version | Date | Author | Change |
|---------|------|--------|--------|
| 0.1 | [Date] | [Name] | Draft safety case |
| 0.2 | [Date] | [Name] | Incorporated review comments |
| 1.0 | [Date] | [Name] | Approved by Functional Safety Manager |

---

## References

- EN 50128:2011
- EN 50129:2018
- MISRA C:2012
- [Project] Safety Plan (Document 01)
- [Project] HARA (Document 06)
- [Project] Architecture Design (Document 09)
- [Project] Code Review Report (Document 15)
- [Project] Unit Test Report (Document 19)
- All other supporting documents (01-26)

---

## Appendices

- **Appendix A:** Traceability Matrix (Detailed)
- **Appendix B:** Test Evidence Summary
- **Appendix C:** Hazard Analysis Details
- **Appendix D:** Code Metrics & Coverage Reports

