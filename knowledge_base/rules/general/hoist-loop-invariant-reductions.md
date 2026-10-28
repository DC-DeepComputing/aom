---
name: libaom/hoist-loop-invariant-reductions
description: When evaluating invariant predicates, buffer pointers, sizes, strides, parameter selections, or SIMD reductions inside loops, hoist them into local variables before the loop
rubric: design
severity: should_fix
---

**Rule.** Hoist loop-invariant expressions—including predicate and helper
function results (e.g., is_cfl_allowed(xd), is_cur_buf_hbd(xd),
is_inter_block(mbmi)), configuration checks (e.g.,
cpi->sf.part_sf.partition_search_type), struct buffer pointers (e.g.,
src->buffers[i]), byte sizes, stride calculations, parameter selections (e.g.,
ternary buffer pointer selection with CONVERT_TO_BYTEPTR), and invariant SIMD
reductions (e.g., horizontal_add_u32x4)—outside loop bodies into local const
variables. Do not repeatedly evaluate invariant functions, struct fields, or
arithmetic expressions across loop iterations or inside multi-dimensional array
indices.

**Why.** Re-evaluating invariant predicates, struct fields, array lookups,
stride shifts, or SIMD reductions inside loop bodies introduces redundant
computation and memory operations, clutters inner loop bodies and array indexing
expressions, and obscures loop-varying state from both the compiler and code
reviewers. Hoisting invariant expressions into well-named local variables
clarifies dependencies and improves runtime performance.

## Examples

Bad:

```cpp
// Variant 1: Invariant predicate and helper evaluations in search loops
for (int mode_idx = 0; mode_idx < UV_INTRA_MODES; ++mode_idx) {
  const UV_PREDICTION_MODE mode = uv_mode_list[mode_idx];
  const int mode_rate =
      mode_costs->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode];
  // ...
}

// Variant 2: Recomputing invariant buffer pointers, byte sizes, and config checks
for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
  for (int y = 0; y < dst_h; y += 16) {
    for (int x = 0; x < dst_w; x += 16) {
      memcpy(dst->buffers[i] + y * dst_stride + x,
             src->buffers[i] + y * src_stride + x, (width << 1));
      if (pc_tree->split[i] != NULL) {
        av1_free_pc_tree_recursive(
            pc_tree->split[i], num_planes, 0, 0,
            cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION);
      }
    }
  }
}

// Variant 3: Loop-invariant parameter selection and SIMD reduction inside loops
for (int i = 0; i < num_blocks; ++i) {
  const uint8_t *all_zeros = is_cur_buf_hbd(xd)
      ? CONVERT_TO_BYTEPTR(av1_highbd_all_zeros)
      : av1_all_zeros;
  uint32x4_t vsum = vmlaq_u32(vzero, vsrc[0][0], _1111);
  acc[i] = vf(buf, stride, all_zeros, 0, &sse) + horizontal_add_u32x4(vsum);
}
```

Good:

```cpp
// Variant 1: Hoisted invariant predicate
const CFL_ALLOWED_TYPE cfl_allowed = is_cfl_allowed(xd);
for (int mode_idx = 0; mode_idx < UV_INTRA_MODES; ++mode_idx) {
  const UV_PREDICTION_MODE mode = uv_mode_list[mode_idx];
  const int mode_rate =
      mode_costs->intra_uv_mode_cost[cfl_allowed][mbmi->mode][mode];
  // ...
}

// Variant 2: Hoisted buffer pointers, row byte size, and config check
const size_t row_bytes = (width << 1);
const bool is_var_based =
    (cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION);
for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
  const uint8_t *src_buffer = src->buffers[i];
  uint8_t *dst_buffer = dst->buffers[i];
  for (int y = 0; y < dst_h; y += 16) {
    for (int x = 0; x < dst_w; x += 16) {
      memcpy(dst_buffer + y * dst_stride + x,
             src_buffer + y * src_stride + x, row_bytes);
      if (pc_tree->split[i] != NULL) {
        av1_free_pc_tree_recursive(pc_tree->split[i], num_planes, 0, 0,
                                   is_var_based);
      }
    }
  }
}

// Variant 3: Preselected invariant buffer pointer and precomputed reduction
const uint8_t *all_zeros = is_cur_buf_hbd(xd)
    ? CONVERT_TO_BYTEPTR(av1_highbd_all_zeros)
    : av1_all_zeros;
const uint32x4_t vsum = vmlaq_u32(vzero, vsrc[0][0], _1111);
const uint32_t sum = horizontal_add_u32x4(vsum);
for (int i = 0; i < num_blocks; ++i) {
  acc[i] = vf(buf, stride, all_zeros, 0, &sse) + sum;
}
```
