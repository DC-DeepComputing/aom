---
name: libaom/omit-redundant-comments-and-use-imperative-phrasing
description: When commenting functions, struct members, or implementation blocks, omit redundant descriptions restating identifiers or caller context, and use imperative phrasing for section headers
rubric: readability
severity: nit
---

**Rule.** Do not write redundant comments that merely narrate self-explanatory
code, repeat function or struct field identifiers, or describe caller-specific
conditions and member paths (such as xd->bd or cm->seq_params.bit_depth).
Reserve function and struct comments for non-obvious invariants, algorithms,
units, valid ranges, or ownership contracts. When writing or retaining section
heading comments to group implementation logic, use active imperative phrasing
(such as 'Set speed features for ...') rather than passive noun phrases (such as
'Speed features for ...').

**Why.** Comments that repeat obvious logic or echo function and field names add
visual clutter and maintenance overhead without providing useful context.
Describing caller-specific conditions or caller struct paths above generic
helpers and structs couples documentation to specific call sites rather than
documenting intrinsic contracts. Using active imperative phrasing for section
headers clearly conveys intended actions and prevents ambiguous noun phrases.

## Examples

Bad:

```cpp
// Bad: Redundant function comment describing caller context
// Set invalid CfL parameters as CfL mode is invalid.
static AOM_INLINE void set_invalid_cfl_parameters(
    uint8_t *best_cfl_alpha_idx, int8_t *best_cfl_alpha_signs) {
  *best_cfl_alpha_idx = CFL_MAX_ALPHA_VAL;
  *best_cfl_alpha_signs = CFL_SIGN_ZERO;
}

// Bad: Restating field names and caller origin paths
typedef struct {
  /*!
   * Equivalent to xd->bd and cm->seq_params.bit_depth
   */
  int bit_depth;
  int width;  // The width
} FrameDimensions;

// Bad: Passive noun phrase section header
// Speed features for the IQ and SSIMULACRA2 tuning modes
if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ ||
    cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
  sf->intra_sf.skip_intra_in_interframe = 0;
}
```

Good:

```cpp
// Good: Self-explanatory helper without redundant caller comments
static AOM_INLINE void set_invalid_cfl_parameters(
    uint8_t *best_cfl_alpha_idx, int8_t *best_cfl_alpha_signs) {
  *best_cfl_alpha_idx = CFL_MAX_ALPHA_VAL;
  *best_cfl_alpha_signs = CFL_SIGN_ZERO;
}

// Good: Clean struct without obvious echo comments
typedef struct {
  int bit_depth;
  int width;
} FrameDimensions;

// Good: Active imperative phrasing for section header
// Set speed features for the IQ and SSIMULACRA2 tuning modes
if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ ||
    cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
  sf->intra_sf.skip_intra_in_interframe = 0;
}
```
