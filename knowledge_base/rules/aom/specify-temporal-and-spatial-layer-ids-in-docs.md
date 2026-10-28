---
name: libaom/aom/specify-temporal-and-spatial-layer-ids-in-docs
description: When documenting AV1 layer-specific OBUs, metadata, or API features, explicitly specify both temporal and spatial layer IDs rather than an ambiguous singular 'layer id'
rubric: readability
severity: nit
---

**Rule.** When documenting AV1 layer-specific OBUs, metadata flags, or
scalability features in doc comments and API headers, explicitly reference both
temporal and spatial layer IDs (such as `temporal_id` and `spatial_id` or
`temporal_layer_id` and `spatial_layer_id`) rather than referring ambiguously to
a singular 'layer id'.

**Why.** AV1 scalability decouples temporal scalability from spatial
scalability, identifying layers via separate `temporal_id` and `spatial_id`
fields in the OBU extension header and distinct `temporal_layer_id` and
`spatial_layer_id` struct members. Referring generically to a singular 'layer
id' is ambiguous and obscures whether temporal, spatial, or operating point
layers are being addressed.

## Examples

Bad:

```cpp
/**
 * Determines whether metadata is layer-specific (OBU will specify the layer id).
 * Applies metadata only to the specified layer.
 */
```

Good:

```cpp
/**
 * Determines whether metadata is layer-specific (OBU header specifies the
 * temporal and spatial layer ids via temporal_id and spatial_id).
 * Applies metadata only to the current frame's layer.
 */
```
