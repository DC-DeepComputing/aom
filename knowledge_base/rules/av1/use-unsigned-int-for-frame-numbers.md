---
name: libaom/av1/use-unsigned-int-for-frame-numbers
description: When declaring struct fields or variables storing frame numbers or frame counts, use unsigned int instead of uint64_t to match CurrentFrame.frame_number
rubric: readability
severity: nit
---

**Rule.** In AV1 codec structures and algorithms, declare struct fields and
variables storing frame numbers, display/encode indices, or frame counts as
unsigned int. Do not use uint64_t for frame numbers, as standard libaom frame
tracking (such as CurrentFrame.frame_number in av1_common_int.h and
RATE_CONTROL.frame_number_encoded in ratectrl.h) uses unsigned int.

**Why.** Frame numbers and counts fit easily within 32-bit unsigned integers
(supporting over 4 billion frames). Using uint64_t is unnecessarily oversized,
wastes struct memory, and creates type mismatches with standard libaom frame
tracking structures.

## Examples

Bad:

```cpp
// In av1/encoder/ratectrl.h or frame tracking structs
struct RATE_CONTROL {
  uint64_t last_frame_low_source_sad;
  uint64_t frame_number_encoded;
};
```

Good:

```cpp
// In av1/encoder/ratectrl.h or frame tracking structs
struct RATE_CONTROL {
  unsigned int last_frame_low_source_sad;
  unsigned int frame_number_encoded;
};
```
