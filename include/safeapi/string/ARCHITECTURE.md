/**
 * @page string_architecture String Module - Architecture
 *
 * Bounded string operations via length-limited wrappers over standard libc.
 * No unbounded operations (strcpy, strcat, sprintf). All operations check
 * bounds and return status codes.
 *
 * @section string_architecture_functions Core Functions
 *
 * sapi_strncpy()   - Bounded copy
 * sapi_strncat()   - Bounded append
 * sapi_strnlen()   - Bounded length
 * sapi_strncmp()   - Bounded compare
 * sapi_snprintf()  - Bounded format
 *
 * @section string_architecture_misra MISRA Rule 21.6
 *
 * ✓ No strcpy, strcat, sprintf
 * ✓ All bounds-checked
 * ✓ Status return codes
 *
 */

