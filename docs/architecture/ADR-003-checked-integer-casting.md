# ADR-003: Checked Integer Casting

Status: Draft
Date: 2026-08-02
Applies to: Platform_RTE, `include/rte/common/rte_cast.h`

## 1. Context

CLAUDE.md prohibits implicit type conversions: "No implicit type conversions
(explicit casts are required)." An implicit or unchecked C-style narrowing
cast (e.g. `uint16_t x = some_uint32;`) can silently truncate a value -
exactly the class of defect MISRA C:2012's essential type model (Rules
10.1-10.8) targets, and unacceptable in SIL 3/4 code where a truncated
value (a timeout, an offset, a message length) can turn into a
safety-relevant misbehavior.

This ADR defines how every conversion between the framework's integer types
is performed: never with a bare C cast at a call site, always through a
named, checked `rte_cast_*` function.

## 2. Decision

### 2.1 Scope: every ordered pair, including widening

A checked function is provided for **every** ordered pair of the project's
integer types, not just the conversions that can lose data. This includes
lossless widening (e.g. `uint8_t -> uint32_t`), so that every conversion
site in the codebase looks the same and is equally easy to find, review,
and grep for - "was this value ever cast without going through
`rte_cast`" is answerable by searching for raw casts, and that search
should have no legitimate hits outside `rte_cast.c` itself.

Types covered: `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`,
`uint16_t`, `uint32_t`, `uint64_t`, and `size_t` (used pervasively by the
framework's own APIs for capacity/length/offset parameters - see
`rte_buffer_t`, `rte_nvm_read/write`, `rte_ipc_send/receive`).

9 types x 8 other types = **72 functions**, named
`rte_cast_<from>_to_<to>`, e.g. `rte_cast_u32_to_u16`,
`rte_cast_i16_to_u8`, `rte_cast_size_to_u32`. Short type tokens: `i8/i16/
i32/i64`, `u8/u16/u32/u64`, `size`.

### 2.2 Signature and failure semantics

```c
rte_status_t rte_cast_u32_to_u16(uint32_t in, uint16_t *out);
```

- `out == NULL` -> `RTE_STATUS_INVALID_PARAM`, nothing dereferenced.
- Value does not fit the destination type -> `RTE_STATUS_VALUE_OUT_OF_RANGE`
  (new status code, appended to `rte_status_t`; see section 2.4), and
  **`*out` is left untouched** - the caller's variable keeps whatever value
  it held before the call. This means callers must always check the
  returned status before using `*out`; the function makes no attempt to
  provide a "safe default" on failure, since a silently-substituted default
  (e.g. clamping or zeroing) could itself be mistaken for a valid value on
  a safety-relevant path.
- Value fits -> `RTE_STATUS_OK`, `*out` set to the converted value.

### 2.3 Range-check algorithm (why it's correct without triggering `-Wsign-conversion`)

Every function reduces the problem to a single comparison against the
destination type's `_MIN`/`_MAX` limit macros, performed in whichever of
`int64_t`/`uint64_t` matches the **source's** signedness (both are wide
enough to hold any of the 9 covered types without loss, since none exceed
64 bits):

- Unsigned source: widen `in` to `uint64_t` (always lossless). If the
  destination is unsigned, safe iff `in_widened <= (uint64_t)DST_MAX`. If
  the destination is signed, safe iff `in_widened <= (uint64_t)DST_MAX`
  (a signed `DST_MAX` is always representable as `uint64_t`).
- Signed source: widen `in` to `int64_t` (always lossless). If the
  destination is unsigned, safe iff `in_widened >= 0` (checked first, so
  the following cast is only evaluated once non-negative is established)
  `&& (uint64_t)in_widened <= (uint64_t)DST_MAX`. If the destination is
  signed, safe iff `in_widened >= (int64_t)DST_MIN && in_widened <=
  (int64_t)DST_MAX`.

This is one of four small templates (source {signed, unsigned} x
destination {signed, unsigned}), instantiated per pair using each
destination type's standard `<stdint.h>`/`<stddef.h>` limit macros
(`INT8_MIN/MAX` ... `UINT64_MAX`, `SIZE_MAX`). Because the comparison is
always performed after widening to a single 64-bit type matching the
*source's* signedness, no branch ever compares a signed value against an
unsigned value of possibly-different rank, which is what triggers spurious
`-Wsign-compare`/`-Wsign-conversion` warnings/UB risk with naive casts.

### 2.4 New status code

```c
RTE_STATUS_VALUE_OUT_OF_RANGE = 11  /* appended, does not renumber existing values */
```

### 2.5 Why named functions instead of macros or C11 `_Generic`

The project targets C99 (`CMAKE_C_STANDARD 99`, matching qualified SIL 3/4
toolchains), so `_Generic` (C11) is unavailable. Even where a toolchain
does support it, function-like macros are avoided for this kind of check:
macro arguments can be evaluated multiple times (a hazard if a future call
site passes an expression with side effects), and 72 explicit, individually
named, individually documented functions are easier for a reviewer or a
static analysis / MISRA deviation review to reason about one at a time than
a generic macro whose behavior depends on the calling context.

### 2.6 Generation, not hand-authorship

The 72 function bodies all follow one of the four templates in 2.3
mechanically - only the destination limit macros and type names change per
function. To eliminate the transcription errors that come with hand-typing
~70 near-identical functions, the header and source were produced by a
small generator script from that template, then compiled and boundary-value
tested. The generator itself is not part of the shipped build; only its
plain, ordinary-looking C output is. This is a one-time authoring aid, not
a build-time code generation step - the framework's build system does not
gain a dependency on it.

## 3. Consequences

- Positive: every narrowing, sign-changing, and widening conversion in the
  codebase is explicit, named, checked, and traceable to one file; a raw
  C-style cast between these types anywhere else in the codebase is now a
  code-review red flag.
- Positive: uniform failure semantics (status-only signaling, untouched
  output on failure) matches every other API in the framework - no special
  case to remember.
- Negative: call sites become more verbose (`rte_status_t st =
  rte_cast_u32_to_u16(x, &y); if (st != RTE_STATUS_OK) { ... }` instead of
  `uint16_t y = (uint16_t)x;`). Accepted as the correct trade for SIL 3/4
  rigor; this is the same trade CLAUDE.md already makes by banning implicit
  conversions outright.
- Deferred: existing service headers (`rte_nvm`, `rte_ipc`, `rte_memory`,
  `rte_buffer`) are not retrofitted to call `rte_cast_*` internally in
  this change - their current bodies don't perform any narrowing casts
  today. Any future code that does convert between these types (in this
  framework or in RBC core code built on top of it) is expected to go
  through `rte_cast_*` rather than a bare cast.

## 4. Location

> **Superseded by ADR-007.** See below for the original path; the current
> physical layout is `include/rte/cast/rte_cast.h` +
> `src/cast/rte_cast.c`, target `rte::cast`.

`include/rte/common/rte_cast.h` + `src/common/rte_cast.c`, added to
the existing `rte_common` library target (see ADR-002).
