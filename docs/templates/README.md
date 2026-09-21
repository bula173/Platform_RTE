# SIL 4 Assessment Document Templates

This directory contains **ready-to-use templates** for all documents required to conduct a formal EN 50128:2011 SIL 4 assessment using RteFramework.

---

## Quick Start

### For Project Managers
1. Read: [`00_DOCUMENT_CHECKLIST.md`](00_DOCUMENT_CHECKLIST.md)
2. Use the checklist to track document creation
3. Use the timeline (6-12 months) for project scheduling

### For Functional Safety Managers
1. Read: [`01_SAFETY_PLAN.md`](01_SAFETY_PLAN.md)
2. Define your project's safety lifecycle
3. Use as the basis for all other documents

### For Architects
1. Read: [`06_HARA_TEMPLATE.md`](06_HARA_TEMPLATE.md)
2. Identify hazards specific to your system
3. Reference the RteFramework hazards as examples

### For QA/Testing Teams
1. Read: [`15_CODE_REVIEW_REPORT.md`](15_CODE_REVIEW_REPORT.md)
2. Set up code review checklist and process
3. Plan your testing strategy based on findings

### For Safety Approvers
1. Read: [`22_SAFETY_CASE.md`](22_SAFETY_CASE.md)
2. Understand the complete safety argument
3. Use as approval criteria for SIL 4 release

---

## Document Overview

### Essential Documents (Start Here)

| # | Document | Purpose | Audience | Effort |
|---|----------|---------|----------|--------|
| **00** | Document Checklist | Track all documents needed | Everyone | 1 day |
| **01** | Safety Plan | Define overall safety strategy | FSM, PM | 1 week |
| **06** | HARA | Identify and assess hazards | Architect, FSM | 2 weeks |
| **15** | Code Review Report | Formal code quality assurance | Dev Lead, QA | 2 weeks |
| **22** | Safety Case | Master argument for SIL 4 | FSM, Notified Body | 2 weeks |

### How the Templates Relate

```
Safety Plan (01)
    │
    ├─→ HARA (06) ─→ Safety Requirements
    │
    ├─→ Design Documents (09-11)
    │
    ├─→ Code Review (15) ─→ Deviations
    │
    ├─→ Testing (19-20) ─→ Coverage
    │
    └─→ Safety Case (22) ← Consolidates everything
        └─→ Notified Body Assessment
```

---

## How to Use These Templates

### Step 1: Customize for Your Project
Each template has placeholder sections marked with `[...]`:
```markdown
**Project:** [Railway System Name - e.g., ERTMS RBC]
**Author:** [Name]
**Approval:** [Signature]
```

Replace these with your project-specific information.

### Step 2: Follow the Structure
Templates provide the required structure per EN 50128:2011. Don't remove major sections—add details instead.

### Step 3: Add Your Evidence
Templates show examples. Replace examples with your project's actual:
- Hazard analysis results
- Code review findings
- Test reports
- Metrics and measurements

### Step 4: Maintain Traceability
Link documents together:
- Safety Case references Hazard Analysis
- Hazard Analysis drives Safety Requirements
- Safety Requirements trace to Code
- Code Review findings documented as Deviations

### Step 5: Review & Approve
Each template includes approval sections. Don't skip sign-off—these are formal safety documents.

---

## Complete Document List (29 Total)

### Phase 1: Planning (Before Design)
```
01_SAFETY_PLAN.md ...................... Overall safety lifecycle approach
02_VERIFICATION_VALIDATION_PLAN.md .... Testing and verification strategy
03_CONFIGURATION_MANAGEMENT_PLAN.md .. Change control and CM procedures
04_TOOL_QUALIFICATION.md ............... If using specialized MISRA tools
```

### Phase 2: Requirements (Requirements Phase)
```
05_SRS_TEMPLATE.md ..................... System Requirements Specification
06_HARA_TEMPLATE.md .................... Hazard Analysis & Risk Assessment
07_SAFE_REQUIREMENTS_TEMPLATE.md ....... Derived Safety Requirements
08_RTE_INTEGRATION_PLAN.md ......... How RteFramework is used
```

### Phase 3: Design (Design Phase)
```
09_ARCHITECTURE_DESIGN.md .............. System and software architecture
10_DETAILED_DESIGN.md .................. Module-level design specifications
11_BACKEND_DESIGN.md ................... OS/RTOS-specific OSAdapter design
12_TRACEABILITY_MATRIX.md .............. Req → Design → Code → Test links
13_DESIGN_REVIEW_REPORT.md ............ Findings from design review
```

### Phase 4: Implementation (Implementation Phase)
```
14_CODE_REVIEW_CHECKLIST.md ........... MISRA + EN 50128 review checklist
15_CODE_REVIEW_REPORT.md .............. Formal code review results
16_STATIC_ANALYSIS_REPORT.md ......... MISRA tool output
17_CODING_STANDARDS.md ................ Deviation from MISRA rules
18_TEST_PLAN.md ....................... Unit, integration, system test strategy
19_UNIT_TEST_REPORT.md ................ Module-level test results
20_INTEGRATION_TEST_REPORT.md ......... System integration test results
21_COVERAGE_ANALYSIS.md ............... Code coverage metrics & analysis
```

### Phase 5: Assessment (Assessment Phase)
```
22_SAFETY_CASE.md ..................... Main argument for system safety
23_FUNCTIONAL_SAFETY_ASSESSMENT.md .... SIL verification evidence
24_RTE_VERIFICATION.md ............ How RteFramework was verified
25_DEVIATION_MANAGEMENT.md ............ All MISRA/EN 50128 deviations
26_ASSESSMENT_READINESS.md ............ Pre-assessment verification checklist
```

### Phase 6: Certification (Assessment Phase)
```
27_ASSESSMENT_REPORT.md ............... Notified Body official assessment
28_FINDING_CLOSURE.md ................. Resolution of assessment findings
29_CERTIFICATE.md ..................... SIL 4 certification (from Notified Body)
```

**Total: 29 documents across 6 phases**

---

## Effort Estimates

### Minimal Assessment (MVP - 6-9 months)
Use documents: 01, 05, 06, 09, 15, 19, 22

**Effort:** ~1-2 people × 6 months  
**Cost:** ~€50k-100k  
**Suitable for:** Simple systems, small teams

### Standard Assessment (Recommended - 9-12 months)
Use documents: All except 02, 03, 04, 23, 25, 26, 28, 29

**Effort:** ~2-3 people × 9 months  
**Cost:** ~€100k-200k  
**Suitable for:** Typical railway projects

### Complete Assessment (Belt & Suspenders - 12-18 months)
Use all 29 documents

**Effort:** ~3-5 people × 12 months  
**Cost:** ~€200k-400k  
**Suitable for:** Large, critical systems or complex RTOS

---

## Document Submission Package

When ready to submit to a Notified Body, package should include:

```
[PROJECT]_SIL4_Assessment_Package/
│
├── README.txt (overview of package contents)
│
├── DOCUMENTS/
│   ├── 01_SAFETY_PLAN.md
│   ├── 05_SRS.md
│   ├── 06_HARA.md
│   ├── 07_SAFE_REQUIREMENTS.md
│   ├── 09_ARCHITECTURE.md
│   ├── 10_DETAILED_DESIGN.md
│   ├── 12_TRACEABILITY_MATRIX.md
│   ├── 15_CODE_REVIEW_REPORT.md
│   ├── 16_STATIC_ANALYSIS_REPORT.md
│   ├── 19_UNIT_TEST_REPORT.md
│   ├── 21_COVERAGE_ANALYSIS.md
│   └── 22_SAFETY_CASE.md
│
├── SOURCE_CODE/
│   ├── src/
│   ├── include/
│   └── CMakeLists.txt
│
├── EVIDENCE/
│   ├── code-review-findings.csv
│   ├── static-analysis-report.xml
│   ├── test-results.xml
│   ├── coverage-report.html
│   └── [Other tool outputs]
│
└── VERSION_CONTROL/
    └── git-log.txt (commit history showing traceability)
```

---

## Common Mistakes to Avoid

### ❌ Don't

- **Omit document structure** — EN 50128 sections are mandatory
- **Skip approval signatures** — Unsigned documents aren't evidence
- **Hide MISRA violations** — Document and justify instead
- **Make promises in Safety Case** — Only claim what you've verified
- **Wait until the end** — Start Safety Case during design, not after code
- **Ignore traceability** — Every requirement must link to design/code/test
- **Use stubs for evidence** — Real tool outputs needed, not hand-crafted examples

### ✓ Do

- **Customize templates** — Make them fit your project
- **Get early feedback** — Share drafts with Notified Body before formal submission
- **Update continuously** — Keep documents in sync with code (use Change Control)
- **Assign owners** — Each document needs a single responsible person
- **Review systematically** — Follow the review workflow in each template
- **Link everything** — Use cross-references and traceability matrices extensively
- **Plan ahead** — 6-12 months is realistic for SIL 4

---

## RteFramework-Specific Sections

Each template has a section addressing RteFramework usage:

**In SAFETY_PLAN (01):**
- Section 5.3: RteFramework baseline and modifications

**In HARA (06):**
- Section 3.1: Hazards specific to RteFramework services
- Examples: Uninitialized OSAdapter, buffer overflow, IPC race conditions, NVM corruption

**In CODE_REVIEW_REPORT (15):**
- Checklist items for RteFramework code review

**In SAFETY_CASE (22):**
- Section 5.3: RteFramework verification strategy
- Evidence of how RteFramework mitigates hazards

---

## External References

- **SAFETY_APPLICATION_CONDITIONS.md** — Describes how to use RteFramework in a safety project
- **EN50128_ALIGNMENT.md** — Detailed mapping of framework design to EN 50128 sections
- **INTEGRATION.md** — How to integrate RteFramework into your project

---

## Getting Help

### Questions About...

| Topic | Reference |
|-------|-----------|
| Overall safety approach | Read Safety Plan template intro + EN 50128:2011 Section 4-5 |
| Hazard identification | Read HARA template + EN 50128:2011 Section 5 |
| Code quality | Read Code Review template + MISRA C:2012 rules |
| Testing strategy | Read Safety Plan Section 4.3 + EN 50128:2011 Section 7.4 |
| RteFramework usage | Read SAFETY_APPLICATION_CONDITIONS.md |
| Overall SIL 4 path | Read Safety Case template + EN50128_ALIGNMENT.md |

### Template Support

These templates are provided as-is with:
- Real-world examples for guidance
- Detailed sections showing expected output format
- References to EN 50128:2011 sections for compliance
- RteFramework context and examples

For detailed MISRA C:2012 guidance, refer to the MISRA C:2012 standard document itself (~200 pages).

---

## Version & License

**RteFramework Templates Version:** 0.1  
**For Use With:** RteFramework v0.1.0+  
**Last Updated:** 2026-08-02  

**License:** These templates are provided for use in projects using RteFramework. Modify freely for your project context.

---

## Quick Links

- **Full Document List:** [00_DOCUMENT_CHECKLIST.md](00_DOCUMENT_CHECKLIST.md)
- **Start Here:** [01_SAFETY_PLAN.md](01_SAFETY_PLAN.md)
- **Hazard Analysis:** [06_HARA_TEMPLATE.md](06_HARA_TEMPLATE.md)
- **Code Review:** [15_CODE_REVIEW_REPORT.md](15_CODE_REVIEW_REPORT.md)
- **Safety Approval:** [22_SAFETY_CASE.md](22_SAFETY_CASE.md)
- **RteFramework Usage:** [../SAFETY_APPLICATION_CONDITIONS.md](../SAFETY_APPLICATION_CONDITIONS.md)

---

**Ready to build a SIL 4 railway system? Start with [01_SAFETY_PLAN.md](01_SAFETY_PLAN.md)** 🚂🛡️

---

## License & Community Contributions

**These templates are shared under the Community Improvement License (CIL).**

By using these templates:
- ✅ You can use, modify, and distribute them freely
- ✅ You must share improvements back to the community via Pull Request
- ✅ Or at minimum, create a GitHub Issue with your feedback and experience

**Why?** Railway safety is a shared responsibility. Your verification work, bug fixes, and Notified Body feedback help everyone build safer systems.

See [LICENSE.md](../../LICENSE.md) for full terms.
