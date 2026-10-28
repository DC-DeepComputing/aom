---
name: libaom/cast-operands-before-arithmetic-to-prevent-overflow
description: When performing arithmetic or metric scaling that may exceed 32-bit precision, cast operands or declare accumulators as int64_t/uint64_t before evaluation rather than casting results
rubric: correctness
severity: should_fix
---

**Rule.** When performing arithmetic on narrower integer types (such as 32-bit
integers) that could overflow or wrap, cast at least one operand to a wider
integer type (such as int64_t, uint64_t, or size_t) before the operation, or
declare count and metric accumulators as int64_t directly. Do not cast the
result expression after evaluation (e.g., (uint64_t)(a + b) or
static_cast<int64_t>(w * h)), and do not perform intermediate multiplications of
count accumulators by block dimensions, scaling multipliers, or weighting
constants using 32-bit operands before comparison against 64-bit frame areas or
thresholds, as the calculation will already have wrapped or triggered undefined
signed overflow at narrower precision.

**Why.** In C and C++, arithmetic operations evaluate at operand precision
before any assignment, cast, or comparison. Casting after addition or
multiplication (such as (uint64_t)(a + b)) operates on an already-wrapped
result. Similarly, intermediate multiplications of 32-bit count accumulators by
block dimensions (such as count * kBlockArea * 30 > area) evaluate in 32-bit
signed arithmetic and can overflow on large frame resolutions (e.g., 4K or 8K)
before comparison against a 64-bit operand. Typing accumulators as int64_t or
casting an operand before evaluation (such as (uint64_t)a + b or
static_cast<int64_t>(width) * height) promotes the calculation to 64-bit
precision and avoids undefined integer overflow.

## Examples

Bad:

```cpp
// Bad: Calculation wraps/overflows at narrower 32-bit precision before widening
const uint64_t total = (uint64_t)(max_val + offset);
const int64_t product = static_cast<int64_t>(width * height);

// Bad: 32-bit count overflows during multiplication before comparison with int64_t area
int count_intrabc = 0;
// ... accumulate count ...
cpi->is_screen_content_type = count_intrabc * kBlockArea * 30 > area;
```

Good:

```cpp
// Good: Casting operand promotes calculation to 64-bit precision
const uint64_t total = (uint64_t)max_val + offset;
const int64_t product = static_cast<int64_t>(width) * height;

// Good: Accumulator declared as int64_t ensures 64-bit intermediate multiplication
int64_t count_intrabc = 0;
// ... accumulate count ...
cpi->is_screen_content_type = count_intrabc * kBlockArea * 30 > area;
```
