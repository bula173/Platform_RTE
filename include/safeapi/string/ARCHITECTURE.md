/**
 * @page string_architecture String Module - Architecture
 *
 * Bounded string operations via length-limited wrappers over standard libc.\n * No unbounded operations (strcpy, strcat, sprintf). All operations check\n * bounds and return status codes.\n *\n * @section functions Core Functions\n *\n * sapi_strncpy()   - Bounded copy\n * sapi_strncat()   - Bounded append\n * sapi_strnlen()   - Bounded length\n * sapi_strncmp()   - Bounded compare\n * sapi_snprintf()  - Bounded format\n *\n * @section misra MISRA Rule 21.6\n *\n * ✓ No strcpy, strcat, sprintf\n * ✓ All bounds-checked\n * ✓ Status return codes\n *\n */\n
