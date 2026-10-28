---
name: libaom/av1/match-bitstream-syntax-order-in-header-structs
description: When declaring or updating bitstream header structs (such as SequenceHeader or ColorConfig), order member fields and section comments to match codec specification syntax sequence and hierarchy
rubric: readability
severity: nit
---

**Rule.** Order member fields and subsections in bitstream syntax structs (such
as SequenceHeader, ColorConfig, frame headers, or OBU parameter structs) to
match the exact sequence of syntax elements defined in the codec specification
(e.g., sequence_header_obu). Do not place fields out of specification order or
group them arbitrarily by type when the specification defines a different
sequence. Additionally, when grouping fields with section comments named after
bitstream syntax structures (such as sequence_header_obu(), color_config(), or
timing_info()), place each field strictly under its corresponding specification
hierarchy comment rather than grouping outer-level syntax fields under inner
subsection comments.

**Why.** Aligning struct member declarations and section comments with codec
bitstream specification syntax order and hierarchy simplifies cross-referencing
code against the standard, avoids confusion during review, and maintains
consistency between struct layouts, bitstream reader/writer functions, and
decoding passes.

## Examples

Bad:

```cpp
// Variant 1: Struct fields out of specification syntax sequence
typedef struct SequenceHeader {
  int color_range;
  aom_chroma_sample_position_t chroma_sample_position;
  int subsampling_x;
  int subsampling_y;
} SequenceHeader;

// Variant 2: Outer-level fields grouped under inner syntax section comment
typedef struct SequenceHeader {
  // Color config.
  BITSTREAM_PROFILE profile;
  aom_bit_depth_t bit_depth;
  aom_color_primaries_t color_primaries;
} SequenceHeader;
```

Good:

```cpp
// Variant 1: Fields ordered to match specification syntax sequence
typedef struct SequenceHeader {
  int color_range;
  int subsampling_x;
  int subsampling_y;
  aom_chroma_sample_position_t chroma_sample_position;
} SequenceHeader;

// Variant 2: Field placed at correct syntax level above subsection comment
typedef struct SequenceHeader {
  BITSTREAM_PROFILE profile;

  // Color config.
  aom_bit_depth_t bit_depth;
  aom_color_primaries_t color_primaries;
} SequenceHeader;
```
