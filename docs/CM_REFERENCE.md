# Configuration Management Reference

Quick links between the **Configuration Management Plan (03)** and actual project files/structures.

---

## Build System (Section 3)

### CMake Structure
| CM Plan Section | Actual File | Purpose |
|---|---|---|
| 3.1 - Root CMakeLists.txt | [CMakeLists.txt](../CMakeLists.txt) | Main build definition, module iteration |
| 3.1 - Per-module CMakeLists.txt | [src/*/CMakeLists.txt](../src/) | Individual library definitions |
| 3.2 - CMake Presets | [CMakePresets.json](../CMakePresets.json) | 12 standardized configurations |
| 3.3 - Toolchain files | [cmake/Toolchain-*.cmake](../cmake/) | POSIX, QNX, Linux, etc. |

### Available Presets
```bash
# See all presets
cmake --list-presets

# Common presets
cmake --preset debug       # Development
cmake --preset release     # Production
cmake --preset ci         # CI/CD strict
cmake --preset asan       # Memory sanitizer
cmake --preset qnx        # QNX RTOS cross-compile
cmake --preset linux-native # POSIX OSAdapter
```

---

## Version Control (Section 4)

### Git Configuration
| CM Plan Section | Actual File | Purpose |
|---|---|---|
| 4.1 - Git setup | .git/config | User, email, core settings |
| 4.1 - Branch policy | [git](../.) | main, develop, feature/*, release/* |
| 4.2 - Commit messages | .git/hooks | [See commit examples] |
| 4.3 - .gitignore | [.gitignore](../.gitignore) | Exclude build, IDE, OS files |

### Standard Branch Workflow
```
main/       ← Production (v1.0.0)
  ↑
  └← release/v1.0.0 (pre-release testing)
           ↑
develop/   ← Integration
  ↑
  └← feature/new-timer (feature branch)
  └← bugfix/issue-42 (bug fix)
```

---

## Change Control (Section 5)

### Change Request Process
| Step | Tool/File | Action |
|------|-----------|--------|
| 1. IDENTIFY | GitHub Issues | Create issue with description |
| 2. EVALUATE | GitHub Project | Team reviews, assigns priority |
| 3. PLAN | GitHub Milestone | Add to release, estimate effort |
| 4. IMPLEMENT | Git | `git checkout -b feature/issue-42` |
| 5. TEST | CMake/ctest | Run all presets, `cmake --preset ci` |
| 6. VERIFY | Pull Request | 2+ reviewers, pass all checks |
| 7. INTEGRATE | Git | Merge to develop, close issue |
| 8. RELEASE | Git tag | Tag v0.2.0, create release |

### Example Change
```bash
# Start feature
git checkout -b feature/improve-timer

# Make changes
vim src/timer/rte_timer.c
cmake --preset debug && cmake --build --preset debug && ctest --preset debug

# Commit with good message
git commit -m "feat: Add timeout parameter to rte_timer_create

Allows flexible timeout configuration per timer instance instead of
hardcoded value. Improves railway system flexibility.

Fixes: #42"

# Push for review
git push origin feature/improve-timer

# → Create Pull Request on GitHub
# → 2 reviewers approve
# → Merge to develop
# → Close issue #42
```

---

## Baselines & Releases (Section 6)

### Configuration Baselines

| Phase | Baseline | Git Tag | Freeze Point |
|-------|----------|---------|--------------|
| Requirements | SRS approved | `baseline/req-0.1` | After HARA |
| Design | Architecture approved | `baseline/design-0.1` | After ADRs |
| Implementation | Code complete, tests pass | `baseline/impl-0.1` | Before assessment |
| Test | All tests passing, >90% coverage | `baseline/test-0.1` | Before safety case |
| Assessment | Safety case approved | `v1.0.0` | Notified Body ready |

### Release Process
```bash
# 1. Freeze code on release branch
git checkout -b release/v1.0.0
git push origin release/v1.0.0

# 2. Update version
sed -i 's/VERSION 0.1.0/VERSION 1.0.0/' CMakeLists.txt
git commit -m "bump: version 0.1.0 → 1.0.0"

# 3. Tag release
git tag -a v1.0.0 -m "Release v1.0.0: SIL 4 certified"
git push origin v1.0.0

# 4. Build all configurations
cmake --preset release && cmake --build --preset release
cmake --preset asan && cmake --build --preset asan
cmake --preset coverage && cmake --build --preset coverage

# 5. Create GitHub release with artifacts
gh release create v1.0.0 --title "SIL 4 Release" \
  --notes "See CHANGELOG.md"
```

---

## Build Process (Section 7)

### Standard Build Workflow
```bash
# 1. Configure (applies preset)
cmake --preset debug
# → Generates CMakeCache.txt, Makefile, ninja config, etc.

# 2. Build
cmake --build --preset debug
# → Compiles src/ → librte_*.a
# → Compiles tests/ → test_rte_*

# 3. Test
ctest --preset debug
# → Runs 8 test executables
# → Reports pass/fail for each

# 4. Analyze
./scripts/run-cppcheck.sh
# → Runs cppcheck with MISRA addon
# → Outputs violations by rule

# 5. Coverage
cmake --build --preset coverage
# → Instruments code with gcov
# → Can generate coverage report via lcov
```

### Quick Reference
```bash
# Complete dev cycle (1 command)
cmake --preset debug && cmake --build --preset debug && ctest --preset debug

# CI strict mode (fail on warnings)
cmake --preset ci && cmake --build --preset ci && ctest --preset ci

# Memory debugging (find leaks)
cmake --preset asan && cmake --build --preset asan && ctest --preset asan

# Release build
cmake --preset release && cmake --build --preset release

# Cross-compile for QNX
export QNX_HOST=/path/to/qnx/host
export QNX_TARGET=/path/to/qnx/target
cmake --preset qnx && cmake --build --preset qnx
```

---

## Documentation Control (Section 8)

### Documentation Types

| Type | Location | Update Frequency | Control |
|------|----------|------------------|---------|
| Architecture | [docs/architecture/](../docs/architecture/) | Per phase gate | Git (ADR-*.md) |
| Requirements | [docs/requirements/](../docs/requirements/) | Per phase gate | Git (SRS.md) |
| API Docs | Doxygen | Per commit | Auto from comments |
| Build Docs | [README.md](../README.md) | Per release | Git |
| Safety Docs | [docs/](../docs/) | Per assessment | Git (approved docs) |
| Test Results | `build/test-*.xml` | Per test run | Archived (not git) |

### Document Template
```markdown
---
Document ID: [PROJECT]-[TYPE]-[YY.MM]
Version: 1.0
Date: 2026-08-02
Status: [DRAFT / APPROVED]
Author: [Name]
Reviewed By: [Name]
Approved By: [Name]
---

# Document Title

[Content...]

## Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Author | | | |
| Reviewer | | | |
| Approver | | | |
```

---

## Release Management (Section 9)

### Release Schedule
```
v0.1.0 (2026-08) ✓ DONE
  - Initial skeleton
  - 13 modules
  - Basic tests

v0.2.0 (2026-10) [PLANNED]
  - Assessment templates
  - 29 SIL 4 documents
  - Integration guide

v0.3.0 (2026-12) [PLANNED]
  - OSAdapter examples (POSIX, QNX)
  - Cross-compilation toolchains
  - Documentation complete

v1.0.0 (2027-06) [PLANNED]
  - SIL 4 certification
  - Notified Body assessment
  - Production ready
```

### Release Checklist
```bash
# Before tagging release v1.0.0
- [ ] All tests passing (8/8)
- [ ] Coverage >90%
- [ ] Static analysis: zero critical findings
- [ ] Code review: 100% of files reviewed
- [ ] Documentation: up-to-date
- [ ] CHANGELOG.md: written
- [ ] Version bumped: CMakeLists.txt
- [ ] git tag: v1.0.0
- [ ] Artifacts archived: test results, coverage, analysis
- [ ] Release notes: published
```

### Archive Structure
```
releases/
├── v0.1.0/
│   ├── build-debug/
│   │   ├── librte_*.a
│   │   └── test_rte_*
│   ├── test-results/
│   │   └── test-*.xml (8 files)
│   ├── coverage-report/
│   │   ├── index.html
│   │   └── coverage.info
│   ├── cppcheck-report.xml
│   └── release-notes.md
├── v0.2.0/
└── v1.0.0/ ← SIL 4 certified
```

---

## Access Control (Section 10)

### Repository Permissions
```bash
# Developer: create features, send PRs
git checkout -b feature/new-feature
git push origin feature/new-feature
# → Create Pull Request on GitHub

# Reviewer: approve code changes
# → Review code on GitHub PR
# → Approve or request changes

# Release Manager: merge and tag releases
git checkout develop
git merge feature/new-feature
git tag v0.2.0
git push origin v0.2.0

# Maintainer: admin access
# → Manage repository settings
# → Merge critical fixes
```

---

## Configuration Audits (Section 11)

### Monthly Audit Checklist
```bash
# 1. Verify source control
git status                          # Should be clean
git log --oneline | head -5         # Recent commits

# 2. Verify versions
grep "VERSION" CMakeLists.txt        # Version updated?
git describe --tags                 # Latest tag?

# 3. Verify builds
cmake --preset debug && cmake --build --preset debug   # Builds?
ctest --preset debug                # Tests pass?

# 4. Verify documentation
ls -l docs/                         # All docs present?
find . -name "*.md" | wc -l        # Documentation updated?

# 5. Verify archives
ls -l releases/                     # v0.1.0, v0.2.0 artifacts?
```

---

## Tool Configuration (Section 13)

### Build Tools
| Tool | Version | Config File | Purpose |
|------|---------|-------------|---------|
| CMake | 3.16+ | [CMakeLists.txt](../CMakeLists.txt) | Build definition |
| C Compiler | C99 | [cmake/Toolchain-*.cmake](../cmake/) | Code translation |
| cppcheck | 2.21+ | [.cppcheck](../.cppcheck) | MISRA analysis |
| Git | 2.x | [.gitignore](../.gitignore) | Version control |

### CI/CD (GitHub Actions)
```
.github/workflows/
├── build.yml        Runs on every PR (build + test)
├── analysis.yml     cppcheck (MISRA compliance)
├── coverage.yml     Code coverage reports
└── release.yml      Automated release creation
```

---

## Quick Links

| Need | Document | File |
|------|----------|------|
| **Build** | Build instructions | [README.md](../README.md) |
| **Architecture** | Design rationale | [docs/architecture/](../docs/architecture/) |
| **Requirements** | All requirements | [docs/requirements/SRS.md](../docs/requirements/) |
| **MISRA** | Compliance status | [docs/MISRA_COMPLIANCE_REPORT.md](../docs/) |
| **CM Plan** | This template | [docs/templates/03_CONFIGURATION_MANAGEMENT_PLAN.md](templates/03_CONFIGURATION_MANAGEMENT_PLAN.md) |
| **Build presets** | Standardized configs | [CMakePresets.json](../CMakePresets.json) |
| **License** | Community Improvement | [LICENSE.md](../LICENSE.md) |
| **Roadmap** | Future work | [ROADMAP.md](ROADMAP.md) |

---

## Workflow Examples

### Creating a New Feature
```bash
# 1. Create issue on GitHub
# Issue #42: "Add timeout parameter to timer"

# 2. Create feature branch
git checkout develop
git pull
git checkout -b feature/issue-42

# 3. Make changes
vim src/timer/rte_timer.c
vim tests/timer/test_rte_timer.c

# 4. Test locally
cmake --preset debug && cmake --build --preset debug && ctest --preset debug

# 5. Run static analysis
./scripts/run-cppcheck.sh | tee cppcheck-report.txt

# 6. Commit with good message
git commit -m "feat: Add timeout parameter to rte_timer_create

Allows flexible timeout configuration per timer instance.

Fixes: #42"

# 7. Push and create PR
git push origin feature/issue-42
# → Go to GitHub, create Pull Request

# 8. Wait for reviews
# → 2+ reviewers approve

# 9. Merge to develop
git checkout develop
git pull
git merge feature/issue-42
git push origin develop

# 10. Close issue (GitHub: auto-closes on merge)
```

### Preparing Release
```bash
# 1. Create release branch
git checkout -b release/v0.2.0
git push origin release/v0.2.0

# 2. Update version
sed -i 's/VERSION 0.1.0/VERSION 0.2.0/' CMakeLists.txt
git add CMakeLists.txt
git commit -m "bump: version 0.1.0 → 0.2.0"

# 3. Write release notes
cat > CHANGELOG.md << 'EOF'
# v0.2.0 - Assessment Templates

## Features
- Added 5 core SIL 4 assessment templates
- Added Configuration Management Plan
- Added Community Improvement License

## Bug Fixes
- Fixed MISRA Rule X violation
- Improved documentation

## Breaking Changes
- None
EOF

git add CHANGELOG.md
git commit -m "docs: Add release notes for v0.2.0"

# 4. Tag release
git tag -a v0.2.0 -m "Release v0.2.0: Assessment templates"
git push origin v0.2.0

# 5. Merge back to develop
git checkout develop
git merge release/v0.2.0
git push origin develop

# 6. Create GitHub release
gh release create v0.2.0 --title "v0.2.0 - Assessment Templates"

# 7. Archive artifacts
mkdir -p releases/v0.2.0/test-results
cp build/test-*.xml releases/v0.2.0/test-results/
# [Archive other artifacts: coverage, analysis, etc.]
```

---

## Questions?

Refer to:
- Full plan: [Configuration Management Plan](templates/03_CONFIGURATION_MANAGEMENT_PLAN.md)
- Build help: [README.md](../README.md) 
- Architecture: [ADRs](../docs/architecture/)
- License: [LICENSE.md](../LICENSE.md)
