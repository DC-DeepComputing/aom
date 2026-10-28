---
name: libaom/av1/group-related-bitstream-syntax-elements
description: When reading or writing bitstream syntax elements in decoder and encoder functions, keep related parameters grouped together without interleaving unrelated fields
rubric: readability
severity: should_fix
---

**Rule.** In bitstream reader and writer functions, keep syntax elements
belonging to the same functional feature or entity (such as luma and chroma
quantization deltas, loop filter levels, or color configuration parameters)
grouped contiguously together. Do not interleave unrelated syntax elements or
metadata parsing between tightly coupled parameters.

**Why.** Grouping related syntax elements together keeps bitstream serialization
and deserialization consistent, improves specification readability, and
simplifies conditional handling for special configurations like monochrome
video.

## Examples

Bad:

```cpp
// Luma delta quant parsed before color metadata, but chroma delta quant parsed after:
seq_params->base_y_dc_delta_q =
    DELTA_DCQUANT_MIN + aom_rb_read_literal(rb, DELTA_DCQUANT_BITS);
if (is_monochrome) {
  // ... unrelated color metadata parsing ...
  return;
}
seq_params->base_uv_dc_delta_q =
    DELTA_DCQUANT_MIN + aom_rb_read_literal(rb, DELTA_DCQUANT_BITS);
```

Good:

```cpp
// Group related syntax elements contiguously together:
#if CONFIG_EXTQUANT
seq_params->base_y_dc_delta_q =
    DELTA_DCQUANT_MIN + aom_rb_read_literal(rb, DELTA_DCQUANT_BITS);
if (!is_monochrome) {
  seq_params->base_uv_dc_delta_q =
      DELTA_DCQUANT_MIN + aom_rb_read_literal(rb, DELTA_DCQUANT_BITS);
}
#endif  // CONFIG_EXTQUANT
```
