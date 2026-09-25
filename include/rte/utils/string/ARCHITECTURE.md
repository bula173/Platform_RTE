@page string_architecture String Module - Architecture

Bounded string operations via length-limited wrappers over standard libc.
No unbounded operations (strcpy, strcat, sprintf). All operations check
bounds and return status codes.

@section string_architecture_functions Core Functions

rte_strncpy()   - Bounded copy
rte_strncat()   - Bounded append
rte_strnlen()   - Bounded length
rte_strncmp()   - Bounded compare
rte_snprintf()  - Bounded format

@section string_architecture_misra MISRA Rule 21.6

✓ No strcpy, strcat, sprintf
✓ All bounds-checked
✓ Status return codes
