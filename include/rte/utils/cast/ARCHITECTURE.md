@page cast_architecture Cast Module - Architecture

Range-checked type conversions. Validates value fits destination type before
converting. Returns RTE_STATUS_VALUE_OUT_OF_RANGE if out of bounds.

@section cast_architecture_functions Conversion Functions

rte_cast_i8_to_i16() - From int8_t to int16_t (and variants)
Return: OK if fits, VALUE_OUT_OF_RANGE otherwise

@section cast_architecture_strategy Check-Before-Convert

1. Check if value in destination range
2. If yes: convert and return OK
3. If no: don't modify output, return error

@section cast_architecture_misra MISRA Rule 10.1

✓ No implicit conversions
✓ Explicit checked casts only
