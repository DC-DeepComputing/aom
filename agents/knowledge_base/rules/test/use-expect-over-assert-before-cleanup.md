---
name: libaom/test/use-expect-over-assert-before-cleanup
description: When writing test assertions before explicit resource cleanup or destruction, use EXPECT_* rather than ASSERT_* so cleanup steps execute on failure
rubric: readability
severity: nit
---

**Rule.** In unit tests where resources (such as codec contexts, buffers, or
file handles) are cleaned up explicitly later in the test function rather than
through RAII, use EXPECT_* instead of ASSERT_* for intermediate checks and
operations. Reserve ASSERT_* for fatal preconditions where following code cannot
safely proceed.

**Why.** In GoogleTest, ASSERT_* macros generate a fatal failure that
immediately returns from the test function, skipping subsequent cleanup logic
like aom_codec_destroy() or manual deallocations. Using EXPECT_* records a test
failure while allowing subsequent cleanup routines to execute, preventing
resource leaks and sanitizer warnings.

## Examples

Bad:

```cpp
// set_svc_params should fail since lag_in_frames > 0.
ASSERT_NE(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
          AOM_CODEC_OK);
ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
```

Good:

```cpp
// set_svc_params should fail since lag_in_frames > 0.
EXPECT_NE(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
          AOM_CODEC_OK);
ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
```
