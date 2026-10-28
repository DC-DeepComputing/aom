---
name: libaom/test/handle-null-image-flush-in-encoder-test-hooks
description: When overriding encoder test hooks like PreEncodeFrameHook or PostEncodeFrameHook, account for null-image flush iterations where video->img() is nullptr.
rubric: correctness
severity: nit
---

**Rule.** In encoder tests overriding test driver hooks (such as
PreEncodeFrameHook or PostEncodeFrameHook), account for the final flush
iteration where video->img() returns nullptr. In zero-lag configurations (such
as realtime mode where g_lag_in_frames == 0), the flush iteration produces no
output packets, so per-frame flag toggles, state updates, or packet assertions
must handle or bypass the null-image iteration.

**Why.** The test harness encode loop invokes a final flush step by calling
encode with a null input frame (video->img() == nullptr). If test hooks
unconditionally modify state or expect packets on every hook invocation, the
flush iteration can cause incorrect state transitions or test assertion failures
when no packets are emitted.

## Examples

Bad:

```cpp
void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                        ::libaom_test::Encoder *encoder) override {
  DatarateTest::PreEncodeFrameHook(video, encoder);
  frame_flags_ ^= AOM_EFLAG_CALCULATE_PSNR;
}
```

Good:

```cpp
void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                        ::libaom_test::Encoder *encoder) override {
  DatarateTest::PreEncodeFrameHook(video, encoder);
  frame_flags_ ^= AOM_EFLAG_CALCULATE_PSNR;
  if (video->img() == nullptr) {
    frame_flags_ = 0;
  }
}
```
