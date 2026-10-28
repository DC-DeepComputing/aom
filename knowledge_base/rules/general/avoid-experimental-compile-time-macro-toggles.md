---
name: libaom/avoid-experimental-compile-time-macro-toggles
description: 'When introducing or refactoring configurable features, avoid compile-time #define switches or dead #if branches; use runtime configuration or commit the chosen path unconditionally.'
rubric: readability
severity: should_fix
---

**Rule.** Do not leave experimental compile-time `#define` switches, inactive
`#if 0` blocks, or dead `#if`/`#ifdef` conditional paths in production code. If
behavior or algorithmic choices must remain configurable, expose them as runtime
parameters or configuration options; otherwise, remove dead code paths and
implement the chosen logic unconditionally.

**Why.** Preprocessor toggles and dead `#if` branches leave inactive code paths
that become stale, increase cognitive load, and risk bit rot. Eliminating ad-hoc
compile-time switches ensures all committed production code is active and
testable, while keeping genuinely tunable behaviors accessible through
structured runtime settings.

## Examples

Bad:

```cpp
#define REFINE_LEVEL_0 0
#define REFINE_CORRESPONDENCES 1

#if REFINE_CORRESPONDENCES
  refine_flow(src_pyr, flow);
#endif
```

Good:

```cpp
// Stop after level 1; refine correspondences after interpolation.
for (int level = src_pyr->n_levels - 1; level >= 1; --level) {
  // ...
}
refine_flow(src_pyr, flow);
```
