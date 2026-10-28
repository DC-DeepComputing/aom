---
name: libaom/remove-unreachable-dead-code-after-returns
description: Delete unreachable dead code and redundant statements following unconditional returns, breaks, or terminating error handlers like aom_internal_error() and CHECK_MEM_ERROR()
rubric: readability
severity: should_fix
---

**Rule.** Do not leave unreachable dead code, obsolete fallthrough statements,
or redundant return statements immediately following unconditional return
statements, breaks, gotos, exhaustive branching, or terminating error helpers
and macros (such as aom_internal_error(), CHECK_MEM_ERROR(), longjmp, abort(),
or exit()). Delete dead code and unreached branches completely to keep control
flow explicit and clean.

**Why.** Terminating error helpers and unconditional control flow statements
permanently transfer control, exit, or abort execution, rendering any subsequent
statements or branches dead code. Leaving unreachable statements or redundant
returns clutters function bodies, triggers compiler warnings, and misleads
readers into believing execution can continue.

## Examples

Bad:

```cpp
// Variant 1: Redundant return after terminating error macro
if (!equal_dimensions(ref_buf, sd)) {
  aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                     "Incorrect buffer dimensions");
  return cm->error->error_code;
}

// Variant 2: Dead fallthrough branch after unconditional return
if (is_dr_mode) {
  dr_predictor(dst, dst_stride, tx_size, above_row, left_col, upsample_above,
               upsample_left, p_angle);
  return;
}
if (mode == DC_PRED) {
  dc_pred[n_left_px > 0][n_top_px > 0][tx_size](dst, dst_stride, above_row,
                                                left_col);
}
```

Good:

```cpp
// Variant 1: No unreachable return after terminating macro
if (!equal_dimensions(ref_buf, sd)) {
  aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                     "Incorrect buffer dimensions");
}

// Variant 2: Clean control flow without dead branches
assert(is_dr_mode);
dr_predictor(dst, dst_stride, tx_size, above_row, left_col, upsample_above,
             upsample_left, p_angle);
```
