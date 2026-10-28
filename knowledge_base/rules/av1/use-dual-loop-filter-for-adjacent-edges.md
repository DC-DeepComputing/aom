---
name: libaom/av1/use-dual-loop-filter-for-adjacent-edges
description: When filtering adjacent block boundary edges during loop filtering, use dual loop filter functions (_dual) instead of calling single-edge filter functions sequentially
rubric: safety
severity: should_fix
---

**Rule.** When filtering two adjacent block boundaries (such as adjacent 4-, 6-,
8-, or 14-pixel edges), call the dual loop filter functions (such as
aom_lpf_horizontal_*_dual, aom_lpf_vertical_*_dual, or their
aom_highbd_lpf_*_dual variants) instead of invoking single-edge loop filter
functions twice in sequence. Pass the respective boundary limit parameters for
both edges in a single dual-filter invocation.

**Why.** Dual loop filter routines are SIMD-optimized across target CPU
architectures (such as SSE2, AVX2, and NEON) to process two adjacent block
boundaries simultaneously in wide vector registers. Calling single-edge
functions sequentially doubles function dispatch and register setup overhead,
causing measurable performance regressions in hot deblocking loops.

## Examples

Bad:

```cpp
aom_lpf_horizontal_4(p, dst_stride, params0.mblim, params0.lim, params0.hev_thr);
aom_lpf_horizontal_4(p + MI_SIZE, dst_stride, params1.mblim, params1.lim, params1.hev_thr);
```

Good:

```cpp
aom_lpf_horizontal_4_dual(p, dst_stride, params0.mblim, params0.lim,
                          params0.hev_thr, params1.mblim, params1.lim,
                          params1.hev_thr);
```
