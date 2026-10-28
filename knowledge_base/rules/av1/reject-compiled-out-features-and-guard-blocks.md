---
name: libaom/av1/reject-compiled-out-features-and-guard-blocks
description: 'When disabling features under build flags like CONFIG_REALTIME_ONLY, reject feature enablement in codec control handlers and guard entire processing blocks with #if directives'
rubric: correctness
severity: should_fix
---

**Rule.** When disabling or compiling out features under build configuration
flags (such as `CONFIG_REALTIME_ONLY` or `CONFIG_DENOISE`), reject feature
enablement in codec control handlers with an error rather than allowing
configuration silently. In encoder and decoder processing pipelines, guard
entire feature evaluation and state-mutation blocks with `#if !CONFIG_*`, rather
than placing `#if` guards only around inner function calls and leaving
surrounding variables or structs uninitialized.

**Why.** Placing preprocessor guards only around inner function calls leaves
outer state variables uninitialized or inconsistent when condition checks
evaluate to true in builds where the feature is disabled. Rejecting feature
configuration early in control handlers and guarding the entire processing block
guarantees unsupported features fail explicitly at configuration time and
eliminates undefined state.

## Examples

Bad:

```cpp
// In encoder pipeline (encode_strategy.c):
if (!frame_params.show_existing_frame) {
  if (cpi->film_grain_table) {
#if !CONFIG_REALTIME_ONLY
    cm->cur_frame->film_grain_params_present = aom_film_grain_table_lookup(
        cpi->film_grain_table, *time_stamp, *time_end, 0 /* =erase */,
        &cm->film_grain_params);
#endif
  } else {
    cm->cur_frame->film_grain_params_present =
        cm->seq_params->film_grain_params_present;
  }
}
```

Good:

```cpp
// In codec control handler (av1_cx_iface.c):
#if CONFIG_REALTIME_ONLY
  ERROR("film_grain removed from realtime only build.");
#endif

// In encoder pipeline (encode_strategy.c):
#if !CONFIG_REALTIME_ONLY
  if (cpi->film_grain_table) {
    cm->cur_frame->film_grain_params_present = aom_film_grain_table_lookup(
        cpi->film_grain_table, *time_stamp, *time_end, 0 /* =erase */,
        &cm->film_grain_params);
  } else {
    cm->cur_frame->film_grain_params_present =
        cm->seq_params->film_grain_params_present;
  }
#endif
```
