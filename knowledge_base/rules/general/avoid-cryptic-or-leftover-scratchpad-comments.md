---
name: libaom/avoid-cryptic-or-leftover-scratchpad-comments
description: When writing or updating C/C++ source code and SIMD helpers, omit cryptic scratchpad notes or draft register labels that do not provide clear documentation.
rubric: readability
severity: nit
---

**Rule.** Do not leave cryptic, obsolete, or scratchpad comments—such as draft
assembly register names (e.g., // s10, // r0, // v0), temporary shorthand
labels, or unexplained scratch notes—in source code or SIMD helper functions.
Ensure all comments explain intent, algorithms, or invariants, and remove draft
scratchpad annotations before submitting.

**Why.** Cryptic trailing comments and draft register annotations from
prototyping or assembly porting clutter the implementation and confuse readers
who cannot deduce their meaning without historical context. Source code comments
must clearly communicate intent or non-obvious logic rather than preserving
developer scratch notes.

## Examples

Bad:

```cpp
// Load 4x4 block of 32-bit values
static inline void load_s32_4x4(const int32_t *s, ptrdiff_t p,
                                int32x4_t *s1, int32x4_t *s2) {
  *s1 = vld1q_s32(s);  // s10
  s += p;
  *s2 = vld1q_s32(s);  // s11
}
```

Good:

```cpp
// Load 4x4 block of 32-bit values
static inline void load_s32_4x4(const int32_t *s, ptrdiff_t p,
                                int32x4_t *s1, int32x4_t *s2) {
  *s1 = vld1q_s32(s);
  s += p;
  *s2 = vld1q_s32(s);
}
```
