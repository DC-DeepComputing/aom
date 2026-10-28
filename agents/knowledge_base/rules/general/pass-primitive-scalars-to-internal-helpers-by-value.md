---
name: libaom/pass-primitive-scalars-to-internal-helpers-by-value
description: When defining internal static helper functions taking primitive scalar values, accept arguments by value rather than by const pointer and dereference at call sites.
rubric: readability
severity: should_fix
---

**Rule.** When defining internal or static helper functions, accept primitive
scalar parameters (such as single integer thresholds, limits, flags, or
dimensions) by value rather than by pointer (e.g., const uint8_t *). When
forwarding from public or RTCD APIs that receive pointer parameters, dereference
the pointers at the call site.

**Why.** Passing primitive scalar types by value avoids unnecessary pointer
indirection and memory loads inside helper functions, simplifies signatures, and
enables compiler optimizations such as register parameter passing.

## Examples

Bad:

```cpp
static void lpf_8_neon(uint8x8_t *p3q3, uint8x8_t *p2q2, uint8x8_t *p1q1,
                       uint8x8_t *p0q0, const uint8_t *blimit,
                       const uint8_t *limit, const uint8_t *thresh) {
  const uint8x8_t thresh_vec = vdup_n_u8(*thresh);
  ...
}

// Call site forwarding from public/RTCD function:
lpf_8_neon(&p3q3, &p2q2, &p1q1, &p0q0, blimit, limit, thresh);
```

Good:

```cpp
static void lpf_8_neon(uint8x8_t *p3q3, uint8x8_t *p2q2, uint8x8_t *p1q1,
                       uint8x8_t *p0q0, const uint8_t blimit,
                       const uint8_t limit, const uint8_t thresh) {
  const uint8x8_t thresh_vec = vdup_n_u8(thresh);
  ...
}

// Call site forwarding from public/RTCD function:
lpf_8_neon(&p3q3, &p2q2, &p1q1, &p0q0, *blimit, *limit, *thresh);
```
