---
name: libaom/comment-out-unused-parameters-in-cpp
description: In C++ definitions and virtual overrides with unused parameters, comment out parameter names in signatures rather than casting to void in bodies
rubric: readability
severity: should_fix
---

**Rule.** In C++ source files, headers, and unit tests, suppress unused
parameter warnings by commenting out or omitting the parameter name in the
function definition signature (e.g., `Type /*param*/` or `Type /* param */`). Do
not use C-style `(void)param;` casts or unused-parameter macros like
`AOM_UNUSED` inside function bodies.

**Why.** In C++, omitting or commenting out parameter names in the signature is
the idiomatic standard for unused parameters in virtual overrides, callbacks,
mock stubs, and interface implementations. Unlike C, C++ does not require
boilerplate `(void)param;` casts or warning-suppression macros inside function
bodies.

## Examples

Bad:

```cpp
// Variant 1: Virtual override hook
void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
  (void)pkt;
  ASSERT_GT(gop_info_.gop_size, 0);
}

// Variant 2: Test mock function or callback
aom_rc_status_t mock_update_encodeframe_result(
    aom_rc_model_t ratectrl_model,
    const aom_rc_encodeframe_result_t *encode_frame_result) {
  (void)ratectrl_model;
  EXPECT_NE(encode_frame_result, nullptr);
  return AOM_RC_OK;
}
```

Good:

```cpp
// Variant 1: Commented out parameter in virtual override
void FramePktHook(const aom_codec_cx_pkt_t * /*pkt*/) override {
  ASSERT_GT(gop_info_.gop_size, 0);
}

// Variant 2: Commented out parameter in test mock
aom_rc_status_t mock_update_encodeframe_result(
    aom_rc_model_t /*ratectrl_model*/,
    const aom_rc_encodeframe_result_t *encode_frame_result) {
  EXPECT_NE(encode_frame_result, nullptr);
  return AOM_RC_OK;
}
```
