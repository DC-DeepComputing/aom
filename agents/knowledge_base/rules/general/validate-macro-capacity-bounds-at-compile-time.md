---
name: libaom/validate-macro-capacity-bounds-at-compile-time
description: 'When defining fixed buffer or stack capacity macros against dependent configuration limits, define explicit constants and validate bounds using #if ... #error rather than AOMMAX'
rubric: readability
severity: should_fix
---

**Rule.** When defining fixed stack, buffer, or array capacity macros that must
accommodate dependent configuration parameters, define the capacity macro
directly as a fixed constant value. Use compile-time preprocessor checks (#if
... #error) to assert that dependent configuration limits do not exceed the
capacity rather than dynamically inflating the capacity macro with AOMMAX().

**Why.** Using expressions like AOMMAX() in static capacity macros couples
buffer allocations to variable configuration parameters and can silently mask
unsupported or out-of-spec build configurations. Defining fixed capacities
directly and validating bounds with preprocessor #if ... #error checks ensures
explicit static sizing and catches invalid configuration combinations at compile
time.

## Examples

Bad:

```cpp
// Bad: Dynamically inflating capacity macro with AOMMAX masks invalid configuration limits
#define MAX_REF_MV_STACK_SIZE (AOMMAX(8, MAX_DRL_BITS + 1))
```

Good:

```cpp
// Good: Define fixed capacity directly and validate dependent limits at compile time
#define MAX_REF_MV_STACK_SIZE 8
#if MAX_DRL_BITS + 1 > MAX_REF_MV_STACK_SIZE
#error "MAX_DRL_BITS too high"
#endif
```
