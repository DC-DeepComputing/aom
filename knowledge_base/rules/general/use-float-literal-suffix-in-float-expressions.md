---
name: libaom/use-float-literal-suffix-in-float-expressions
description: Use explicit floating-point literals (e.g., 0.0, 1.0) rather than integers in floating-point contexts, and append the 'f' suffix (e.g., 1.0f) for single-precision floats
rubric: safety
severity: should_fix
---

**Rule.** In floating-point expressions, initializations, comparisons,
assertions, ternary branches, division, and macro constant definitions, use
explicit floating-point literals with a fractional part (e.g., 0.0, 1.0, 10.0)
rather than integer literals (0, 1). For single-precision float variables,
calculations, and math helper calls (e.g., fabsf(), sqrtf(), fclamp()),
explicitly append the 'f' or 'F' suffix (e.g., 0.0f, 1.0f) to literals rather
than using unsuffixed double literals.

**Why.** In C and C++, unsuffixed floating-point literals have type double.
Operating on single-precision float operands with unsuffixed literals implicitly
promotes calculations to double precision, causing unnecessary conversions,
performance overhead, or redundant (float) casts. Conversely, using integer
literals in floating-point contexts obscures intended types, risks unintended
integer truncation during division (e.g., 1 / n), and causes type mismatches in
ternary expressions or assertion comparisons.

## Examples

Bad:

```cpp
// Using integer literals in floating-point init, ternary, division, assertions, or macros:
#define VAR_BOOST_MAX_BOOST 8
double weighted_sum = 0;
double scale = pow(base_scale, (1 / num_mi));
assert(input->buf[i * input->width + j] >= 0);
section->log_coded_error += frame->coded_error ? log(frame->coded_error) : 0;

// Using unsuffixed double literals in float expressions promotes operands to double:
float val = input[i] * 0.5 + 1.0;
uint8_t out = static_cast<uint8_t>(fclamp(output[i] + 0.5, 0.0f, 255.0f));
static INLINE float softsign(float x) { return x / (float)(fabsf(x) + 1.0); }
```

Good:

```cpp
// Explicit floating-point literals clarify types and prevent integer truncation:
#define VAR_BOOST_MAX_BOOST 8.0
double weighted_sum = 0.0;
double scale = pow(base_scale, (1.0 / num_mi));
assert(input->buf[i * input->width + j] >= 0.0);
section->log_coded_error += frame->coded_error ? log(frame->coded_error) : 0.0;

// Suffix 'f' on float literals and single-precision helpers preserve float arithmetic:
float val = input[i] * 0.5f + 1.0f;
uint8_t out = static_cast<uint8_t>(fclamp(output[i] + 0.5f, 0.0f, 255.0f));
static INLINE float softsign(float x) { return x / (fabsf(x) + 1.0f); }
```
