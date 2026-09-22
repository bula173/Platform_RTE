# GitHub Actions Workflows

This directory contains automated CI/CD workflows for the RteFramework.

## Workflows

### 1. **CI Build & Test** (`ci.yml`)

**Trigger:** On every push to `master`/`develop` and pull requests

**What it does:**
- Builds framework with multiple presets (debug, release, ci, asan, ubsan)
- Runs unit tests on each configuration
- Performs MISRA C:2012 static analysis with cppcheck
- Checks code formatting
- Verifies all CMake presets are valid

**Key jobs:**
- `build-and-test` — Multi-preset build matrix
- `linux-native` — Tests Linux build script
- `static-analysis` — MISRA compliance checking
- `code-quality` — Format and style verification
- `build-all-presets` — Validates all available presets

**Artifacts:**
- `misra-report` — Static analysis results (cppcheck output)

**Status badge:**
```markdown
![CI](https://github.com/bula173/Platform_RTE/actions/workflows/ci.yml/badge.svg)
```

---

### 2. **Build & Deploy Documentation** (`docs.yml`)

**Trigger:** 
- On push to `master`/`develop` (if docs-related files changed)
- Manual trigger (`workflow_dispatch`)

**What it does:**
- Installs Doxygen and dependencies
- Generates API documentation from source code comments
- Validates HTML output
- Deploys to GitHub Pages automatically

**Key jobs:**
- `build-docs` — Generates Doxygen documentation
- `deploy-docs` — Publishes to GitHub Pages
- `validate-docs` — Quality checks on generated docs

**Artifacts:**
- `doxygen-documentation` — Generated HTML docs (7-day retention)

**GitHub Pages:**
- Automatically published to: `https://bula173.github.io/Platform_RTE/`
- Only deployed on pushes to `master` or `develop`

**Documentation includes:**
- API reference for all public functions
- Module architecture diagrams (from ADRs)
- Source code browser
- Requirement traceability tags (REQ-*)

---

### 3. **Cross-Compilation Tests** (`cross-compile.yml`)

**Trigger:**
- On push to `master`/`develop` (if build/cmake files changed)
- Manual trigger

**What it does:**
- Tests building for Linux (native and release)
- Tests ARM Linux cross-compilation
- Validates QNX RTOS toolchain configuration
- Verifies all toolchain files are correct
- Validates CMake presets

**Key jobs:**
- `linux-cross-compile-test` — Native + ARM builds
- `qnx-cross-compile-config` — QNX configuration validation
- `toolchain-validation` — Verifies CMake toolchain syntax
- `preset-validation` — Checks CMakePresets.json

**Platforms tested:**
- ✅ Linux (x86_64 native)
- ✅ Linux (ARM cross-compile)
- ⚠️ QNX (config validation only; no QNX SDK in CI)

---

### 4. **Release Build & Quality Gate** (`release.yml`)

**Trigger:** On git tag (e.g., `git tag v0.2.0`)

**What it does:**
- Runs full quality gate (strict CI preset)
- Executes all tests
- Runs MISRA analysis
- Generates and validates documentation
- Builds release artifacts for multiple presets
- Generates release notes from git history
- Publishes documentation to GitHub Pages
- Creates downloadable artifacts

**Key jobs:**
- `quality-gate` — Strict quality checks before release
- `build-artifacts` — Creates release binaries/headers
- `documentation-release` — Publishes docs
- `release-notes` — Generates changelog

**Artifacts created:**
- `release-release` — Optimized release build
- `release-linux-release` — Linux-specific release build
- `release-notes` — Changelog from git history

**Release checklist:**
1. ✅ All tests pass
2. ✅ MISRA analysis clean
3. ✅ Documentation generated
4. ✅ Binaries built and packaged
5. ✅ Published to GitHub Pages

---

## Workflow Behavior Matrix

| Event | CI | Docs | Cross-Compile | Release |
|-------|----|----|---|---|
| Push to master | ✅ | ✅* | ✅* | — |
| Push to develop | ✅ | ✅* | ✅* | — |
| Pull request | ✅ | — | ✅* | — |
| Tag push (v*) | — | — | — | ✅ |
| Manual trigger | — | ✅ | ✅ | ✅ |

*Only if relevant files changed

---

## Monitoring Builds

### GitHub Actions Dashboard
Visit: https://github.com/bula173/Platform_RTE/actions

### Recent Runs
- Check status of latest workflow runs
- Download artifacts
- View logs

### Status Badges
Add to README:
```markdown
![CI](https://github.com/bula173/Platform_RTE/actions/workflows/ci.yml/badge.svg)
![Docs](https://github.com/bula173/Platform_RTE/actions/workflows/docs.yml/badge.svg)
![Cross-Compile](https://github.com/bula173/Platform_RTE/actions/workflows/cross-compile.yml/badge.svg)
```

---

## Troubleshooting

### "Build failed on ubuntu-latest"
- Check system dependencies in workflow (`sudo apt-get install -y ...`)
- Verify CMake version requirements

### "Documentation deployment failed"
- Check GitHub Pages settings in repo Settings → Pages
- Verify `gh-pages` branch exists (created automatically on first deploy)
- Check workflow logs for Doxygen errors

### "MISRA analysis warnings"
- Review `cppcheck-report.xml` artifact
- Add exceptions to `.cppcheck-suppressions` if needed
- Update compliance report: `docs/MISRA_COMPLIANCE_REPORT.md`

### "Preset validation failed"
- Check CMakePresets.json syntax (valid JSON?)
- Verify all referenced toolchain files exist
- Run locally: `cmake --list-presets`

---

## Customization

### Modify CI behavior
Edit `.github/workflows/ci.yml`:
- Add/remove build presets
- Adjust sanitizer flags
- Change test commands

### Adjust documentation settings
Edit `.github/workflows/docs.yml` or `Doxyfile`:
- Change output directory
- Exclude files from docs
- Modify Doxygen settings

### Add new platforms
1. Create `cmake/Toolchain-NewPlatform.cmake`
2. Add presets to `CMakePresets.json`
3. Create test job in `cross-compile.yml`
4. Document in `docs/CROSS_COMPILATION.md`

---

## Performance Notes

- **CI workflow:** ~5-10 minutes (matrix builds run in parallel)
- **Docs workflow:** ~2-3 minutes (Doxygen generation)
- **Cross-compile:** ~5 minutes (multiple platform tests)
- **Release:** ~15-20 minutes (all quality gates + artifacts)

---

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [Doxygen](http://www.doxygen.nl/)
- [cppcheck MISRA](http://cppcheck.sourceforge.net/misra.pdf)
