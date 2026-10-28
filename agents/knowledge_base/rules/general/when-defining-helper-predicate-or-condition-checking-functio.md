---
name: libaom/when-defining-helper-predicate-or-condition-checking-functio
description: When adding or modifying function declarations, helpers, predicates, or specialized variants, provide doc comments detailing purpose, parameters, return semantics, algorithmic strategy, and distinctions from base routines
rubric: readability
severity: should_fix
---

**Rule.** Precede newly added or modified function declarations and definitions
(in both headers and source files), including helpers, predicates, and
specialized variants, with concise documentation comments (such as Doxygen
`/*!\brief ... */` or clear comment blocks) describing their purpose,
parameters, expected inputs/outputs, return value semantics, and side effects.
Specifically:
- **Algorithmic & Heuristic Routines**: For complex image processing, transformation, or heuristic analysis functions, expand the doc comment to detail the high-level strategy, step breakdown, and algorithmic rationale.
- **Bit-Packed & Non-Obvious Parameters**: Explicitly document composite parameters (such as `mode_ctx` or `drl_mode_ctx`) whose bitfield layouts are unpacked via shifts and masks.
- **Specialized Variants & Formats**: When introducing a specialized variant or alternative of an existing routine, document how its behavior or parameters differ from the base routine, explicitly noting operational formats, subsampling modes (e.g., 4:4:4), or tuning configurations rather than relying solely on function names.
- **Predicates, Dispatch Gates & Fallbacks**: Explain evaluation criteria and return values for condition/signaling checks. When gating between alternative implementations, fallback paths, or SIMD variants, explicitly name the concrete function identifiers for each branch and reference underlying rationales (e.g., 'See comment in `<function>()` for detail').

**Why.** Without clear documentation, callers and maintainers must
reverse-engineer intent, parameter contracts, packed bitfield layouts, dispatch
fallback chains, and heuristic algorithms from implementation details. Explicit
doc comments prevent incorrect usage, clarify distinctions between specialized
variants, document bitstream signaling and predicate return semantics, and
ensure interfaces across headers and subsystems remain maintainable.

## Examples

Bad:

```cpp
// 1. Missing strategy/details on complex heuristic or image processing helper:
static void estimate_screen_content_enhanced(AV1_COMP *cpi, FeatureFlags *features);

// 2. Variant and bit-packed parameter without layout or diff documentation:
void av1_convolve_nonsep_cls(const uint8_t *dgd, int width, int height, ...);
static INLINE int16_t drl_mode_ctx(int16_t mode_ctx) {
  const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;
  return GLOBALMV_MODE_CONTEXTS * newmv_ctx + ((mode_ctx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK);
}

// 3. Undocumented header declaration:
void av1_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x, int mi_y, int32_t *output_wmmat);

// 4. Predicate and dispatch gate omitting return semantics, concrete target functions, or rationales:
static INLINE int av1_signal_orip_for_horver_modes(const AV1_COMMON *cm, ...);
// The normative scaler only handles scaling ratios >= 1/4; use non-normative otherwise.
static inline bool av1_has_optimized_scaler(const int src_width, ...);
```

Good:

```cpp
// 1. Doxygen comment explaining purpose, block partitioning, and heuristic strategy:
/*!\brief Estimates if the source frame is a candidate to enable palette mode.
 * Divides luma plane into blocks, counts unique colors and variance, and classifies categories. */
static void estimate_screen_content_antialiasing_aware(AV1_COMP *cpi, FeatureFlags *features);

// 2. Clear distinction from base routine and explicit bitfield layout documentation:
// Convolve using class-based filters. Differs from av1_convolve_nonsep by selecting filter from cls map.
void av1_convolve_nonsep_cls(const uint8_t *dgd, int width, int height, ...);
// mode_ctx packs newmv_ctx, zeromv_ctx, and refmv_ctx bitfields.
static INLINE int16_t drl_mode_ctx(int16_t mode_ctx);

// 3. Documented header function declaration:
// Computes and applies rotation to an existing warping matrix.
int av1_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x, int mi_y);

// 4. Predicate return semantics and dispatch gate naming concrete branch functions and rationale references:
// Returns 1 if enable/disable ORIP is signaled in bitstream, 0 otherwise.
static INLINE int av1_signal_orip_for_horver_modes(const AV1_COMMON *cm, ...);
// Returns true if scaling is supported by normative av1_resize_and_extend_frame() (ratios >= 1/4 and <= 16;
// see comment in aom_scaled_2d_c() for detail). Unsupported ratios fall back to av1_resize_and_extend_frame_nonnormative().
static inline bool av1_has_optimized_scaler(const int src_width, ...);
```
