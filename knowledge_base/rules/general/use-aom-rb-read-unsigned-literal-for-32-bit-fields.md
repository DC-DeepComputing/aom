---
name: libaom/use-aom-rb-read-unsigned-literal-for-32-bit-fields
description: When reading 32-bit unsigned bitstream syntax elements with aom_read_bit_buffer, call aom_rb_read_unsigned_literal instead of splitting into multiple aom_rb_read_literal calls with bit-shifts
rubric: readability
severity: should_fix
---

**Rule.** When parsing 32-bit unsigned bitstream syntax elements (such as f(32)
timing, time scale, or buffer delay fields) with aom_read_bit_buffer, use
aom_rb_read_unsigned_literal(reader, 32) rather than splitting the read across
multiple aom_rb_read_literal() calls and bit-shifting. Reserve
aom_rb_read_literal() for signed values or bit lengths up to 31 bits.

**Why.** aom_rb_read_literal() returns a signed int and asserts bits <= 31,
meaning it cannot directly read full 32-bit unsigned values without triggering
assertions or signed overflow. In contrast, aom_rb_read_unsigned_literal()
returns a uint32_t and accepts up to 32 bits, eliminating manual multi-step
shifts, intermediate variables, and redundant bounds or error checking.

## Examples

Bad:

```cpp
// Anti-pattern: Splitting 32-bit read into multiple 16-bit literal calls
uint32_t high = aom_rb_read_literal(reader, 16);
uint32_t low = aom_rb_read_literal(reader, 16);
uint32_t num_units_in_display_tick = (high << 16) | low;
```

Good:

```cpp
// Correct: Use dedicated 32-bit unsigned literal reader helper
uint32_t num_units_in_display_tick =
    aom_rb_read_unsigned_literal(reader, 32);
```
