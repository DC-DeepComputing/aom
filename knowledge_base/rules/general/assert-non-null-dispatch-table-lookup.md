---
name: libaom/assert-non-null-dispatch-table-lookup
description: When indexing function pointer dispatch tables that contain NULL for unsupported configurations, assert that the retrieved pointer is non-null before calling it
rubric: correctness
severity: should_fix
---

**Rule.** When indexing into a function pointer dispatch table that contains
`NULL` entries for unsupported dimensions, formats, or parameter configurations,
assert that the retrieved function pointer is non-null (`assert(fn != NULL)`)
before calling it.

**Why.** Dispatch tables indexed by block dimensions or modes frequently contain
NULL entries for unsupported combinations. Calling an unverified function
pointer without an assertion triggers static analysis warnings about potential
NULL pointer dereferences and makes invalid lookup indices or unsupported
configurations harder to detect during debug testing.

## Examples

Bad:

```cpp
dr_prediction_z3_fn f = dr_prediction_z3_arr[get_msb(bw)][get_msb(bh)];
f(dst, stride, left, upsample_left, dy);
```

Good:

```cpp
dr_prediction_z3_fn f = dr_prediction_z3_arr[get_msb(bw)][get_msb(bh)];
assert(f != NULL);
f(dst, stride, left, upsample_left, dy);
```
