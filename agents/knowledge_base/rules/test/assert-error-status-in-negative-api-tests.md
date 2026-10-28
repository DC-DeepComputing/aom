---
name: libaom/test/assert-error-status-in-negative-api-tests
description: When writing unit tests that verify APIs reject invalid parameters or unsupported configurations, assert that calls return an error status rather than ignoring return values
rubric: testing
severity: nit
---

**Rule.** In unit tests verifying that an API rejects invalid parameters,
incompatible modes, or unsupported configurations, explicitly assert that the
API call returns an expected error status (such as ASSERT_NE/EXPECT_NE against
AOM_CODEC_OK or EXPECT_EQ with AOM_CODEC_INVALID_PARAM). Do not invoke the
function without checking its return value.

**Why.** Invoking an API function without checking its return status in a test
intended to verify error handling or input validation only tests that the call
does not crash. Explicit status assertions ensure the library properly detects
and rejects invalid configurations.

## Examples

Bad:

```cpp
// Testing that invalid SVC parameters are rejected.
aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params);
```

Good:

```cpp
// Explicitly assert that invalid SVC parameters return an error status.
EXPECT_NE(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
          AOM_CODEC_OK);
```
