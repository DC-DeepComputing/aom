---
name: libaom/scale-element-limits-when-checking-aom-vector-byte-size
description: When bounding container size using aom_vector_byte_size before pushing elements, multiply element count limits by sizeof(*element) and use strict less-than comparisons
rubric: correctness
severity: should_fix
---

**Rule.** When checking vector capacity or bounding element additions using
aom_vector_byte_size(), multiply maximum element count thresholds by
sizeof(*element) to match byte units. Additionally, use a strict `<` comparison
rather than `<=` before calling aom_vector_push_back() to avoid pushing when the
container has reached maximum capacity.

**Why.** aom_vector_byte_size() returns the vector's size in bytes rather than
element count, leading to unit mismatches when compared directly against count
limits. Using `<=` instead of `<` causes an off-by-one error where an element is
appended even when the vector is already full.

## Examples

Bad:

```cpp
#define kMaxCandidatesPerHashBucket 1024

// Bug: aom_vector_byte_size returns bytes, not count, and <= allows an extra push.
if (aom_vector_byte_size(p_hash_table->p_lookup_table[hash_value]) <=
    kMaxCandidatesPerHashBucket) {
  aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                       curr_block_hash);
}
```

Good:

```cpp
#define kMaxCandidatesPerHashBucket 256

// Correct: Scale count by sizeof element and use strict < comparison.
if (aom_vector_byte_size(p_hash_table->p_lookup_table[hash_value]) <
    kMaxCandidatesPerHashBucket * sizeof(*curr_block_hash)) {
  aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                       curr_block_hash);
}
```
