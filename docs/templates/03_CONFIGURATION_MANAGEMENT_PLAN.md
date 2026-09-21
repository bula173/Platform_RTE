# Configuration Management Plan

**Document ID:** [PROJECT]-CM-PLAN-26.08  
**Project:** [Railway System Name]  
**SIL Target:** 4  
**Version:** 1.0  
**Date:** [Date]  
**Status:** [DRAFT / APPROVED]  
**CM Manager:** [Name]  

---

## 1. Introduction

### 1.1 Purpose
This Configuration Management Plan defines how safeAPIFramework and project artifacts are managed, controlled, versioned, and released throughout the development lifecycle.

### 1.2 Scope
- safeAPIFramework source code (src/, include/, tests/)
- Project implementation
- Build system (CMake, presets, toolchain files)
- Documentation (requirements, design, test reports)
- Assessment artifacts (safety case, compliance reports)
- Assessment templates and examples
- Excluded: Third-party RTOS, external tools

### 1.3 CM Responsibilities
- **CM Manager:** Overall CM strategy and oversight
- **Configuration Auditor:** Verify compliance with this plan
- **Build Manager:** Build system administration
- **Release Manager:** Package and release management

---

## 2. Project Structure

### 2.1 Directory Layout

```
safeAPIFramework/
├── .git/                           Version control repository
├── .github/                        GitHub workflows, templates
│   ├── workflows/                  CI/CD automation
│   └── issue_templates/            Issue templates
│
├── .claude/                        Claude Code configuration
│   ├── SKILL.md                    Skill definitions
│   └── references/                 Architecture patterns, checklists
│
├── CMakePresets.json              Standardized build configurations
├── CMakeLists.txt                 Main build definition
├── Doxyfile                       Documentation generation config
├── LICENSE.md                     Community Improvement License
│
├── cmake/                         CMake modules & toolchains
│   ├── CompilerWarnings.cmake     Strict warning flags
│   ├── SafeAPIHelpers.cmake       Helper functions for downstream
│   ├── Toolchain-Linux.cmake      POSIX backend toolchain
│   ├── Toolchain-QNX.cmake        QNX RTOS toolchain
│   └── [other toolchains]
│
├── include/safeapi/               Public API headers (13 modules)
│   ├── types/
│   ├── status/
│   ├── buffer/
│   ├── cast/
│   ├── safestate/
│   ├── string/
│   ├── timer/
│   ├── nvm/
│   ├── memory/
│   ├── task/
│   ├── ipc/
│   ├── log/
│   └── reboot/
│
├── src/                          Implementation (13 modules)
│   ├── status/
│   │   ├── CMakeLists.txt
│   │   └── rte_status.c
│   ├── buffer/
│   ├── cast/
│   ├── safestate/
│   ├── string/
│   ├── timer/
│   ├── nvm/
│   ├── memory/
│   ├── task/
│   ├── ipc/
│   ├── log/
│   └── reboot/
│
├── tests/                        Unit tests (per module)
│   ├── CMakeLists.txt
│   ├── status/test_rte_status.c
│   ├── buffer/test_rte_buffer.c
│   ├── [other test files]
│   └── common/test_rte_safestate.c
│
├── scripts/                      Helper scripts
│   ├── run-cppcheck.sh          Static analysis runner
│   ├── build-linux.sh           Linux build helper
│   ├── build-qnx.sh             QNX build helper
│   └── generate-docs.sh         Documentation generation
│
├── examples/                     Example backends & integrations
│   ├── posix-backend/           POSIX (Linux/development)
│   ├── qnx-backend/             QNX RTOS backend
│   └── rtos-template/           Template for new RTOS
│
├── docs/                         Documentation
│   ├── README.md                Main documentation
│   ├── INTEGRATION.md           Integration guide
│   ├── SAFETY_APPLICATION_CONDITIONS.md
│   ├── EN50128_ALIGNMENT.md
│   ├── PACKAGE_README.md
│   ├── MISRA_COMPLIANCE_REPORT.md
│   │
│   ├── architecture/            Architecture Decision Records
│   │   ├── ADR-001-os-abstraction-layer.md
│   │   ├── ADR-002-cross-layer-data-buffers.md
│   │   ├── ... (7 total)
│   │   └── README.md
│   │
│   ├── requirements/            Requirements specifications
│   │   ├── SRS.md              Consolidated SRS
│   │   └── REQUIREMENTS.md     Traceability matrix
│   │
│   └── templates/              SIL 4 assessment templates
│       ├── README.md
│       ├── 00_DOCUMENT_CHECKLIST.md
│       ├── 01_SAFETY_PLAN.md
│       ├── 06_HARA_TEMPLATE.md
│       ├── 15_CODE_REVIEW_REPORT.md
│       └── 22_SAFETY_CASE.md
│
├── build/                       Build outputs (not version controlled)
│   ├── compile_commands.json   For static analysis
│   ├── libsafeapi_*.a          Static libraries
│   └── test_rte_*             Test executables
│
└── ROADMAP.md                  Future development roadmap
```

### 2.2 Artifact Categories

| Category | Location | Control | Retention |
|----------|----------|---------|-----------|
| **Source Code** | src/, include/ | Git (full history) | Permanent |
| **Tests** | tests/ | Git (full history) | Permanent |
| **Build Config** | CMake*, scripts/ | Git (full history) | Permanent |
| **Documentation** | docs/ | Git (full history) | Permanent |
| **Build Outputs** | build/ | Not version controlled | Per-build |
| **Test Results** | build/test-results/ | Archived per release | 5 years (SIL 4) |
| **Static Analysis** | build/cppcheck-report.* | Archived per release | 5 years (SIL 4) |
| **Coverage Reports** | build/coverage/ | Archived per release | 5 years (SIL 4) |

---

## 3. Build System Configuration

### 3.1 CMake Structure

**Root CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.16)
project(safeAPIFramework VERSION 0.1.0)

# Global settings
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(SAFEAPI_BUILD_TESTS "Build unit tests" ON)
option(SAFEAPI_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

# Per-module subdirectories (ADR-007)
foreach(feature status types buffer cast safestate string
                timer nvm memory task ipc log reboot)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${feature}/CMakeLists.txt")
        add_subdirectory(src/${feature})
    endif()
endforeach()

# Tests
if(SAFEAPI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Static analysis (cppcheck)
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} ...
        COMMENT "Running MISRA C:2012 analysis"
    )
endif()
```

**Per-module CMakeLists.txt:**
```cmake
add_library(safeapi_status STATIC rte_status.c)
target_include_directories(safeapi_status PUBLIC ${CMAKE_SOURCE_DIR}/include)
add_library(safeapi::status ALIAS safeapi_status)
```

### 3.2 CMake Presets (Build Configurations)

**Available presets (CMakePresets.json):**

| Preset | Purpose | Use Case | Tests |
|--------|---------|----------|-------|
| **debug** | Development | Daily development, debugging | ON |
| **release** | Production | Optimized release builds | ON |
| **ci** | Continuous Integration | Automated checks, strict | ON (fail-fast) |
| **asan** | Memory debugging | AddressSanitizer (detect leaks) | ON |
| **ubsan** | Behavior debugging | UndefinedBehaviorSanitizer | ON |
| **coverage** | Test coverage | Code coverage analysis | ON |
| **clang** | Compiler testing | Clang/LLVM compiler | ON |
| **gcc** | Compiler testing | GCC compiler | ON |
| **linux-native** | POSIX backend | Linux development | OFF |
| **qnx** | QNX RTOS | QNX cross-compile | OFF |
| **minimal** | Fast build | Headers-only check | OFF |

**Example usage:**
```bash
# Configure
cmake --preset debug

# Build
cmake --build --preset debug

# Test
ctest --preset debug

# Release build
cmake --preset release && cmake --build --preset release
```

### 3.3 Toolchain Files

**cmake/Toolchain-Linux.cmake:**
```cmake
# POSIX/Linux backend (development)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
add_compile_definitions(SAFEAPI_BACKEND_POSIX)
```

**cmake/Toolchain-QNX.cmake:**
```cmake
# QNX RTOS backend (target hardware)
set(CMAKE_C_COMPILER $ENV{QNX_HOST}/usr/bin/qcc)
set(CMAKE_SYSTEM_NAME QNX)
add_compile_definitions(SAFEAPI_BACKEND_QNX)
```

---

## 4. Version Control Configuration

### 4.1 Git Setup

**Repository initialization:**
```bash
git init
git config user.name "Project Team"
git config user.email "team@railway.example.com"
git config core.filemode true          # Preserve executable bits
git config core.safecrlf true          # Handle line endings
```

**Standard branches:**
```
main/               Production-ready (SIL 4 certified)
develop/            Integration branch (pre-release)
feature/*           Feature branches (short-lived)
bugfix/*            Bug fix branches
release/*           Release preparation
hotfix/*            Emergency fixes to production
```

### 4.2 Commit Message Standard

**Format:**
```
[Category] Short summary (max 60 chars)

Detailed description (wrap at 72 chars):
- Why this change was necessary
- What was changed
- Any breaking changes or side effects

Fixes: #123
Related-To: #456
Co-Authored-By: Name <email@example.com>
```

**Categories:**
```
build:    CMake, build system changes
ci:       GitHub Actions, CI/CD updates
docs:     Documentation changes
feat:     New feature
fix:      Bug fix
perf:     Performance improvements
refactor: Code refactoring (no functional change)
style:    Code style/formatting
test:     Test additions/changes
chore:    Maintenance tasks
```

**Example:**
```
build: Add QNX cross-compilation toolchain

Adds cmake/Toolchain-QNX.cmake for building on QNX RTOS.
Requires QNX_HOST and QNX_TARGET environment variables.
Tested with QNX 7.1.

Fixes: #42
```

### 4.3 .gitignore

```
# Build outputs
build/
CMakeUserPresets.json
*.a
*.o
*.so

# IDE
.vscode/
.idea/
*.swp
*.swo

# Documentation
html/
latex/

# Test coverage
*.gcda
*.gcno
coverage/

# MISRA reports
cppcheck-report.*

# OS
.DS_Store
.Thumbs.db
```

---

## 5. Change Control Process

### 5.1 Change Request Workflow

```
1. IDENTIFY
   Developer identifies issue/feature
   Creates GitHub Issue with description

2. EVALUATE
   Team reviews issue
   Assigns priority (P1/P2/P3/P4)
   Estimates effort

3. PLAN
   Assign to developer/team
   Add to milestone/release
   Link related issues

4. IMPLEMENT
   Create feature branch: git checkout -b feature/issue-42
   Make changes (commits with good messages)
   Write/update tests

5. TEST
   Unit tests passing
   Code review by peer (min. 2 reviewers for SIL 4)
   Static analysis (cppcheck)
   All checks pass

6. VERIFY
   Second reviewer approves changes
   CM manager verifies version labels
   Tests confirmed passing

7. INTEGRATE
   Merge to develop branch
   Close issue
   Update CHANGELOG

8. RELEASE
   Create release tag: v0.1.0
   Build release artifacts
   Tag version in git
```

### 5.2 Change Control Board (CCB)

**Composition:**
- Project Manager (chair)
- Functional Safety Manager
- Lead Architect
- QA/Test Lead
- CM Manager

**Meeting:** Monthly (or as-needed for critical issues)

**Decisions:**
- ✅ Approve/reject change requests
- ✅ Prioritize changes
- ✅ Approve baseline modifications
- ✅ Release authorization

### 5.3 Emergency Changes (Hotfixes)

**Process for critical production bugs:**

```
1. Create hotfix branch: git checkout -b hotfix/security-42
2. Fix and test thoroughly
3. Merge to main (production) and develop (integration)
4. Tag as patch release: v0.1.1
5. Backport to all active releases
6. CCB reviews in next meeting
```

---

## 6. Baselines & Milestones

### 6.1 Configuration Baselines

**Baseline:** Approved, frozen set of artifacts for a phase gate

| Baseline | Artifacts | Approval | Retention |
|----------|-----------|----------|-----------|
| **Requirements Baseline** | SRS, HARA, SafeRS | FSM + Architect | Permanent (git) |
| **Design Baseline** | Architecture, Design Docs, Traceability | Architect | Permanent (git) |
| **Implementation Baseline** | Source code, tests, static analysis | Dev Lead + QA | Permanent (git) |
| **Test Baseline** | Test results, coverage, test reports | QA Lead | 5 years |
| **Assessment Baseline** | Safety case, compliance evidence | FSM | Permanent (assessment archive) |

### 6.2 Release Baselines

**Version scheme:** `MAJOR.MINOR.PATCH`

```
v0.1.0 - Initial framework release (skeleton)
v0.2.0 - Feature: Backend templates
v1.0.0 - First production release (SIL 4 certified)
v1.0.1 - Patch: Security fix
v1.1.0 - Minor: New RTOS backend
v2.0.0 - Major: Architecture redesign (rare)
```

**Tagging:**
```bash
git tag -a v0.1.0 -m "Release: safeAPIFramework v0.1.0"
git push origin v0.1.0
```

---

## 7. Build Process

### 7.1 Standard Build Workflow

```bash
# 1. Configure (applies preset configuration)
cmake --preset debug

# 2. Build (compile all sources)
cmake --build --preset debug

# 3. Test (run all unit tests)
ctest --preset debug

# 4. Analyze (static analysis with MISRA)
./scripts/run-cppcheck.sh

# 5. Coverage (measure test coverage)
cmake --build --preset coverage
lcov --capture --directory build --output-file coverage.info
```

### 7.2 Release Build Process

**Steps:**
1. **Freeze code** — No more commits to release branch
2. **Update version** — CMakeLists.txt, documentation
3. **Tag release** — `git tag v1.0.0`
4. **Build artifacts** — All presets (release, debug, coverage)
5. **Test artifacts** — Verify all builds successful
6. **Generate docs** — Doxygen documentation
7. **Archive evidence** — Test results, analysis reports, coverage
8. **Sign & release** — Create GitHub release with artifacts

**Example:**
```bash
# Create release branch
git checkout -b release/v1.0.0
git push origin release/v1.0.0

# Update version in CMakeLists.txt
sed -i 's/VERSION 0.1.0/VERSION 1.0.0/' CMakeLists.txt
git commit -m "bump: version 0.1.0 → 1.0.0"

# Tag release
git tag -a v1.0.0 -m "Release v1.0.0: SIL 4 ready"
git push origin v1.0.0

# Build all configurations
for preset in debug release asan coverage; do
  cmake --preset $preset
  cmake --build --preset $preset
done

# Archive test results
mkdir -p releases/v1.0.0/test-results
cp build/test-*.xml releases/v1.0.0/test-results/
```

---

## 8. Documentation Control

### 8.1 Documentation Management

**Types of documentation:**

| Document Type | Responsibility | Update Frequency | Version Control |
|---------------|-----------------|------------------|-----------------|
| **Architecture** | Architect | Once per release | Git (tagged) |
| **Requirements** | System Architect | Per phase gate | Git baseline |
| **Design** | Team | Per phase gate | Git baseline |
| **Code comments** | Developers | During implementation | Git (per commit) |
| **Test results** | QA | Per test run | Archived (not git) |
| **Safety case** | FSM | Pre-assessment | Git + Archive |
| **Release notes** | Release Manager | Per release | Git tag annotation |

### 8.2 Documentation Standards

**All documents must include:**
```markdown
---
Document ID: [PROJECT]-[TYPE]-[YY.MM]
Version: [A.B.C]
Date: [Date]
Status: [DRAFT / REVIEW / APPROVED / RELEASED]
Author: [Name]
Reviewed By: [Name]
Approved By: [Name]
Change History:
| Ver | Date | Author | Change |
|-----|------|--------|--------|
| 1.0 | ... | ... | ... |
---
```

---

## 9. Release Management

### 9.1 Release Schedule

**Planned releases:**
```
v0.1.0 (2026-08)  - Initial skeleton (DONE)
v0.2.0 (2026-10)  - Assessment templates complete
v0.3.0 (2026-12)  - Backend examples (POSIX, QNX)
v1.0.0 (2027-06)  - SIL 4 certified release
```

### 9.2 Release Checklist

- [ ] All tests passing (>90% coverage)
- [ ] Static analysis passed (zero critical findings)
- [ ] Code review complete (all files reviewed)
- [ ] Documentation updated (README, Doxygen, ADRs)
- [ ] CHANGELOG.md written
- [ ] Version number updated
- [ ] Tagged in git
- [ ] Release notes created
- [ ] Test artifacts archived (5-year retention)
- [ ] Compliance evidence archived

### 9.3 Artifact Archiving

**Retain for 5 years (SIL 4 requirement):**
- Test execution logs
- Coverage reports
- Static analysis reports
- Build logs
- Commit history (git)

**Archive location:**
```
releases/
├── v0.1.0/
│   ├── build/
│   ├── test-results/
│   ├── coverage-report/
│   ├── cppcheck-report.xml
│   └── release-notes.md
├── v0.2.0/
└── v1.0.0/
```

---

## 10. Access Control & Permissions

### 10.1 Git Repository Permissions

| Role | Permissions | Examples |
|------|-----------|----------|
| **Developer** | Push to feature/* branches, PR | Create features, bug fixes |
| **Code Reviewer** | Review PRs, approve merges | Verify code quality |
| **Release Manager** | Merge to main, create tags | Release authorization |
| **Maintainer** | Full repo access | Overall governance |
| **Public** | Read-only (fork only) | Anyone (community) |

### 10.2 Build System Access

| Role | Permissions |
|------|-----------|
| **Developer** | Local builds (cmake/ctest) |
| **CI/CD** | Automated builds via GitHub Actions |
| **Release Manager** | Create release builds & artifacts |
| **Auditor** | Read-only access to builds/reports |

---

## 11. Configuration Audits

### 11.1 Audit Schedule

**Frequency:** Monthly (or per phase gate for SIL 4 projects)

**Checklist:**
- [ ] All source files under version control
- [ ] No uncommitted changes in main branch
- [ ] All tags properly formatted
- [ ] Build artifacts reproducible
- [ ] Test results archived
- [ ] Documentation up-to-date
- [ ] CMakeLists.txt version matches git tag
- [ ] CHANGELOG.md current
- [ ] Static analysis reports archived

### 11.2 Audit Report

**Template:**
```
Configuration Audit Report
Date: [Date]
Auditor: [Name]

Findings:
- ✓ Source code control: PASS
- ✓ Version labeling: PASS
- ⚠ Documentation: 1 file out of date
- ✓ Build reproducibility: PASS

Issues:
1. [Issue] - [Severity] - [Action] - [Owner] - [Due date]

Approved by: [Functional Safety Manager]
```

---

## 12. Traceability Records

### 12.1 Traceability Matrix

**Links:**
- **Requirements** → Git commit SHAs (in SRS.md)
- **Design** → Architecture docs (ADR-*.md)
- **Implementation** → Source files (src/*.c)
- **Tests** → Test files (tests/*.c)
- **Compliance** → Static analysis reports

**Maintenance:**
- Updated per git commit
- Reviewed at phase gates
- Finalized for assessment

### 12.2 Deviation Log

**Format:**
```
Deviation: [ID]
Rule: MISRA C:2012 Rule 15.5
File: src/status/rte_status.c
Reason: Guard clauses improve readability
Risk: Low
Mitigation: Code review confirms consistency
Approved By: [FSM]
Date: [Date]
```

---

## 13. Tool Configuration

### 13.1 Build Tools

| Tool | Version | Purpose | Config |
|------|---------|---------|--------|
| CMake | 3.16+ | Build system | CMakeLists.txt |
| C Compiler | C99 | Code translation | cmake/Toolchain-*.cmake |
| cppcheck | 2.21+ | MISRA analysis | .cppcheck |
| Git | 2.x | Version control | .gitignore, .gitattributes |

### 13.2 CI/CD (GitHub Actions)

**Workflows (`.github/workflows/`):**
```
- build.yml        Run on every PR (build + test)
- analysis.yml     Run cppcheck (MISRA compliance)
- coverage.yml     Generate coverage reports
- release.yml      Automated release creation
```

---

## 14. Communication & Reporting

### 14.1 Status Reports

**Weekly:**
- Commits since last report
- Current branch status
- Build health (passing/failing tests)
- Static analysis status

**Monthly (CCB meeting):**
- Release progress
- Issues and resolutions
- Upcoming milestones
- Audit findings

### 14.2 Issue Tracking

**GitHub Issues for:**
- Bug reports
- Feature requests
- Documentation updates
- Assessment feedback

**Labels:**
```
bug              Problem to fix
feature          New capability
improvement      Enhancement
documentation    Docs update
assessment       Notified Body feedback
sil-4           SIL 4 relevant
```

---

## 15. Configuration Management Records

### 15.1 Records to Maintain

| Record | Retention | Location |
|--------|-----------|----------|
| Source code history | Permanent | Git repository |
| Build logs | 5 years | releases/ archive |
| Test results | 5 years | releases/ archive |
| Analysis reports | 5 years | releases/ archive |
| Baselines | Permanent | Git tags |
| Change requests | 5 years | GitHub Issues |
| Audit reports | 5 years | CM records |
| Release notes | Permanent | Git releases/ |

### 15.2 Record Retention

**SIL 4 requirement:** 5-year retention minimum
```
releases/v1.0.0/
├── 2027-build-logs.txt
├── 2027-test-results.xml
├── 2027-coverage-report/
├── 2027-cppcheck-report.xml
└── 2027-static-analysis-log.txt
```

---

## 16. Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| CM Manager | [Name] | | |
| Project Manager | [Name] | | |
| Functional Safety Manager | [Name] | | |

---

## 17. Change History

| Version | Date | Author | Change |
|---------|------|--------|--------|
| 0.1 | [Date] | [Name] | Initial draft |
| 1.0 | [Date] | [Name] | Approved by CM Board |

---

## References

- CMakeLists.txt (build definition)
- CMakePresets.json (standardized builds)
- .gitignore (version control exclusions)
- CLAUDE.md (coding standards)
- docs/architecture/ (ADRs)
- ROADMAP.md (development plan)
- LICENSE.md (Community Improvement License)
