---
name: libaom/order-entity-before-attribute-in-dimension-names
description: When naming variables or parameters for buffer, block, or frame dimensions, use entity_attribute ordering (e.g., ref_buf_width) rather than attribute_entity (width_ref_buf)
rubric: readability
severity: nit
---

**Rule.** Follow the standard <entity>_<attribute> naming convention (e.g.,
ref_buf_width, block_height, src_stride) rather than <attribute>_<entity> (e.g.,
width_ref_buf, height_ref_buf) when naming variables or parameters for buffer,
block, or frame dimensions and properties.

**Why.** Placing the entity or buffer descriptor before dimension attributes
maintains consistency across the codebase, makes related entity properties sort
and group logically together, and reads more naturally in code.

## Examples

Bad:

```cpp
const int width_ref_buf = (search_size_width << 1) + bw;
const int height_ref_buf = (search_size_height << 1) + bh;
```

Good:

```cpp
const int ref_buf_width = (search_size_width << 1) + bw;
const int ref_buf_height = (search_size_height << 1) + bh;
```
