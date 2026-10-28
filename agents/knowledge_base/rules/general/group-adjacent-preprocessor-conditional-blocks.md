---
name: libaom/group-adjacent-preprocessor-conditional-blocks
description: 'When guarding declarations by identical preprocessor conditions (such as #ifdef __cplusplus or #if AOM_ARCH_*), consolidate them into a single conditional block rather than writing redundant nested or consecutive guards'
rubric: readability
severity: nit
---

**Rule.** Consolidate adjacent declarations, constants, or type definitions
guarded by identical preprocessor conditional directives (such as #ifdef
__cplusplus, #if AOM_ARCH_*, or #if CONFIG_*) into a single conditional
compilation block. Do not write separate, consecutive #if ... #endif blocks for
adjacent items that share the exact same guard condition, and do not place
redundant, duplicate, or consecutively nested identical preprocessor guards
(such as opening #if AOM_ARCH_AARCH64 inside an already-guarded #if
AOM_ARCH_AARCH64 scope). Scope preprocessor guards cleanly and flatly around
only the intended conditional definitions.

**Why.** Fragmenting adjacent declarations across repetitive, consecutive
preprocessor blocks or nesting identical condition directives adds visual noise
and boilerplate. Consolidating identical guards keeps header and source
interfaces clean and readable. Furthermore, nested duplicate directives obscure
which sections remain conditionally compiled when inner #endif directives close,
making subsequent directive nesting confusing and error-prone.

## Examples

Bad:

```cpp
// Bad: Consecutive separate blocks for adjacent declarations sharing identical guard
#ifdef __cplusplus
enum class FrameDropDecision {
  kOk,
  kDrop,
};
#else
enum FrameDropDecision {
  kFrameDropDecisionOk,
  kFrameDropDecisionDrop,
};
#endif  // __cplusplus

#ifdef __cplusplus
static constexpr size_t kAV1MaxLayers = 32;
#else
#define kAV1MaxLayers 32
#endif  // __cplusplus

// Bad: Redundant nested identical preprocessor guards
#if AOM_ARCH_AARCH64
#if AOM_ARCH_AARCH64
static const uint8_t z2_merge_shuffles_u16x4[5][8] = { ... };
#endif  // AOM_ARCH_AARCH64

static AOM_FORCE_INLINE uint16x4x2_t highbd_dr_prediction_z2_gather(...) {
  ...
}
#endif  // AOM_ARCH_AARCH64
```

Good:

```cpp
// Good: Group adjacent items sharing identical condition into a single block
#ifdef __cplusplus
enum class FrameDropDecision {
  kOk,
  kDrop,
};
static constexpr size_t kAV1MaxLayers = 32;
#else
enum FrameDropDecision {
  kFrameDropDecisionOk,
  kFrameDropDecisionDrop,
};
#define kAV1MaxLayers 32
#endif  // __cplusplus

// Good: Flatten redundant guards into a single clean conditional block
#if AOM_ARCH_AARCH64
static const uint8_t z2_merge_shuffles_u16x4[5][8] = { ... };

static AOM_FORCE_INLINE uint16x4x2_t highbd_dr_prediction_z2_gather(...) {
  ...
}
#endif  // AOM_ARCH_AARCH64
```
