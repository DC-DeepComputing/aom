---
name: libaom/prefer-single-sided-bounds-over-two-sided-clamp
description: When bounding a value where types, constants, or invariants guarantee one limit cannot be violated, use single-sided AOMMIN or AOMMAX instead of two-sided clamp
rubric: readability
severity: should_fix
---

**Rule.** Do not use two-sided bounding functions (clamp(), clamp64(), or
fclamp()) when established arithmetic invariants, unsigned types, or static
constants guarantee that the value cannot violate one of the boundaries. Use
single-sided AOMMAX(val, min) (or fmax) when the upper bound is unreachable, and
AOMMIN(val, max) (or fmin) when the lower bound is unreachable or trivially
satisfied (such as a non-negative unsigned value or positive literal against a
lower bound of 0).

**Why.** Using a two-sided clamp when one bound is statically unreachable,
tautological, or guarded by invariants creates redundant checks, obscures the
actual range constraints, and misleads readers about possible input values.
Using single-sided AOMMIN or AOMMAX documents the true runtime constraint,
avoids dead checks, and simplifies the expression.

## Examples

Bad:

```cpp
// Bad: Redundant lower bound for static positive constant or unsigned value
gf_cfg->lag_in_frames = clamp(39, 0, MAX_LAG_BUFFERS);
uint32_t capped_rate = clamp(bitrate, 0, MAX_BITRATE);

// Bad: Arithmetic invariants guarantee subtraction cannot exceed MAXQ (base_qindex <= MAXQ, boost >= 0)
int sb_qindex = clamp(base_qindex - boost, MINQ + 1, MAXQ);
```

Good:

```cpp
// Good: Single-sided AOMMIN guards only the reachable upper bound
gf_cfg->lag_in_frames = AOMMIN(39, MAX_LAG_BUFFERS);
uint32_t capped_rate = AOMMIN(bitrate, MAX_BITRATE);

// Good: Single-sided AOMMAX guards only the reachable lower bound
int sb_qindex = AOMMAX(base_qindex - boost, MINQ + 1);
```
