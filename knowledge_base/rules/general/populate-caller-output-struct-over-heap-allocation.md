---
name: libaom/populate-caller-output-struct-over-heap-allocation
description: Populate caller-allocated output struct pointers instead of heap allocating with aom_malloc or new, naming in-place initializers 'init' rather than 'create'
rubric: safety
severity: should_fix
---

**Rule.** When helper, utility, or C API functions initialize, compute, or
return structured data or configuration objects, populate caller-allocated
storage passed via pointer (or return small structs by value) rather than
dynamically allocating on the heap with `aom_malloc`, `malloc`, or `new`.
Additionally, name functions that perform in-place initialization with
`*_init_*` or `init` rather than `*_create_*` or `create`, reserving `create`
for functions that dynamically allocate and return new instances.

**Why.** Dynamically allocating structs or configuration objects on the heap
inside helper functions adds unnecessary allocation overhead in
performance-sensitive paths and introduces memory leak hazards when callers must
manage or lack matching deallocators (`aom_free` / `free` / `delete`).
Furthermore, naming in-place initialization helpers with `create` misleadingly
signals heap allocation and ownership transfer rather than in-place mutation of
caller-managed storage.

## Examples

Bad:

```cpp
// Bad: Dynamically allocating structured data with aom_malloc in a helper function
static INLINE const WarpedMotionParams *find_gm_ref_params(AV1_COMMON *const cm,
                                                           int distance,
                                                           int base) {
  WarpedMotionParams *ref_params =
      (WarpedMotionParams *)aom_malloc(sizeof(WarpedMotionParams));
  if (!ref_params) return NULL;
  memcpy(ref_params, &cm->global_motion[LAST_FRAME],
         sizeof(WarpedMotionParams));
  return ref_params;
}

// Bad: Heap-allocating config struct or naming an in-place initializer 'create'
struct AV1RateControlRtcConfig *av1_ratecontrol_rtc_create_ratecontrol_config() {
  return new AV1RateControlRtcConfig();
}

void av1_ratecontrol_rtc_create_ratecontrol_config(
    struct AV1RateControlRtcConfig *config) {
  if (config != NULL) {
    *config = AV1RateControlRtcConfig();
  }
}
```

Good:

```cpp
// Good: Populating caller-provided storage via an output pointer
static INLINE void find_gm_ref_params(WarpedMotionParams *ref_params,
                                      AV1_COMMON *const cm, int distance,
                                      int base) {
  memcpy(ref_params, &cm->global_motion[LAST_FRAME],
         sizeof(WarpedMotionParams));
}

// Good: Populating caller-allocated storage and naming the helper 'init'
void av1_ratecontrol_rtc_init_ratecontrol_config(
    struct AV1RateControlRtcConfig *config) {
  if (config != NULL) {
    *config = AV1RateControlRtcConfig();
  }
}
```
