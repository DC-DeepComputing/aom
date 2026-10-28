---
name: libaom/align-comment-terminology-with-identifiers
description: When documenting configuration settings, struct fields, or API symbols, use exact identifier names and maintain consistent terminology across public headers, CLI flags, and internal structs.
rubric: readability
severity: should_fix
---

**Rule.** When documenting variables, parameters, constants, configuration
settings, or API symbols, use exact, up-to-date identifier names (e.g.,
kColorThresh rather than outdated final_color_thresh, bandwidth rather than
bitrate for target_bandwidth, and AV1E_SET_AUTO_TILES rather than shorthand
SET_AUTO_TILING). Additionally, maintain consistent terminology and descriptions
across public API headers (such as aom/aom_encoder.h), CLI argument help strings
(in av1/arg_defs.c), and internal struct field comments (in
av1/encoder/encoder.h) when exposing or updating configuration options.

**Why.** Inconsistent phrasing or comments referencing nonexistent, outdated, or
colloquial names mislead readers, obscure data flow, create ambiguity about
configuration option semantics, and hinder symbol searches across the codebase.

## Examples

Bad:

```cpp
// Counts of blocks with no more than final_color_thresh colors.
if (n_colors > 1 && n_colors <= kColorThresh) {
  counts_1++;
}

// Always drop for spatial enhancement layer if layer bitrate is 0.
if ((cpi->svc.spatial_layer_id > 0 &&
     cpi->oxcf.rc_cfg.target_bandwidth == 0) ||
    av1_rc_drop_frame(cpi)) {
  return 1;
}

/*!
 * Flag to indicate auto tiling is enabled. The flag means the
 * SET_AUTO_TILING control is enabled...
 */

// In av1/encoder/encoder.h (diverging from CLI help in av1/arg_defs.c):
// Controls how the encoder uses fixed QP offsets.
int use_fixed_qp_offsets;
```

Good:

```cpp
// Counts of blocks with no more than kColorThresh colors.
if (n_colors > 1 && n_colors <= kColorThresh) {
  counts_1++;
}

// Always drop for spatial enhancement layer if layer bandwidth is 0.
if ((cpi->svc.spatial_layer_id > 0 &&
     cpi->oxcf.rc_cfg.target_bandwidth == 0) ||
    av1_rc_drop_frame(cpi)) {
  return 1;
}

/*!
 * Flag to indicate auto tiling is enabled. The flag means the
 * AV1E_SET_AUTO_TILES control is enabled...
 */

// In av1/encoder/encoder.h (matching CLI help in av1/arg_defs.c):
// Controls how the encoder applies fixed QP offsets.
int use_fixed_qp_offsets;
```
