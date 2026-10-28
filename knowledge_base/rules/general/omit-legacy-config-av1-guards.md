---
name: libaom/omit-legacy-config-av1-guards
description: 'When adding or modifying codec or DSP functions, do not wrap code in legacy #if CONFIG_AV1 preprocessor guards'
rubric: readability
severity: should_fix
---

**Rule.** Do not wrap DSP or AV1 codec functions and definitions in `#if
CONFIG_AV1` or `#ifdef CONFIG_AV1` preprocessor guards. In libaom, AV1 is the
base codec rather than an optional toggleable module, so bare `CONFIG_AV1`
guards are redundant legacy boilerplate from libvpx.

**Why.** In libaom, AV1 functionality is core and always enabled. Legacy
`CONFIG_AV1` preprocessor guards from the libvpx fork are obsolete, add
unnecessary code clutter, and have been eliminated project-wide.

## Examples

Bad:

```cpp
#if CONFIG_AV1

static INLINE void comp_mask_pred_16_ssse3(...) {
  // ...
}

void aom_comp_mask_pred_ssse3(...) {
  // ...
}

#endif  // CONFIG_AV1
```

Good:

```cpp
static INLINE void comp_mask_pred_16_ssse3(...) {
  // ...
}

void aom_comp_mask_pred_ssse3(...) {
  // ...
}
```
