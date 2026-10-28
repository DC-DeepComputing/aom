---
name: libaom/av1/document-hierarchical-boundary-exclusions
description: When building hierarchical 2D block structures or pyramid hashes where border sub-units are excluded from larger blocks, document boundary invariants in explanatory comments
rubric: readability
severity: nit
---

**Rule.** When implementing hierarchical 2D block algorithms, pyramid
downsampling, or multi-scale block hashing where sub-blocks near image or frame
borders cannot be aggregated into larger parent blocks, document the boundary
exclusion invariant with an explanatory comment. Explicitly clarify why boundary
sub-units computed at finer scales are intentionally omitted from coarser block
generation, and explain loop bounds that stop short of image dimensions.

**Why.** In multi-scale block algorithms, sub-units at image borders fit within
the frame at small block sizes but exceed frame boundaries when combined into
larger blocks. Without explanatory comments, loop bounds that exclude these
border entries appear as indexing bugs, off-by-one errors, or unused
calculations during code review and maintenance.

## Examples

Bad:

```cpp
  for (int y_pos = 0; y_pos < y_end; y_pos++) {
    for (int x_pos = 0; x_pos < x_end; x_pos++) {
      p[0] = src_pic_block_hash[pos];
      p[1] = src_pic_block_hash[pos + src_size];
      p[2] = src_pic_block_hash[pos + src_size * pic_width];
      p[3] = src_pic_block_hash[pos + src_size * pic_width + src_size];
      dst_pic_block_hash[pos] =
          av1_get_crc32c_value(calc, (uint8_t *)p, length);
      pos++;
    }
    pos += block_size - 1;
  }
```

Good:

```cpp
  for (int y_pos = 0; y_pos < y_end; y_pos++) {
    for (int x_pos = 0; x_pos < x_end; x_pos++) {
      // Build up a bigger block from 4 smaller, non-overlapping source block
      // hashes, and compute its hash. Note: source blocks at the right and
      // bottom borders cannot be part of larger blocks, therefore they won't be
      // considered into the block hash value generation process.
      p[0] = src_pic_block_hash[pos];
      p[1] = src_pic_block_hash[pos + src_size];
      p[2] = src_pic_block_hash[pos + src_size * pic_width];
      p[3] = src_pic_block_hash[pos + src_size * pic_width + src_size];
      dst_pic_block_hash[pos] =
          av1_get_crc32c_value(calc, (uint8_t *)p, length);
      pos++;
    }
    pos += block_size - 1;
  }
```
