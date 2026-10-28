---
name: libaom/do-not-free-unallocated-or-uninitialized-pointers
description: When handling structs with dynamic pointer members, do not call aom_free on unallocated or uninitialized pointers or add premature cleanup for deferred features
rubric: safety
severity: should_fix
---

**Rule.** Do not invoke aom_free() on struct pointer members that have not been
allocated or were left uninitialized by prior operations. Avoid introducing
premature deallocation calls or cleanup routines for optional fields or features
whose allocation logic is deferred to future changes.

**Why.** In C, stack-allocated structs leave uninitialized pointer members
containing indeterminate addresses. Passing uninitialized or unallocated
pointers to aom_free() triggers undefined memory deallocation, resulting in
memory corruption or crashes.

## Examples

Bad:

```cpp
aom_rc_encodeframe_decision_t encode_frame_decision;
codec_status = av1_extrc_get_encodeframe_decision(
    &cpi->ext_ratectrl, cpi->gf_frame_index, &encode_frame_decision);
if (codec_status != AOM_CODEC_OK) {
  aom_internal_error(cm->error, codec_status,
                     "av1_extrc_get_encodeframe_decision() failed");
}
// Invalid: sb_params_list is unallocated and uninitialized in this change.
aom_free(encode_frame_decision.sb_params_list);
```

Good:

```cpp
aom_rc_encodeframe_decision_t encode_frame_decision;
codec_status = av1_extrc_get_encodeframe_decision(
    &cpi->ext_ratectrl, cpi->gf_frame_index, &encode_frame_decision);
if (codec_status != AOM_CODEC_OK) {
  aom_internal_error(cm->error, codec_status,
                     "av1_extrc_get_encodeframe_decision() failed");
}
// Cleanup is omitted until sb_params_list allocation logic is implemented.
```
