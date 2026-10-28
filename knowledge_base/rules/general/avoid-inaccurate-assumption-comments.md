---
name: libaom/avoid-inaccurate-assumption-comments
description: When writing or modifying code, preprocessor guards, or heuristics, keep comments, doc blocks, TODOs, and spec citations synchronized with actual logic, operators, dimensions, and types
rubric: readability
severity: should_fix
---

**Rule.** Ensure all explanatory comments, doc blocks, test descriptions, table
headers, and specification citations accurately reflect adjacent code
identifiers, logic, values, and architectural invariants:
1. **Identifiers, Operators, and Logic**: Keep variable names, mode/frame contexts (such as intra vs. inter frames), execution prerequisites, dispatch tiers (e.g., SSE2 vs. C helpers), and comparison operators (e.g., describing '>= threshold' rather than '= threshold') synchronized with adjacent code. Avoid tautological explanations.
2. **Preprocessor Conditionals and Guards**: Update or delete comments explaining `#if`, `#ifdef`, `#elif`, or `#ifndef` directives (e.g., `CONFIG_*`, `HAVE_*`) when build flags or guards are modified.
3. **Configurations, Dimensions, and Tables**: Ensure numerical values, frame dimensions (`cfg.g_w`, `cfg.g_h`, `oxcf`), bitrates, filter tap dimensions, band/scaling factors, and constant lookup tables (LUTs) accurately reflect code parameters. Explicitly clarify domain-to-bit mappings for logarithmic/exponential scales (e.g., element count `1 << bits`).
4. **Buffer Ranges, Types, and Function Contracts**: Update comments describing memory offsets, scratch buffer alignments (`aom_memalign`), arithmetic bit ranges upon type widening (e.g., `int64_t`), and doc blocks detailing return values (e.g., `NULL` returns in functions like `od_ec_enc_done`) or ordering invariants.
5. **Specification Citations and Resolved Annotations**: Remove obsolete or resolved `TODO` comments (e.g., after re-enabling assertions). When citing draft specifications (e.g., OBU header requirements), explicitly cite the draft revision or commit hash rather than attributing draft requirements to published release versions.

**Why.** Outdated, contradictory, or misplaced comments, stale preprocessor
descriptions, misleading operators, and incorrect specification citations
mislead reviewers, callers, and future maintainers about actual runtime
constraints, variable mutations, active build configurations, function return
contracts, and bitstream conformance. Synchronizing comments, doc blocks, TODOs,
and specification citations with code changes prevents false assumptions, avoids
masking regressions, and eliminates ambiguity during maintenance and audits.

## Examples

Bad:

```cpp
// Variant 1: Inaccurate comparison operator and dispatch tier / mode.
// Invoke C when skip_cdef_sb = 2 in inter frame.
if (cpi->sf.rt_sf.skip_cdef_sb >= 2) {
  av1_resize_horz_dir_sse2(input, in_stride, intbuf, height, len, width2);
}

// Variant 2: Configuration dimensions mismatch.
// Encode at 640x360 TL1, set keyframe.
cfg.g_w = 256;
cfg.g_h = 64;

// Variant 3: Stale dynamic range comment after widening cast.
// (mean * mean): dynamic range 31 bits.
var = sse - (int)(((int64_t)mean * (int64_t)mean) >> (bwl + 2));

// Variant 4: Stale preprocessor comment and draft spec attribution.
// Allocate buffer even if SMALL_MODEL is true per AV1 spec v1.0.0.
#if CONFIG_INTRA_ENTROPY && !CONFIG_USE_SMALL_MODEL
const int obu_extension_flag = has_nonzero_op_id && is_layer_obu;

// Variant 5: Obsolete TODO left after re-enabling assertion.
// TODO(dev): Re-enable this after failure is addressed.
assert(ms_params->ms_buffers.ref->width == ms_params->ms_buffers.src->width);
```

Good:

```cpp
// Variant 1: Accurate operator, dispatch tier, and mode context.
// Invoke SSE2 when skip_cdef_sb >= 2 in intra frame.
if (cpi->sf.rt_sf.skip_cdef_sb >= 2) {
  av1_resize_horz_dir_sse2(input, in_stride, intbuf, height, len, width2);
}

// Variant 2: Synchronized with configured dimensions.
// Encode at 256x64 TL1, set keyframe.
cfg.g_w = 256;
cfg.g_h = 64;

// Variant 3: Updated comment / TODO reflecting widened range.
// TODO(any): verify (mean * mean) dynamic range in bits after int64_t cast.
var = sse - (int)(((int64_t)mean * (int64_t)mean) >> (bwl + 2));

// Variant 4: Matches active preprocessor guard and cites spec draft commit.
// Allocate buffer when intra entropy is enabled without small model (AV1 draft commit 5e04f).
#if CONFIG_INTRA_ENTROPY && !CONFIG_USE_SMALL_MODEL
const int obu_extension_flag = has_nonzero_op_id && is_layer_obu;

// Variant 5: Obsolete TODO removed upon re-enabling assertion.
assert(ms_params->ms_buffers.ref->width == ms_params->ms_buffers.src->width);
```
