# ADR-006: Safe String Manipulation and Endianness-Safe Buffer Access

Status: Draft
Date: 2026-08-02
Applies to: `include/safeapi/common/sapi_string.h`, additions to
`include/safeapi/common/sapi_buffer.h`

## 1. Context

Two related gaps: (1) the framework had no string handling of its own, so
any application code built on it would fall back to `strcpy`/`strcat`/
`sprintf` - exactly the unbounded, MISRA-flagged functions this project
exists to avoid; (2) `sapi_buffer_t` (ADR-002) copies raw bytes with no
defined multi-byte value byte order, which is a real interoperability gap
for a framework meant to exchange data between layers that could run on
different hardware (NVM data persisted across a firmware/hardware update,
IPC between differently-endian nodes).

## 2. Decision

### 2.1 `sapi_string_t` wraps `sapi_buffer_t`

```c
typedef struct sapi_string_s
{
    sapi_buffer_t buf;   /* buf.length = string length, NOT counting a NUL */
} sapi_string_t;
```

A distinct wrapping struct (not a bare `typedef sapi_buffer_t sapi_string_t`)
so the type system stops a raw byte buffer from being passed where string
semantics are expected and vice versa - consistent with this project's
preference for explicit, checked type boundaries (ADR-003). Internally it
reuses `sapi_buffer_t`'s bounds-checked, caller-owned storage; no new
allocation strategy is introduced.

`buf.length` tracks the string's length **not counting** a NUL terminator.
A NUL is only materialized on demand by `sapi_string_c_str()` (2.3), so a
`sapi_string_t` can hold binary-safe content up to that point and only pays
the "must have room for one more byte" cost when a caller actually needs a
C string to hand to a legacy/third-party API.

### 2.2 Never call `strlen()` on unbounded/untrusted input

`sapi_string_copy()` accepts a plain `const char *src` (for interop with
string literals and legacy C APIs) but never calls `strlen(src)` - `strlen`
has no bound and would defeat the entire point of this module if `src`
turned out not to be NUL-terminated within a safe range. Instead it scans
`src` for a NUL up to `dest`'s capacity and fails with
`SAPI_STATUS_RESOURCE_EXHAUSTED` if none is found in that range. Callers
that already know their exact length (e.g. a length-prefixed field from
`sapi_ipc`) should prefer `sapi_string_copy_n()`, which takes an explicit
length and never scans.

### 2.3 Operations covered

Per the "most usable features developers use daily" framing: bounded
equivalents of `strcpy`/`strcat`/`strlen`/`strcmp`, substring/character
search, numeric formatting (both directions), and splitting.

- `sapi_string_init` / `sapi_string_clear` / `sapi_string_length`
- `sapi_string_c_str` - ensures a NUL terminator is present within
  capacity (without incrementing `length`) and returns the pointer;
  fails `SAPI_STATUS_RESOURCE_EXHAUSTED` if there is no spare byte for it.
- `sapi_string_copy` / `sapi_string_copy_n` / `sapi_string_concat`
- `sapi_string_compare` - bounded `strcmp` equivalent; result returned via
  an output parameter (`int32_t *out_cmp`), not the return value, so the
  return value stays reserved for `sapi_status_t` like every other
  function in the framework.
- `sapi_string_find_char` / `sapi_string_find_substr` - "not found" is
  reported through an `out_found` boolean, not an error status: failing to
  find a substring is a normal outcome, not a fault.
- `sapi_string_from_u32` / `_from_i32` / `_from_u64` / `_from_i64` - base-10
  formatting into a `sapi_string_t` (replacing its content).
- `sapi_string_append_u32` / `_append_i32` / `_append_u64` / `_append_i64` /
  `_append_hex_u32` - base-10 (or lowercase-hex) formatting **appended** to a
  `sapi_string_t`'s existing content, advancing its length. Added
  (REQ-COMMON-STR-029..033) so the reference application can assemble a log
  line or diagnostic string from literal + numeric parts with a checked,
  non-variadic primitive instead of falling back to
  `snprintf(buf, n, "...%u...%x...", ...)` - which is bounded but pulls in
  `<stdio.h>` (MISRA Rule 21.6) and a format string. `sapi_string_concat`
  supplies the literal segments between appends.
- `sapi_string_to_u32` / `_to_i32` / `_to_u64` / `_to_i64` - base-10
  parsing; malformed input or a value that doesn't fit the destination
  type yields `SAPI_STATUS_INVALID_PARAM` / `SAPI_STATUS_VALUE_OUT_OF_RANGE`
  respectively (reusing the same status the `sapi_cast` module uses for
  "doesn't fit," for a consistent meaning across the framework).

### 2.4 Splitting is reentrant, unlike `strtok`

`strtok` keeps hidden internal state, which makes it non-reentrant and
unsafe to interleave across tasks - a well-known reason it's often banned
outright in safety-critical guidelines. `sapi_string_split_next()` instead
takes an explicit, caller-owned cursor:

```c
sapi_status_t sapi_string_split_next(const sapi_string_t *str,
                                      char delimiter,
                                      size_t *io_cursor,
                                      sapi_const_buffer_t *out_token,
                                      bool *out_has_token);
```

`*io_cursor` starts at 0 (caller-initialized) and is advanced by the
function on each call; `out_token` is a zero-copy `sapi_const_buffer_t`
view into `str`'s own storage (valid only as long as `str`'s storage is).
Multiple independent splits of the same or different strings can run
concurrently on different tasks because all state is caller-owned.

### 2.5 Buffer endianness helpers: explicit per call, no default

```c
sapi_status_t sapi_buffer_write_u16_le(sapi_buffer_t *buf, uint16_t value);
sapi_status_t sapi_buffer_write_u16_be(sapi_buffer_t *buf, uint16_t value);
/* ...and the u32/u64 equivalents */

sapi_status_t sapi_buffer_read_u16_le(const sapi_buffer_t *buf, size_t offset, uint16_t *out_value);
sapi_status_t sapi_buffer_read_u16_be(const sapi_buffer_t *buf, size_t offset, uint16_t *out_value);
/* ...and the u32/u64 equivalents */
```

No "default"/"network order" variant is provided - every call site states
`_le` or `_be` explicitly. A hidden default is exactly the kind of
convention someone eventually gets wrong across a team or across years of
a railway signaling system's service life; an explicit suffix costs
nothing at the call site and removes the ambiguity entirely. Byte order is
handled by explicit shifting/masking (not `htons`/`ntohl`-family functions,
which assume a fixed network-order convention this ADR deliberately
avoids, and are not guaranteed available outside POSIX-like environments
this framework's OAL is explicitly meant to abstract away from).

`write_*` functions append at the buffer's current `length` (like
`sapi_buffer_copy_in`) and advance it; `read_*` functions take an explicit
`offset` and do not mutate the buffer, mirroring the existing
append-write/random-access-read asymmetry already implicit in
`sapi_buffer_copy_in`/`sapi_buffer_copy_out`.

## 3. Consequences

- Positive: no application built on this framework has a reason to reach
  for `strcpy`/`strcat`/`sprintf`/`atoi`/`strtok`/`ntohs` again - every
  common daily-use operation from that family has a bounded, checked
  equivalent.
- Positive: endianness is always explicit at the call site; persisted NVM
  data and IPC payloads can now be written/read in a deliberately chosen,
  documented byte order instead of raw host-order `memcpy`.
- Negative: `sapi_string_t`'s "NUL only materialized on demand" model means
  a caller who forgets to call `sapi_string_c_str()` before handing
  `buf.data` to a legacy API directly will not get a NUL-terminated
  buffer - documented prominently rather than silently null-terminating on
  every mutation (which would cost a byte of capacity on every string
  regardless of whether it's ever used as a C string).
- Deferred: no wide-character/UTF-8-aware operations; this module treats
  content as raw bytes/ASCII, matching the rest of the framework's
  byte-oriented design. Unicode handling, if ever needed, is a separate
  future ADR.

## 4. Location

> **Superseded by ADR-007.** See below for the original path; the current
> physical layout is `include/safeapi/string/sapi_string.h` +
> `src/string/sapi_string.c`, target `safeapi::string` (links
> `safeapi::buffer` and `safeapi::cast`).

`include/safeapi/common/sapi_string.h` + `src/common/sapi_string.c`
(added to `safeapi_common`). Endian helpers added directly to
`sapi_buffer.h`/`sapi_buffer.c` (ADR-002) rather than a new file, since
they operate on `sapi_buffer_t` itself.
