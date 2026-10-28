---
name: libaom/avoid-duplicating-declarations-across-preprocessor-branches
description: 'When preprocessor branches duplicate signatures, call sites, types, flags, or expressions, factor common code outside #if/#else or guard differing tokens inline'
rubric: readability
severity: should_fix
---

**Rule.** Do not duplicate entire variable declarations, function prototypes,
definitions, call sites, statements, or shared expression logic across #if/#else
preprocessor branches when they differ only by a conditional argument,
parameter, sub-expression, or feature flag. Keep common arguments, parameters,
condition operands, and flags outside preprocessor blocks, and conditionally
guard only the differing token or sub-expression inline within a single
statement. For types that vary across architectures or build configurations,
define a single conditional typedef rather than duplicating #if/#else guards
around raw types across function declarations, table definitions, and local
variables. Additionally, eliminate redundant preprocessor directives entirely
when #if and #else branches contain identical code or logic, and do not use
#ifdef __cplusplus or #if defined(__cplusplus) guards in C++ source files (.cc
or .cpp).

**Why.** Duplicating entire function signatures, call sites, declarations,
assignments, or expressions across #if ... #else ... #endif branches introduces
redundant boilerplate, increases visual clutter, and risks logic drift or merge
conflicts when shared terms or parameters are updated in one branch but
forgotten in another. Factoring common logic outside directives, guarding
differing arguments inline, or centralizing conditional type selection in a
typedef keeps code concise, synchronized, and easy to maintain. Furthermore,
identical branches and __cplusplus guards in C++ translation units create dead
code and unnecessary preprocessor noise.

## Examples

Bad:

```cpp
// Bad: Duplicating call sites, parameter lists, or expressions across #if/#else
#if CONFIG_FEATURE
  av1_predict_intra_block(cm, xd, pd->width, pd->height, mode, plane, 0);
  const uint32_t mode_flags = NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT;
#else
  av1_predict_intra_block(cm, xd, pd->width, pd->height, mode, plane);
  const uint32_t mode_flags = NEED_LEFT | NEED_ABOVE;
#endif

// Bad: Duplicating type branching across declarations instead of a typedef
#if AOM_USE_8BIT_WARPED_FILTER_TABLE
extern const int8_t av1_warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8];
#else
extern const int16_t av1_warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8];
#endif

// Bad: Redundant branches with identical code or #ifdef __cplusplus in .cc
#if CONFIG_NEW_REF_SIGNALING
  const RefCntBuffer *const buf = get_ref_frame_buf(cm, rf[1]);
#else
  const RefCntBuffer *const buf = get_ref_frame_buf(cm, rf[1]);
#endif
#ifdef __cplusplus
return FrameDropDecision::kDrop;
#endif
```

Good:

```cpp
// Good: Guarding only the conditional argument/flag inline
  av1_predict_intra_block(cm, xd, pd->width, pd->height, mode, plane
#if CONFIG_FEATURE
                          ,
                          0
#endif
  );
  const uint32_t mode_flags = NEED_LEFT | NEED_ABOVE
#if CONFIG_FEATURE
                              | NEED_ABOVELEFT
#endif
      ;

// Good: Centralize conditional type in a typedef
#if AOM_ARCH_AARCH64 || AOM_ARCH_X86 || AOM_ARCH_X86_64
typedef int16_t WarpedFilterCoeff;
#else
typedef int8_t WarpedFilterCoeff;
#endif
extern const WarpedFilterCoeff av1_warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8];

// Good: Declare identical code unconditionally and omit redundant guards in .cc
  const RefCntBuffer *const buf = get_ref_frame_buf(cm, rf[1]);
  return FrameDropDecision::kDrop;
```
