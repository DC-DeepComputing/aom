---
name: libaom/avoid-premature-parameters-and-flag-parsing
description: When refactoring or adding functions, avoid adding unused parameters or speculative flag parsing for unimplemented features; defer them to the feature CL
rubric: change_scope
severity: should_fix
---

**Rule.** Do not add unused function parameters, dead argument plumbing, or
speculative flag/bitmask parsing for features that are not yet enabled or
exercised in the current change. Defer new parameters, struct fields, and
bitmask decoding to the change that actually implements, uses, and tests the
feature.

**Why.** Plumbing unused parameters and adding speculative flag parsing before
the consuming feature is ready introduces dead code, creates maintenance churn,
complicates code review, and risks masking bugs or diverging from the final
feature design.

## Examples

Bad:

```cpp
// Adding unused parameters and parsing bitmask fields before the feature is implemented
static size_t av1_write_metadata_array(AV1_COMP *const cpi, uint8_t *dst,
                                       size_t dst_size,
                                       uint8_t obu_extension_header) {
  const uint32_t metadata_insert_location =
      (current_metadata->insert_flag) & 0x0f;
  const bool is_layer_specific = false;
  return av1_write_obu_header(
      &cpi->ppi->level_params, &cpi->frame_header_count, OBU_METADATA,
      cm->seq_params->has_nonzero_operating_point_idc, is_layer_specific,
      obu_extension_header, dst);
}
```

Good:

```cpp
// Keep signature focused on currently active features; defer extension parameters
static size_t av1_write_metadata_array(AV1_COMP *const cpi, uint8_t *dst,
                                       size_t dst_size) {
  if (current_metadata->insert_flag == AOM_MIF_KEY_FRAME ||
      current_metadata->insert_flag == AOM_MIF_ANY_FRAME) {
    return av1_write_obu_header(
        &cpi->ppi->level_params, &cpi->frame_header_count, OBU_METADATA,
        cm->seq_params->has_nonzero_operating_point_idc,
        /*is_layer_specific_obu=*/false, /*obu_extension_header=*/0, dst);
  }
  return 0;
}
```
