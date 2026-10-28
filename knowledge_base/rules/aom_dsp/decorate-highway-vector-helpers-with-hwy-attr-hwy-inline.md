---
name: libaom/aom_dsp/decorate-highway-vector-helpers-with-hwy-attr-hwy-inline
description: When defining inline vector helper functions in Highway SIMD headers or source files, decorate them with HWY_ATTR HWY_INLINE rather than HWY_ATTR alone
rubric: readability
severity: should_fix
---

**Rule.** In Highway SIMD headers and implementations, decorate inline helper
functions inside HWY_NAMESPACE with both HWY_ATTR and HWY_INLINE (e.g., HWY_ATTR
HWY_INLINE). Do not decorate vector helpers with HWY_ATTR alone or use standard
inline without HWY_INLINE.

**Why.** Highway vector helpers require HWY_ATTR to assign target-specific SIMD
ISA attributes during multi-target compilation, and HWY_INLINE (always_inline)
to guarantee inlining into calling SIMD routines without function-call overhead
across target dispatch boundaries.

## Examples

Bad:

```cpp
// Anti-pattern: Missing HWY_INLINE decorator on Highway helper function.
template <typename D>
HWY_ATTR hn::Vec<hn::BlockDFromD<D>> BlockReduceSum(D int_tag,
                                                    hn::VFromD<D> v) {
  return hn::Add(v, v);
}
```

Good:

```cpp
// Correct: Both HWY_ATTR and HWY_INLINE decorators applied.
template <typename D>
HWY_ATTR HWY_INLINE hn::Vec<hn::BlockDFromD<D>> BlockReduceSum(
    D int_tag, hn::VFromD<D> v) {
  return hn::Add(v, v);
}
```
