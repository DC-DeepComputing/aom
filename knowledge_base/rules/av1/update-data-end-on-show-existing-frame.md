---
name: libaom/av1/update-data-end-on-show-existing-frame
description: When decoding frame headers with show_existing_frame set in AV1 decoder, update output buffer end pointer *p_data_end to the parsed header boundary
rubric: correctness
severity: should_fix
---

**Rule.** When decoding AV1 frame headers where `cm->show_existing_frame` is set
(e.g. in `av1_decode_frame_headers_and_setup`), update the output data end
pointer (`*p_data_end = data + hdr_size`) to point to the parsed header
boundary. Ensure the decoder does not assume tile payloads follow or attempt to
parse trailing payload bytes for header-only frames.

**Why.** Frames with `show_existing_frame` enabled consist entirely of an
uncompressed frame header without tile groups or payload data. Failing to adjust
`*p_data_end` leaves the caller with an invalid buffer boundary, causing
subsequent parsing logic (such as OBU or tile parsing) to read past the frame
header into trailing bytes.

## Examples

Bad:

```cpp
const uint32_t hdr_size =
    av1_decode_frame_headers_and_setup(pbi, rb, trailing_bits_present);
// *p_data_end is not updated for show_existing_frame
return hdr_size;
```

Good:

```cpp
const uint32_t hdr_size =
    av1_decode_frame_headers_and_setup(pbi, rb, trailing_bits_present);
const AV1_COMMON *cm = &pbi->common;
if (cm->show_existing_frame) {
  *p_data_end = data + hdr_size;
}
return hdr_size;
```
