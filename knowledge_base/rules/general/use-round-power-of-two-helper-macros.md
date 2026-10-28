---
name: libaom/use-round-power-of-two-helper-macros
description: When performing power-of-two rounding, use ROUND_POWER_OF_TWO, ROUND_POWER_OF_TWO_SIGNED, or 64-bit macros from aom_ports/mem.h instead of raw shifts or open-coded rounding offsets
rubric: readability
severity: should_fix
---

**Rule.** When dividing or right-shifting values by a power of two with
rounding, use the standard ROUND_POWER_OF_TWO, ROUND_POWER_OF_TWO_SIGNED,
ROUND_POWER_OF_TWO_64, or ROUND_POWER_OF_TWO_SIGNED_64 helper macros from
"aom_ports/mem.h". Do not use raw bit-shifts, open-code manual rounding offset
calculations (such as ((1 << shift) >> 1) or (1 << (shift - 1))), or retain
stale hardcoded integer offsets when shift amounts vary. Use the _SIGNED
variants whenever the input value can be negative to guarantee correct symmetric
rounding.

**Why.** Standard rounding macros in "aom_ports/mem.h" ensure consistent,
type-appropriate rounding semantics across 32-bit, 64-bit, and signed values
(preventing asymmetry or negative-infinity truncation). They clarify
mathematical intent and eliminate visual clutter and subtle rounding bugs caused
by manual bit-shifts or hardcoded offset constants when shift amounts vary.

## Examples

Bad:

```cpp
// Bad: Hardcoded constant when rshift varies causes rounding errors.
const int rshift = cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2 ? 4 : 2;
const int64_t rdmult = (val + 2) >> rshift;

// Bad: Open-coding manual rounding offset and bit-shift.
const int rounding = (1 << rshift) >> 1;
const int64_t rdmult = (val + rounding) >> rshift;

// Bad: Raw right shift truncates without rounding; manual bias open-coded.
int32_t val = (int32_t)((matrix[2] * cosine_val) + (matrix[3] * sine_val)) >>
              SINE_PRECISION_BITS;
int32_t rounded = (diff + (1 << (shift - 1))) >> shift;
```

Good:

```cpp
#include "aom_ports/mem.h"

// Good: Standard 64-bit rounding macro adapts correctly to variable rshift.
const int rshift = cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2 ? 4 : 2;
const int64_t rdmult = ROUND_POWER_OF_TWO_64(val, rshift);

// Good: Standard ROUND_POWER_OF_TWO and signed helper macros from aom_ports/mem.h.
int32_t val = ROUND_POWER_OF_TWO(
    (matrix[2] * cosine_val) + (matrix[3] * sine_val), SINE_PRECISION_BITS);
int32_t rounded = ROUND_POWER_OF_TWO_SIGNED(diff, shift);
```
