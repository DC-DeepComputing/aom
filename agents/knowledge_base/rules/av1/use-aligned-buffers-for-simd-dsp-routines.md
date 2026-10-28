---
name: libaom/av1/use-aligned-buffers-for-simd-dsp-routines
description: When passing buffers to functions invoking alignment-sensitive SIMD or DSP routines (such as aom_subtract_block), use aligned predictor buffers instead of unaligned motion-vector-offset pointers
rubric: safety
severity: blocking
---

**Rule.** Do not pass unaligned buffer pointers (such as reference frames
indexed by arbitrary motion vector offsets) directly into functions that invoke
SIMD or DSP assembly kernels requiring memory alignment (such as
aom_subtract_block or tpl_get_satd_cost). Instead, construct an aligned
intermediate predictor buffer (e.g., via av1_enc_build_one_inter_predictor or
DECLARE_ALIGNED) and pass the aligned buffer.

**Why.** SIMD assembly implementations (such as SSE2, AVX2, or NEON routines in
aom_subtract_block) use aligned load/store instructions (e.g., MOVA) or assume
strict memory alignment. Passing arbitrary motion-vector-offset pointers into
unaligned reference frame buffers violates these alignment preconditions and
triggers segmentation faults and alignment crashes.

## Examples

Bad:

```cpp
const FULLPEL_MV best_fullmv = get_fullmv_from_mv(&best_rfidx_mv.as_mv);
inter_cost = tpl_get_satd_cost(
    bd_info, src_diff, bw, src_mb_buffer, src_stride,
    &ref_mb[best_fullmv.row * ref_stride + best_fullmv.col], ref_stride,
    coeff, bw, bh, tx_size);
```

Good:

```cpp
av1_enc_build_one_inter_predictor(predictor, bw, &best_rfidx_mv.as_mv,
                                  &inter_pred_params);
inter_cost =
    tpl_get_satd_cost(bd_info, src_diff, bw, src_mb_buffer, src_stride,
                      predictor, bw, coeff, bw, bh, tx_size);
```
