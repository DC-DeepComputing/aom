---
name: libaom/name-filter-helpers-specifically
description: When naming internal helper functions in codec filtering or processing stages, use specific algorithm or stage names rather than generic names like do_filtering
rubric: readability
severity: nit
---

**Rule.** Avoid overly generic helper function names (such as do_filtering,
filter, or process) in codec pipeline stages. Name helper functions after the
specific algorithm or filtering stage they implement (such as
apply_temporal_filter, cdef_filter_block, or apply_loop_restoration) to
distinguish them from other filtering and processing stages across the codec.

**Why.** Codecs contain multiple distinct filtering and processing stages
(including temporal filtering, deblocking, CDEF, super-resolution, and loop
restoration). Generic helper names reduce readability and obscure which specific
algorithm is being executed, even within static internal functions.

## Examples

Bad:

```cpp
// Static helper with overly generic name
static AOM_FORCE_INLINE void do_filtering(const uint16_t *src,
                                          ptrdiff_t src_stride,
                                          uint32_t *accumulator) {
  // ...
}
```

Good:

```cpp
// Static helper named with the specific filtering stage
static AOM_FORCE_INLINE void highbd_apply_temporal_filter(
    const uint16_t *src, ptrdiff_t src_stride, uint32_t *accumulator) {
  // ...
}
```
