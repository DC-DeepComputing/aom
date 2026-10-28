---
name: libaom/av1/validate-encoded-uleb-size-matches-expected
description: When encoding integers with aom_uleb_encode, verify both successful return and that coded_size equals expected size before advancing buffer offsets
rubric: correctness
severity: should_fix
---

**Rule.** When encoding variable-length integers with aom_uleb_encode(), verify
both that the function succeeds (returns 0) and that the output coded size
matches the expected field length (e.g. coded_size == expected_size) whenever
buffer offsets rely on an exact field length. Alternatively, use
aom_uleb_encode_fixed_size() when encoding a padded, fixed-size LEB128 field.

**Why.** aom_uleb_encode() succeeds as long as the encoded LEB128 value fits
within the available buffer capacity, which can be smaller than the
precalculated or preallocated field size. If downstream pointer arithmetic or
buffer offsets assume an exact field length, an unverified smaller coded size
results in corrupted bitstream layouts or misaligned buffer offsets.

## Examples

Bad:

```cpp
size_t coded_obu_size;
if (aom_uleb_encode(obu_size, length_of_obu_size, buff_ptr,
                    &coded_obu_size) != 0) {
  return AOM_CODEC_ERROR;
}
buff_ptr += length_of_obu_size;
```

Good:

```cpp
size_t coded_obu_size;
if (aom_uleb_encode(obu_size, length_of_obu_size, buff_ptr,
                    &coded_obu_size) != 0 ||
    coded_obu_size != length_of_obu_size) {
  return AOM_CODEC_ERROR;
}
buff_ptr += length_of_obu_size;
```
