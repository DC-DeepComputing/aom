---
name: libaom/direct-scalar-assignment-for-neon-reductions
description: When storing scalar results from ARM NEON across-vector reduction intrinsics like vaddvq_*, assign directly to scalar pointers instead of using vst1_*
rubric: readability
severity: nit
---

**Rule.** When storing the result of ARM NEON across-vector reduction intrinsics
(such as vaddvq_*, vaddv_*, vmaxvq_*, vminvq_*, or vaddlvq_*), do not pass them
to vector store intrinsics like vst1_*; assign the returned scalar value
directly to the destination pointer or variable.

**Why.** ARM NEON across-vector reduction intrinsics compute a horizontal
reduction across all lanes and return a primitive scalar type rather than a
vector register. Wrapping their return values in vector store intrinsics such as
vst1_* is redundant, causes type mismatches, and can introduce unnecessary
register movements between scalar and SIMD register files.

## Examples

Bad:

```cpp
// Redundantly passing scalar reduction result to a vector store intrinsic
vst1_s64(total_dist, vaddvq_s64(sum));
```

Good:

```cpp
// Directly assigning the scalar reduction result to the destination pointer
*total_dist = vaddvq_s64(sum);
```
