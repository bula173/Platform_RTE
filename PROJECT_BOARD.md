# safeAPIFramework Project Board

Quick visual tracker for feature implementation status. For full details, see [ROADMAP.md](docs/ROADMAP.md) and [FEATURE_EXPANSION.md](docs/FEATURE_EXPANSION.md).

---

## v0.2.0 (Q3 2026) — Core Features & Configuration

### Configuration System
- [ ] #10 CMake Feature Flags Configuration System (P1)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 1 week

### Tier 1: High Priority
- [ ] #1 Hierarchical State Machine (HSM) (P1)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 2-3 weeks
  - Blocks: #5, #3 (Diagnostics can consume HSM events; Watchdog monitors HSM)
  - ADR: ADR-008 (pending)

- [ ] #2 Event/Message Queue (P1)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 1-2 weeks
  - Blocks: None (independent)
  - ADR: ADR-009 (pending)

- [ ] #3 Watchdog & Health Monitor (P1)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 2-3 weeks
  - Depends on: Timer, Reboot (existing)
  - ADR: ADR-010 (pending)

### Tier 2: Testing & Observability
- [ ] #5 Diagnostic Ring Buffer (P2)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 1-2 weeks

- [ ] #7 Checksum & CRC Utilities (P2)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 1 week

- [ ] #9 Mock OSAdapter Harness (P2)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 2 weeks
  - Supports: All other features (testing infrastructure)

**Estimated Effort for v0.2.0:** 10–12 weeks of engineering time

---

## v0.3.0 (Q4 2026) — Advanced Features

### Tier 1: Continuation
- [ ] #4 Protected Data / Synchronized Access (P1)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 2-3 weeks
  - ADR: ADR-011 (pending)

### Tier 2: Configuration & Scheduling
- [ ] #6 Safe Configuration Manager (P2)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 2-3 weeks
  - Depends on: NVM (existing)

- [ ] #8 Cyclic Scheduler / Time Slot Allocator (P2)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 2-3 weeks

### Configuration System (Phase 2)
- [ ] #11 Runtime Feature Registry (P2)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 1-2 weeks
  - Depends on: #10 (CMake Flags must exist first)

**Estimated Effort for v0.3.0:** 8–10 weeks

---

## v0.4.0+ (2027) — Specialized & Certification

### Tier 3: Optional Features
- [ ] #13 Performance Counters (P3)
  - Status: `proposed`
  - Effort: 1-2 weeks
  - Target: v0.4.0 or later

- [ ] #14 Binary Serialization (ASN.1/TLV) (P3)
  - Status: `proposed`
  - Effort: 2-3 weeks
  - Target: v0.4.0 or later

- [ ] #15 CAN Bus Utilities (P3)
  - Status: `proposed`
  - Effort: 1-2 weeks
  - Target: v0.4.0 or later (rail-specific)

- [ ] #16 Authenticated Encryption (AES-GCM Binding) (P3)
  - Status: `proposed`
  - Effort: 1-2 weeks
  - Target: v0.4.0 or later (threat-model dependent)

### Configuration System (Phase 3)
- [ ] #12 Static Config Header (Hybrid Approach) (P2)
  - Status: `proposed` → `designing` → `in-progress` → `testing` → `done`
  - Effort: 1 week
  - Depends on: #10, #11
  - ADR: ADR-019 (pending)

---

## Status Legend

| Status | Meaning |
|--------|---------|
| `proposed` | Initial concept, not yet committed |
| `designing` | Architecture/design phase (ADR in progress) |
| `in-progress` | Active implementation |
| `testing` | Implementation complete, testing/review phase |
| `done` | Merged and released |

---

## How to Update This Board

1. **Check Status:** Review [ROADMAP.md](docs/ROADMAP.md) for latest details
2. **Update Issue:** Move the checkbox from `[ ]` to `[x]` when status advances
3. **Link ADR:** Add link to ADR when design document is created
4. **Update Status:** Change `proposed` → `designing` → `in-progress` → `testing` → `done`
5. **Commit:** Push changes to GitHub

Example:
```markdown
- [x] #1 Hierarchical State Machine (HSM) (P1)
  - Status: `done` ✅ (Released in v0.2.0)
  - Effort: 2-3 weeks
  - ADR: ADR-008
```

---

## Quick Metrics

**Total Issues:** 16 (4 Tier 1 + 5 Tier 2 + 4 Tier 3 + 3 Config System)

**Total Estimated Effort:**
- v0.2.0: 10–12 weeks
- v0.3.0: 8–10 weeks
- v0.4.0+: 8–12 weeks (depends on which Tier 3 features are implemented)

**Critical Path (Longest Dependency Chain):**
1. CMake Feature Flags (#10) → 1 week
2. HSM (#1) → 2–3 weeks (can run in parallel with #10)
3. Watchdog (#3) → 2–3 weeks (depends on #1 for event handling)

**Estimated Timeline for SIL 4 Certification:**
- v0.2.0 + v0.3.0 + v0.4.0 (with Static Config Header) = ~28–32 weeks
- Full certification cycle (design + verification + testing): 6–12 months

---

## Setting Up GitHub Project (Web UI)

To visualize this board in GitHub's native Project interface:

1. Go to https://github.com/bula173/safeAPIFreamwork/projects
2. Click **New Project**
3. Select **Table** or **Kanban** view
4. Name: "safeAPIFramework Roadmap"
5. Add custom fields:
   - **Priority:** Single select (P1, P2, P3)
   - **Effort (weeks):** Number
   - **Target Release:** Single select (v0.2.0, v0.3.0, v0.4.0+)
6. Link Issues #1–#16 to the project
7. Filter/sort by priority and release

This Markdown board serves as a quick reference; the GitHub Project provides visual Kanban/Table views.

---

## References

- [ROADMAP.md](docs/ROADMAP.md) — Detailed roadmap with timelines
- [FEATURE_EXPANSION.md](docs/FEATURE_EXPANSION.md) — Full feature specifications
- [GitHub Issues](https://github.com/bula173/safeAPIFreamwork/issues) — Issue discussions (#1–#16)
- [ADRs](docs/architecture/) — Architecture Decision Records (to be created)
- [SRS](docs/requirements/SRS.md) — Software Requirements Specification (to be updated)
