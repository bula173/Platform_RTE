# Safety Plan

**Document ID:** [PROJECT]-SAFETY-PLAN-26.08  
**Project:** [Railway System Name - e.g., ERTMS RBC]  
**SIL Target:** 4  
**Version:** 1.0  
**Date:** [Date]  
**Status:** [DRAFT / APPROVED]  
**Author:** [Name, Functional Safety Manager]  

---

## 1. Introduction

### 1.1 Purpose
This Safety Plan defines the overall strategy, organization, and approach for developing [System Name] to meet EN 50128:2011 SIL 4 safety requirements.

### 1.2 Scope
- **In Scope:** All software components, including safeAPIFramework baseline
- **Out of Scope:** Hardware design, RTOS implementation (if externally supplied)

### 1.3 Normative References
- EN 50128:2011 — Railway applications - Software on board rolling stock - Software safety
- EN 50129:2018 — Railway applications - Communication, signalling and processing systems - Functional safety and technical safety
- MISRA C:2012 — Guidelines for the use of the C language in critical systems
- CENELEC Product Safety Standards

---

## 2. Lifecycle Model

### 2.1 Development Lifecycle
```
Requirements Phase
    ↓
Design Phase
    ↓
Implementation Phase
    ↓
Verification Phase
    ↓
Assessment Phase (Notified Body)
    ↓
Deployment & Operation
```

**Process:** [V-Model / Agile / Spiral - describe your chosen model]

### 2.2 Verification Strategy

| Activity | Phase | Responsibility | Output |
|----------|-------|-----------------|--------|
| Requirements Review | Requirements | System Architect | Review Report |
| Design Review | Design | Architecture Lead | Design Review Report |
| Code Review | Implementation | Code Review Team | Code Review Report |
| Static Analysis | Implementation | QA / Tools | MISRA Report |
| Unit Testing | Implementation | Developers | Test Report |
| Integration Testing | Verification | Test Team | Integration Test Report |
| System Testing | Verification | Test Team | System Test Report |
| Functional Safety Assessment | Assessment | Notified Body | Assessment Report |

---

## 3. Organization & Responsibilities

### 3.1 Roles

| Role | Name | Responsibilities |
|------|------|------------------|
| **Functional Safety Manager** | [Name] | Overall safety strategy, safety approval |
| **Project Manager** | [Name] | Schedule, resources, stakeholder management |
| **System Architect** | [Name] | System design, architecture decisions |
| **Development Lead** | [Name] | Code quality, implementation standards |
| **QA / Test Lead** | [Name] | Testing strategy, verification |
| **Configuration Manager** | [Name] | Change control, baselines, traceability |

### 3.2 Review Authority
- **Technical Reviews:** [Name, Role]
- **Safety Reviews:** [Functional Safety Manager]
- **Management Approval:** [Project Manager / Program Manager]

---

## 4. Tools & Methods

### 4.1 Development Tools
| Tool | Purpose | Justification |
|------|---------|---------------|
| CMake | Build system | Standard, widely supported |
| C99 Compiler | Translation | Matches EN 50128 qualified toolchains |
| [cppcheck / Tool] | Static Analysis | MISRA compliance verification |
| [Unit Test Framework] | Unit Testing | [e.g., CUnit, Unity] |
| Git | Version Control | Traceability, change history |

### 4.2 Static Analysis Tools
- **Primary:** [cppcheck / LDRA / Parasoft]
- **Standard:** MISRA C:2012
- **Coverage Goal:** 100% of source code
- **Deviation Process:** All deviations documented and approved

### 4.3 Testing Methods
- **Unit Testing:** All modules tested individually
- **Integration Testing:** Modules tested together
- **System Testing:** Full system tested end-to-end
- **Coverage Target:** >90% code coverage (SIL 4)

---

## 5. Coding Standards

### 5.1 Language & Standard
- **Language:** C99 (C11 compatible)
- **Standard:** MISRA C:2012 Mandatory & Required rules
- **Exceptions:** [Document any deviations and rationale]

### 5.2 Key Constraints
✓ No dynamic memory allocation (malloc/free banned)  
✓ Explicit error handling (no exceptions)  
✓ Checked type conversions (no bare casts)  
✓ Bounded operations (no strcpy, sprintf)  
✓ No recursion  
✓ Pointer validation required  

### 5.3 safeAPIFramework Baseline
- **Version:** 0.1.0
- **Usage:** OS Abstraction Layer (OAL)
- **Verification:** Per SAFETY_APPLICATION_CONDITIONS.md
- **Modifications:** [List any modifications to safeAPIFramework code]

---

## 6. Traceability & Configuration Management

### 6.1 Traceability
- **Bidirectional:** Requirements ↔ Design ↔ Code ↔ Tests
- **Matrix Tool:** [Spreadsheet / Dedicated tool]
- **Coverage:** 100% of requirements traced to implementation
- **Update:** Real-time with changes, reviewed at phase gates

### 6.2 Configuration Management
- **Baseline:** Established at end of each phase
- **Change Control:** Formal change request process (see CM Plan)
- **Build Management:** Reproducible builds, version controlled
- **Release Management:** Tagged releases for assessment

---

## 7. Requirement Traceability

### 7.1 Requirement Categories

| Category | Count | Owner | Status |
|----------|-------|-------|--------|
| Functional Requirements | [n] | System Architect | [Draft / In Review / Approved] |
| Safety Requirements | [n] | Functional Safety Manager | [Draft / In Review / Approved] |
| Design Constraints | [n] | System Architect | [Draft / In Review / Approved] |
| Quality Requirements (MISRA, etc.) | [n] | QA Lead | [Draft / In Review / Approved] |

### 7.2 Requirement Sources
- Safety standards (EN 50128, EN 50129)
- Customer specifications
- HARA findings (safety-driven requirements)
- Architectural patterns (safeAPIFramework)

---

## 8. Review & Assessment

### 8.1 Internal Review Gates
| Gate | Input | Output | Approval |
|------|-------|--------|----------|
| Requirements Gate | SRS, HARA | Approved requirements | Functional Safety Manager |
| Design Gate | Architecture, Design Review Report | Approved design | System Architect |
| Code Gate | Code Review Report, Static Analysis | Approved code | Development Lead |
| Test Gate | Test Results, Coverage Report | Approved test evidence | QA Lead |
| Assessment Readiness | All above evidence packaged | Ready for Notified Body | Functional Safety Manager |

### 8.2 Notified Body Assessment
- **Timing:** [Target date for assessment start]
- **Body:** [CENELEC-certified lab]
- **Scope:** Full SIL 4 assessment
- **Expected Duration:** 3-6 months
- **Evidence Package:** [List key documents to be submitted]

---

## 9. Risk Management

### 9.1 Technical Risks
| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| MISRA violations found late | Medium | High | Early static analysis, code review before implementation |
| Test coverage inadequate | Low | High | Coverage tools, coverage goals set early |
| RTOS backend issues | Medium | High | Early backend design review, prototype testing |
| Schedule overrun | Medium | Medium | Buffer in schedule, early identification of issues |

### 9.2 Organizational Risks
| Risk | Mitigation |
|------|-----------|
| Key personnel unavailability | Cross-train team members |
| Tool unavailability | Qualified backup tools identified |
| Notified Body delays | Engage Notified Body early |

---

## 10. Documentation & Evidence

### 10.1 Documentation Deliverables
See `00_DOCUMENT_CHECKLIST.md` for complete list of 29 documents.

### 10.2 Evidence Package for Notified Body
```
Assessment Package includes:
- Safety Case
- All requirement/design/test documents
- Code review and static analysis reports
- Source code (under version control)
- Test logs and coverage reports
- HARA and risk assessment
- Deviation management
```

### 10.3 Storage & Access
- **Location:** [Repository path / System]
- **Access:** [Team members, Notified Body]
- **Backup:** [Frequency and method]
- **Retention:** [Duration - typically 10+ years]

---

## 11. Quality Metrics

### 11.1 Code Quality Targets
| Metric | Target | Verification |
|--------|--------|--------------|
| MISRA Compliance | 100% or documented deviations | Static analysis tool |
| Code Coverage | >90% | Coverage tool |
| Cyclomatic Complexity | <10 per function | Static analysis tool |
| Code Review Coverage | 100% | Review checklist |

### 11.2 Schedule Metrics
- Requirements phase completion: [Date]
- Design phase completion: [Date]
- Code completion: [Date]
- Testing completion: [Date]
- Assessment readiness: [Date]

---

## 12. Communication & Reporting

### 12.1 Stakeholder Updates
- **Monthly:** Status report to Program Manager
- **Bi-weekly:** Team syncs
- **Quarterly:** Steering committee review

### 12.2 Issue Escalation
- **Critical (SIL violation):** Immediate escalation to Functional Safety Manager
- **Major (schedule impact):** Weekly escalation
- **Minor (tracking):** Sprint review

---

## 13. Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Functional Safety Manager | | | |
| Project Manager | | | |
| System Architect | | | |
| Quality Assurance Lead | | | |

---

## 14. Change History

| Version | Date | Author | Change |
|---------|------|--------|--------|
| 0.1 | [Date] | [Name] | Initial draft |
| 1.0 | [Date] | [Name] | Approved by FSM |

---

## References

- EN 50128:2011
- EN 50129:2018
- MISRA C:2012
- safeAPIFramework Documentation
- Project Charter
- [Other standards specific to your system]
