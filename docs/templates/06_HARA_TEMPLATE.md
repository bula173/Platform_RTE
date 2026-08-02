# Hazard Analysis and Risk Assessment (HARA)

**Document ID:** [PROJECT]-HARA-26.08  
**Project:** [System Name]  
**SIL Target:** 4  
**Version:** 1.0  
**Date:** [Date]  
**Status:** [DRAFT / APPROVED]  
**Author:** [Name]  
**Reviewed By:** [Functional Safety Manager]  

---

## 1. Introduction

### 1.1 Purpose
Identify and assess all hazards and failure modes in [System Name] that could lead to unsafe conditions.

### 1.2 Scope
- safeAPIFramework OS Abstraction Layer
- Application logic using safeAPIFramework
- Integration with RTOS
- Excluded: RTOS implementation itself, hardware failures

### 1.3 Method
Risk-based approach per EN 50128:2011 Section 5:
- **Hazard Identification:** What can go wrong?
- **Failure Mode Analysis:** How can it happen?
- **Risk Assessment:** How severe? How likely?
- **Risk Control:** What's the mitigation?

---

## 2. System Context

### 2.1 System Overview
[Brief description of system function, e.g., "ERTMS Radio Block Centre (RBC) - provides movement authority to trains"]

### 2.2 System Boundaries
```
┌────────────────────────────────┐
│  Railway Network Infrastructure │ (Outside scope)
└────────────┬───────────────────┘
             │
    ┌────────▼────────────────┐
    │   [System Under Design] │ ← In scope for HARA
    ├─────────────────────────┤
    │  Application Layer      │
    │  ───────────────────    │
    │  safeAPIFramework OAL   │
    │  ───────────────────    │
    │  RTOS / OS Backend      │ (Partly in scope)
    └────────┬─────────────────┘
             │
    ┌────────▼──────────────────┐
    │  Hardware/Firmware        │ (Outside scope)
    └──────────────────────────┘
```

### 2.3 Key Functions
| Function | SIL | Controlled by | Hazard |
|----------|-----|---------------|--------|
| [Function 1] | 4 | [Component] | [Associated hazard] |
| [Function 2] | 4 | [Component] | [Associated hazard] |

---

## 3. Hazard Identification

### 3.1 Hazards in safeAPIFramework Context

#### H1: Uninitialized Backend Service
**Description:** safeAPIFramework service called before backend registered  
**Cause:** Missing backend registration, wrong initialization order  
**Effect:** NULL pointer dereference, system crash  
**SIL:** 4 (Loss of safety-critical function)  

**Prevention:** 
- [ ] Backend registration in main() before any service use
- [ ] Runtime check for NULL backend pointer
- [ ] Documentation of initialization sequence

**Detection:**
- [ ] Code review of main.c and initialization code
- [ ] Unit test: call service without backend (expect safe error)

---

#### H2: Buffer Overflow in String Operations
**Description:** String operation (sapi_string_*) receives untrusted input  
**Cause:** Application doesn't validate input length, buffer size insufficient  
**Effect:** Memory corruption, security risk, crash  
**SIL:** 4 (Potential for unsafe condition)  

**Prevention:**
- [ ] Application validates all input sizes
- [ ] Buffer sizes documented and static
- [ ] Code review checks for bounded string operations

**Detection:**
- [ ] Static analysis: MISRA rules for buffer operations
- [ ] Fuzzing tests with oversized inputs
- [ ] Code review of all string usage

---

#### H3: Race Condition in IPC (Inter-Process Communication)
**Description:** Concurrent access to IPC queue without synchronization  
**Cause:** RTOS backend doesn't provide mutex/semaphore, application doesn't synchronize  
**Effect:** Message loss, data corruption, undefined behavior  
**SIL:** 4 (Loss of safe communication)  

**Prevention:**
- [ ] RTOS backend implements thread-safe IPC
- [ ] Application code properly synchronizes multi-threaded access
- [ ] Documentation of thread safety assumptions

**Detection:**
- [ ] Thread safety analysis / tool
- [ ] Integration tests with concurrent IPC traffic
- [ ] Code review of IPC usage patterns

---

#### H4: NVM (Non-Volatile Memory) Corruption
**Description:** Power loss or corruption during NVM write leaves data in inconsistent state  
**Cause:** Incomplete write, CRC check fails, corrupted sector  
**Effect:** Lost critical configuration, inconsistent system state  
**SIL:** 4 (Loss of data integrity)  

**Prevention:**
- [ ] NVM backend implements atomic writes (journal/checkpoint)
- [ ] CRC/integrity checks on all NVM data
- [ ] Application has recovery strategy for corrupted data

**Detection:**
- [ ] Integration test: NVM write + verify CRC
- [ ] Failure injection: corrupt NVM data, verify recovery
- [ ] HARA for NVM backend (RTOS responsibility)

---

#### H5: Timer Not Initialized / Timeout Expired
**Description:** Timer service called before initialization, or timeout not met  
**Cause:** Missing timer_create, backend timeout inadequate  
**Effect:** Logic malfunction, missed safety-critical deadlines  
**SIL:** 4 (Potential functional failure)  

**Prevention:**
- [ ] Timer created before first use, documented lifecycle
- [ ] Timeout values verified against safety requirements
- [ ] Watchdog timer for overall system health

**Detection:**
- [ ] Code review: timer initialization sequence
- [ ] Unit test: verify timeout behavior
- [ ] System test: verify timing under load

---

#### H6: Resource Pool Exhaustion (e.g., Task Limit)
**Description:** Dynamic task creation exceeds pool size  
**Cause:** Application doesn't limit tasks, unbounded loop creates tasks  
**Effect:** Task creation fails, critical tasks don't start, system degradation  
**SIL:** 4 (Loss of safety function)  

**Prevention:**
- [ ] Static resource allocation (no dynamic pools where possible)
- [ ] Resource limits documented and enforced
- [ ] Recovery strategy if limit exceeded

**Detection:**
- [ ] Code review: resource allocation patterns
- [ ] Static analysis: find dynamic allocations
- [ ] Test: resource exhaustion scenarios

---

### 3.2 Hazard Summary Table

| ID | Hazard | Cause | Effect | SIL | Existing Control | Additional Mitigation |
|:--:|--------|-------|--------|:---:|------------------|----------------------|
| H1 | Uninitialized Backend | Missing init | Crash | 4 | Code review, unit test | Documentation, runtime check |
| H2 | Buffer Overflow | Untrusted input | Memory corruption | 4 | Bounded operations | Input validation, static analysis |
| H3 | Race Condition (IPC) | No synchronization | Data corruption | 4 | RTOS mutex | Thread safety analysis, tests |
| H4 | NVM Corruption | Power loss | Data loss | 4 | CRC check | Atomic writes, recovery strategy |
| H5 | Timer Timeout Failure | Timer not ready | Missed deadline | 4 | Timer design | Watchdog, timeout verification |
| H6 | Resource Exhaustion | Dynamic creation | Functional loss | 4 | Resource limits | Static allocation, enforcement |

---

## 4. Failure Mode & Effects Analysis (FMEA)

### 4.1 FMEA for safeAPIFramework Services

#### Service: sapi_timer_create()

| Failure Mode | Failure Cause | Failure Effect | Current Control | Risk Priority |
|--------------|---------------|----------------|-----------------|----------------|
| Returns SAPI_STATUS_RESOURCE_EXHAUSTED | No timer slots available | Timer not created, no timeout protection | Pre-allocation of timer pool | Mitigated |
| Returns SAPI_STATUS_INVALID_PARAM | Invalid name/handle pointer | Timer not created, error handling required | Parameter validation | Mitigated |
| NULL backend | Backend not registered | Crash with NULL dereference | Application ensures backend registered | Mitigated |

**Risk Level:** Low (all failure modes have mitigations)

#### Service: sapi_nvm_read()

| Failure Mode | Failure Cause | Failure Effect | Current Control | Risk Priority |
|--------------|---------------|----------------|-----------------|----------------|
| Returns SAPI_STATUS_DATA_CORRUPTION | NVM sector corrupted | Read returns invalid data | CRC check, application validation | Mitigated |
| Timeout during read | Slow NVM backend | Operation blocks indefinitely | Timeout in backend | Mitigated |
| Returns SAPI_STATUS_HARDWARE_FAULT | Underlying NVM failure | Complete loss of NVM access | Application fallback to defaults | Acceptable |

**Risk Level:** Medium (requires backend implementation quality)

[Continue for other safeAPIFramework services...]

---

## 5. Risk Assessment

### 5.1 Risk Matrix

**Severity (Consequence):**
- **Critical (4):** Loss of safety function, injury potential
- **High (3):** Degradation of safety function
- **Medium (2):** Minor functional loss
- **Low (1):** Nuisance

**Likelihood (Probability):**
- **High (H):** Will occur multiple times in system lifetime
- **Medium (M):** Likely to occur
- **Low (L):** Unlikely to occur
- **Rare (R):** Very unlikely

**Risk Score:** Severity × Likelihood
- 16-12: Critical → SIL 4
- 11-9: High → SIL 3
- 8-6: Medium → SIL 2
- <6: Low → SIL 1

### 5.2 Risk Scoring Table

| ID | Hazard | Severity | Likelihood | Risk Score | SIL | Control Effectiveness |
|:--:|--------|:--------:|:----------:|:----------:|:---:|:---------------------:|
| H1 | Uninitialized Backend | 4 (Critical) | R (Rare) | 4 | 1 | Code review → SIL 4 |
| H2 | Buffer Overflow | 4 (Critical) | M (Medium) | 12 | 4 | Static analysis + review |
| H3 | Race Condition | 4 (Critical) | L (Low) | 4 | 1 | Thread safety + tests |
| H4 | NVM Corruption | 4 (Critical) | L (Low) | 4 | 1 | CRC + recovery |
| H5 | Timer Timeout | 4 (Critical) | R (Rare) | 4 | 1 | Verification + watchdog |
| H6 | Resource Exhaustion | 3 (High) | M (Medium) | 9 | 3 | Static limits + checks |

---

## 6. Risk Control Measures

### 6.1 Design Mitigations
- **Layered Architecture:** Application isolated from OS via safeAPIFramework
- **Explicit Error Handling:** Every function returns status code
- **Bounded Operations:** No strcpy, sprintf, dynamic allocation
- **Safe-State Transitions:** SAPI_ASSERT, SAPI_SAFESTATE, SAPI_REBOOT

### 6.2 Implementation Mitigations
- **Code Review:** MISRA + EN 50128 compliance review
- **Static Analysis:** Automated detection of common errors
- **Pointer Validation:** NULL checks on all pointers
- **Initialization Tracking:** Explicit init/fini for all resources

### 6.3 Verification Mitigations
- **Unit Testing:** Individual function correctness
- **Integration Testing:** Module interactions
- **Fault Injection:** Forced failure scenarios
- **Performance Testing:** Timeout, resource limits

### 6.4 RTOS Backend Mitigations
Backend implementation must provide:
- Thread-safe mutexes/semaphores
- Interrupt-safe operations
- CRC verification for storage
- Timeout on all blocking operations
- Resource cleanup on errors

---

## 7. Residual Risk Assessment

**Before Controls:** Risk scores in Section 5.2  
**After Controls:** 

| ID | Hazard | Original SIL | Control Measures | Residual SIL | Acceptable? |
|:--:|--------|:------------:|------------------|:------------:|:-----------:|
| H1 | Uninitialized Backend | 1 | Code review + docs | 4 | ✓ Yes |
| H2 | Buffer Overflow | 4 | Static analysis + review + input validation | 4 | ✓ Yes |
| H3 | Race Condition | 1 | Thread safety analysis + tests | 4 | ✓ Yes |
| H4 | NVM Corruption | 1 | CRC + atomicity + recovery | 4 | ✓ Yes |
| H5 | Timer Timeout | 1 | Verification + watchdog | 4 | ✓ Yes |
| H6 | Resource Exhaustion | 3 | Static allocation + enforcement | 4 | ✓ Yes |

**Conclusion:** All hazards have been reduced to SIL 4 through appropriate controls.

---

## 8. Outstanding Items

| Item | Action | Owner | Target Date | Status |
|------|--------|-------|-------------|--------|
| H2: Fuzzing tests for string operations | Develop test suite | QA | [Date] | Pending |
| H3: Thread safety tool evaluation | Select/configure tool | QA | [Date] | Pending |
| H4: NVM failure injection test | Implement test | Test | [Date] | Pending |

---

## 9. Safety Requirements Derivation

Based on HARA findings, the following safety requirements are derived:

**SR-001:** safeAPIFramework services shall not be called before backend registration  
**SR-002:** All string operations shall use bounded sapi_string_* functions  
**SR-003:** Multi-threaded access to shared resources shall be protected by RTOS synchronization primitives  
**SR-004:** All NVM data shall be protected by CRC/integrity checks  
**SR-005:** Timer values shall be verified against application timing requirements  
**SR-006:** Resource allocations shall not exceed statically-defined limits  

[See SafeRequirements document for full requirements]

---

## 10. Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Functional Safety Manager | | | |
| System Architect | | | |

---

## 11. Change History

| Version | Date | Author | Change |
|---------|------|--------|--------|
| 0.1 | [Date] | [Name] | Initial draft |
| 1.0 | [Date] | [Name] | Approved |

---

## References

- EN 50128:2011 Section 5 (Hazard Analysis)
- EN 50129:2018
- IEC 60812:2018 (Failure Mode and Effects Analysis)
- safeAPIFramework Documentation
