---
name: libaom/derive-feature-macros-directly-from-config-flags
description: 'When defining feature or override macros defaulting to binary CONFIG_* build flags, define the macro directly as CONFIG_* without #if/#else branching'
rubric: readability
severity: should_fix
---

**Rule.** When defining a feature or override preprocessor macro whose value
derives from or tracks a binary `CONFIG_*` build configuration flag (which
evaluates to 0 or 1), define it directly as `#define MACRO CONFIG_FEATURE`
(optionally guarded by `#ifndef MACRO` to allow compile-time overrides) outside
any `#if CONFIG_*` guard rather than splitting into `#if CONFIG_FEATURE` /
`#else` branching blocks or redundant fallback definitions.

**Why.** In libaom, binary `CONFIG_*` preprocessor macros are globally defined
to evaluate to `0` or `1`. Assigning a macro directly to the `CONFIG_*` symbol
cleanly handles both enabled and disabled states without redundant `#if ...
#define 1 #else #define 0 #endif` branching or duplicate fallback definitions,
while preserving override support.

## Examples

Bad:

```cpp
// Redundant #if/#else branching to define 1 or 0
#if CONFIG_PARTITION_SEARCH_ORDER
#define COLLECT_MOTION_SEARCH_FEATURE_SB 1
#else
#define COLLECT_MOTION_SEARCH_FEATURE_SB 0
#endif

// Redundant fallback inside and outside #if
#if CONFIG_PARTITION_SEARCH_ORDER
#ifndef COLLECT_MOTION_SEARCH_FEATURE_SB
#define COLLECT_MOTION_SEARCH_FEATURE_SB CONFIG_PARTITION_SEARCH_ORDER
#endif
#else
#define COLLECT_MOTION_SEARCH_FEATURE_SB 0
#endif
```

Good:

```cpp
// Directly alias the binary CONFIG_* flag (with optional override support)
#ifndef COLLECT_MOTION_SEARCH_FEATURE_SB
#define COLLECT_MOTION_SEARCH_FEATURE_SB CONFIG_PARTITION_SEARCH_ORDER
#endif
```
