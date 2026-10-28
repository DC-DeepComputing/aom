---
name: libaom/omit-redundant-intermediate-clamping
description: When accumulating arithmetic values in wider types (such as int64_t or double), omit intermediate clamping and perform bounds checks only once before narrowing
rubric: readability
severity: nit
---

**Rule.** When performing multi-step arithmetic using a wider accumulator
variable (such as int64_t or double) before converting or assigning to a
narrower destination type (such as int), omit redundant intermediate clamping or
range checks (such as AOMMIN, AOMMAX, clamp(), or ternary expressions). Perform
bounds clamping once at the end of the calculation prior to narrowing or
returning the value, provided intermediate arithmetic will not overflow the
wider accumulator.

**Why.** Using a wider integer or floating-point type provides sufficient
headroom to prevent overflow during multi-step arithmetic. Repeated intermediate
clamping or ternary checks add visual clutter, redundant branches, and runtime
operations without altering the final result. A single clamp before type
conversion guarantees range safety while keeping accumulation logic clean and
concise.

## Examples

Bad:

```cpp
// Redundant ternary / AOMMIN clamping on intermediate int64_t arithmetic
int64_t frame_target = *this_frame_target;
frame_target += fast_extra_bits;
frame_target = (frame_target < INT_MAX) ? frame_target : INT_MAX;
if (additional_bits > 0) {
  frame_target += additional_bits;
  frame_target = AOMMIN(frame_target, INT_MAX);
}
*this_frame_target = (int)AOMMIN(frame_target, INT_MAX);
```

Good:

```cpp
// Intermediate clamping omitted; clamped once before narrowing to int
int64_t frame_target = *this_frame_target;
frame_target += fast_extra_bits;
if (additional_bits > 0) {
  frame_target += additional_bits;
}
*this_frame_target = (int)AOMMIN(frame_target, INT_MAX);
```
