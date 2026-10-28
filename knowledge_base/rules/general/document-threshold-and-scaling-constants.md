---
name: libaom/document-threshold-and-scaling-constants
description: When defining constants, macros, heuristic thresholds, sizing limits, or buffer allocations, document their algorithmic purpose, units, derivation terms, configurations, datasets, and parity TODOs
rubric: readability
severity: should_fix
---

**Rule.** When defining or modifying named constants, preprocessor macros,
configuration/sizing limits, buffer allocations, heuristic thresholds,
scaling/shift factors, numerical bounds, empirical formulas, or tuning
parameters, annotate their definitions with clear explanatory comments directly
above the declaration describing their purpose, evaluated metric, physical
units, and algorithmic role:

1. **Derivation Terms, Formulas & Padding**: Detail how buffer capacities, sizing macros (e.g., `FRAME_BUFFERS`), buffer allocations, compound formulas, and numerical bounds are derived. For allocations or sizing constants with padding, boundary margins, or filter taps (e.g., `SUBPEL_TAPS`), explain each formula component and use named constants or macro definitions for padding terms and multipliers rather than unexplained numeric literals. When documenting multi-step derivations or bit-shift adjustments (such as `round_offset_shim`), explicitly explain what each term represents rather than using ambiguous positional references like 'outermost' or 'innermost', and keep derivation terms uncollapsed in code so formulas remain traceable.
2. **Constituent Subsets, Configurations & Mode Scope**: For aggregate counts, mode limits (e.g., `LUMA_MODE_COUNT`), or configuration-derived capacities (e.g., supported block sizes, prediction modes, transform types, color formats), explicitly enumerate the supported cases or cite the indexing helper function (e.g., `hash_block_size_to_index()`) or enum mapping configurations to table slots or buffer offsets. For specialized variants (e.g., encoder vs. decoder, real-time vs. all-intra like `FRAME_BUFFERS_ALLINTRA`), document which operating modes rely on each variant.
3. **Domain Semantics, Bounds & Trade-offs**: Document physical or perceptual interpretations of parameter values (e.g., whether higher or lower quantization matrix levels represent flatter matrices), monotonicity, dynamic range bounds (e.g., 32-bit overflow limits), and domain trade-offs (e.g., signaling overhead versus adjustment granularity).
4. **Empirical Metrics & Specific Datasets**: When constants or tuning curves are empirically derived, document the optimization methodology, target quality/speed metrics (e.g., SSIMULACRA 2, VMAF, BD-Rate, PSNR, Butteraugli), and exact test corpus names (e.g., 'Daala\'s subset1', 'CID22', 'AOM CTC') rather than generic terms like 'the test set'.
5. **Citations & Parity TODOs**: Cite authoritative specifications or papers where applicable. When retaining integer truncation, lower precision, or non-standard formulas for parity with an external reference implementation (e.g., SVT-AV1), document the rationale and add an attributed `// TODO(bug_url): ...` or `// TODO(username): ...` with a tracking bug for future investigation.

**Why.** Undocumented constants, macro thresholds, magic multipliers, derivation
shims, unexplained buffer allocation formulas, and heuristic parameters obscure
algorithmic intent, memory requirements, and domain trade-offs. Positional
references like 'outermost' or 'innermost' create ambiguity in nested arithmetic
expressions, while vague labels or missing dataset attributions leave
maintainers unable to understand why specific values or bounds were chosen.
Documenting purpose, physical units, mathematical derivations, filter/padding
terms, constituent subsets, indexing helpers, operating mode scope, evaluation
corpora, and literature links directly in code comments enables maintainers to
safely verify memory bounds, recalibrate heuristics, and refactor tuning models
without regressions, while attributed tracking TODOs ensure reference-parity
compromises remain traceable.

## Examples

Bad:

```cpp
// Variant 1: Undocumented macro counts, buffer sizing, and specialized variants
#define TOP_INTRA_MODEL_COUNT 4
#define LUMA_MODE_COUNT 61
#define FRAME_BUFFERS (REF_FRAMES + 1 + INTER_REFS_PER_FRAME)
#define FRAME_BUFFERS_ALLINTRA 2

// Variant 2: Magic offsets in buffer allocation & unexplained multiplier
CHECK_MEM_ERROR(
    cm, x->upsample_pred,
    aom_memalign(16, (1 + is_highbitdepth) * ((MAX_SB_SIZE + 16) + 16) *
                         MAX_SB_SIZE * sizeof(*x->upsample_pred)));
// 6 is the number of block sizes supported.
#define kMaxAddr (6 << kSrcBits)

// Variant 3: Ambiguous positional label in rounding shift derivation
// The outermost -1 is needed because we halved the filter values.
const int32x4_t round_offset_shim = vdupq_n_s32(
    (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

// Variant 4: Undocumented thresholds, shift factors, and trade-offs
int shift = 3;
int norm_uv_sad_thresh = 100;
int delta_q_res = (qindex >= 160) ? 8 : 1;

// Variant 5: Heuristic tuning & reference parity without corpus or TODO
// Chroma QM levels tuned for SSIMULACRA 2 on subset1.
static inline int aom_get_qmlevel_444_chroma(int qindex, int first, int last) {
  int qmlevel = (qindex <= 40) ? 10 : ((qindex <= 100) ? 9 : 4);
  return clamp(qmlevel, first, last);
}
variances[subb_i * VAR_BOOST_SUBBLOCKS_IN_SB_DIM + subb_j] =
    vf(x->plane[0].src.buf + i * stride + j, stride, av1_all_zeros, 0, &sse) / 64;
```

Good:

```cpp
// Variant 1: Purpose, constituent subsets, derivation, and mode scope documented
// Number of top model rd to store for pruning y modes in intra mode decision.
#define TOP_INTRA_MODEL_COUNT 4
// Total number of luma intra prediction modes (directional + non-directional).
#define LUMA_MODE_COUNT 61
// Buffer count: REF_FRAMES for map, 1 for cur_frame, INTER_REFS_PER_FRAME for scaled refs.
// Used in decoder and GOOD/REALTIME encoder modes (ALLINTRA uses FRAME_BUFFERS_ALLINTRA).
#define FRAME_BUFFERS (REF_FRAMES + 1 + INTER_REFS_PER_FRAME)
#define FRAME_BUFFERS_ALLINTRA 2

// Variant 2: Documented size derivation, named padding constant, and indexing helper
// Output buffer of horizontal filtering in aom_upsampled_pred(). As interpolation
// filter length is SUBPEL_TAPS, allocate (MAX_SB_SIZE + SUBPEL_TAPS) * MAX_SB_SIZE.
CHECK_MEM_ERROR(
    cm, x->upsample_pred,
    aom_memalign(16, (1 + is_highbitdepth) * (MAX_SB_SIZE + SUBPEL_TAPS) *
                         MAX_SB_SIZE * sizeof(*x->upsample_pred)));
// Each of the 6 supported block sizes (4, 8, 16, 32, 64, 128) has its own
// hash table, indexed by the return value of hash_block_size_to_index().
#define kMaxAddr (6 << kSrcBits)

// Variant 3: Explicit derivation term explanation without positional ambiguity
// (The extra -1 shift is needed because we halved the filter values.)
const int32x4_t round_offset_shim = vdupq_n_s32(
    (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

// Variant 4: Evaluated metrics, trade-offs, and multi-threshold conditions documented
// Factor used to scale y_sad down when comparing uv_sad against y_sad.
int shift = 3;
// Normalized UV SAD threshold for low-variance blocks.
int norm_uv_sad_thresh = 100;
// Trade off signaling overhead against delta_q granularity across qindex levels.
int delta_q_res = (qindex >= 160) ? 8 : 1;

// Variant 5: Specific dataset, QM polarity (15=flat, 0=steep), and parity tracking TODO
// Chroma QM levels for SSIMU2 derived on Daala's subset1 to maximize SSIMULACRA 2 scores.
// Higher levels are flatter (15 is flat, 0 is steepest). Ref: https://arxiv.org/pdf/2008.06091.
static inline int aom_get_qmlevel_444_chroma(int qindex, int first, int last) {
  int qmlevel = (qindex <= 40) ? 10 : ((qindex <= 100) ? 9 : 4);
  return clamp(qmlevel, first, last);
}
// TODO(crbug.com/aomedia/375221136): Truncate variances to int for SVT-AV1 parity; use (n+32)/64 later.
variances[subb_i * VAR_BOOST_SUBBLOCKS_IN_SB_DIM + subb_j] =
    vf(x->plane[0].src.buf + i * stride + j, stride, av1_all_zeros, 0, &sse) / 64;
```
