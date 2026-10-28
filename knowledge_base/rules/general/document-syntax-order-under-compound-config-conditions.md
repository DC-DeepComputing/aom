---
name: libaom/document-syntax-order-under-compound-config-conditions
description: When compound conditional compilation branches (#if CONFIG_*) alter syntax signaling or parsing order across feature flag combinations, add explanatory comments detailing the syntax order
rubric: readability
severity: nit
---

**Rule.** When multiple experiment or feature flags (`CONFIG_*`) interact to
alter bitstream syntax signaling or parsing order across conditional compilation
branches (such as `#if CONFIG_A && !CONFIG_B`), add explanatory comments
detailing the expected syntax order under each flag combination. Document where
syntax elements are signaled relative to adjacent elements both when individual
features are enabled in isolation and when combined.

**Why.** Compound compile-time flags and conditional branching can obscure
bitstream syntax ordering and make decoder/encoder synchronization difficult to
track. Explicit comments clarify the intended bitstream layout across all active
and inactive feature combinations, preventing subtle desynchronization bugs
during specification updates or feature deprecation.

## Examples

Bad:

```cpp
#if CONFIG_MRLS && !CONFIG_ORIP
#if CONFIG_SDP
  if (xd->tree_type != CHROMA_PART)
#endif
    // Parsing reference line index
    mbmi->mrl_index =
        (cm->seq_params.enable_mrls && av1_is_directional_mode(mbmi->mode))
            ? read_mrl_index(ec_ctx, r)
            : 0;
#endif
```

Good:

```cpp
#if CONFIG_MRLS && !CONFIG_ORIP
  // When both ORIP and MRLS are enabled, mrl_index is signaled right after
  // intra mode and before angle delta.
  // When MRLS is enabled but ORIP is disabled, mrl_index is signaled here
  // (after angle delta).
#if CONFIG_SDP
  if (xd->tree_type != CHROMA_PART)
#endif
    // Parsing reference line index
    mbmi->mrl_index =
        (cm->seq_params.enable_mrls && av1_is_directional_mode(mbmi->mode))
            ? read_mrl_index(ec_ctx, r)
            : 0;
#endif
```
