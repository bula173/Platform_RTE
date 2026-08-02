# Project Safety & Coding Standards

## Core Rules
- Target: Embedded C (C99 / C11)
- Compliance: MISRA C:2012 (Mandatory & Required rules)
- Safety Lifecycle: CENELEC EN 50128 (SIL 2 / SIL 3 context)

## Coding Conventions
- **No dynamic memory**: `malloc`, `free`, and `realloc` are strictly banned. Use static allocations.
- **Fixed-width types**: Use `<stdint.h>` types (`uint32_t`, `int16_t`) instead of native `int` or `long`.
- **Pointer safety**: Maximum one level of dereferencing where practical; validate all pointers against `NULL` before use.
- **Functions**: Limit function complexity (Cyclomatic Complexity < 10); single point of exit preferred.
- **Type qualifiers**: Use `const` for immutable pointer targets and parameters.

## Documentation & Traceability
- Every public function must document inputs, outputs, error handling, and safety-critical assumptions.
- Link safety requirements to code via comments (e.g., `/* REQ-ID: SR_SW_042 */`), and keep a consolidated
  requirements specification under `docs/requirements/` (e.g. `SRS.md`) as the canonical source those
  comment tags cite — update the spec first when a requirement's wording changes, then the code comment.
- All documentation is kept in Doxygen format. Every public type and function shall have a Doxygen
  comment block with `@brief`, `@param` for every parameter, `@return` describing every possible status/
  value, and pre-conditions/post-conditions/safety-critical assumptions called out in prose where relevant.
  Every file shall have a file-level `@file`/`@brief` block. Keep a project `Doxyfile` at the repo root so
  HTML docs can be generated locally (`doxygen Doxyfile`).

## Prohibited Practices
- No implicit type conversions (explicit casts are required); prefer a checked conversion helper over a
  bare C-style cast so out-of-range values are caught rather than silently truncated.
- No uninitialized variables or recursion.
- No use of standard library `errno` or `assert` in production paths.

## MISRA Compliance Verification
- Every non-trivial change shall be checked against MISRA C:2012 Mandatory & Required rules before being
  considered done, not just at final review.
- Prefer running an automated checker (e.g. `cppcheck --enable=all --addon=misra --std=c99 -I include src`,
  or a licensed tool such as PC-lint Plus / LDRA / Parasoft C/C++test / Polyspace where available) over a
  manual read.
- Maintain a compliance report at `docs/MISRA_COMPLIANCE_REPORT.md`: which rules were checked, how
  (tool output vs. manual review — state plainly when no tool was available), and any deviations with
  rationale. Update it whenever code changes; do not let it go stale relative to the source tree.