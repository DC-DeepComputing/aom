---
name: libaom/use-reference-not-pointer-for-refcounts-and-handles
description: When documenting reference-counted resources, pool indices, or handles in comments and identifiers, describe them as 'references' or 'referencing' rather than 'pointers' or 'pointing'
rubric: readability
severity: nit
---

**Rule.** In comments, documentation, and variable names describing
reference-counted resources, buffer pools, and handle- or index-based tracking,
refer to relationships and reference-holding entities as 'referencing',
'reference', or 'holding a reference to' rather than 'pointing to' or 'pointer'.
Reserve 'pointer' and 'pointing to' strictly for actual C memory pointers
(`T*`), and avoid naming integer index variables or handles with pointer
terminology (e.g. use `idx_ptr` or `fb_idx` instead of `ptr` for index holders).

**Why.** In C codebases like libaom, pooled resources (such as frame buffer
pools) are tracked using integer array indices, slot IDs, or logical handles
alongside actual memory pointers. Using 'pointer' or 'pointing to' to describe
non-pointer indices or logical reference counts causes ambiguity about whether
raw memory addresses or array indices are being manipulated, and obscures
reference-counting semantics.

## Examples

Bad:

```cpp
// Modify 'ptr' to point to the buffer at 'new_idx', and update the ref counts.
// One less pointer to the buffer at 'old_idx', so decrease ref count.
void update_buffer_ref(int *ptr, int new_idx, int old_idx);

// 'ref_count' is the number of variables currently pointing to this buffer.
int ref_count;

// Pointer to the current frame in the buffer pool.
int cur_fb_idx;
```

Good:

```cpp
// Modify 'idx_ptr' to reference the buffer at 'new_idx', and update the ref counts.
// One less reference to the buffer at 'old_idx', so decrease ref count.
void update_buffer_ref(int *idx_ptr, int new_idx, int old_idx);

// 'ref_count' is the number of variables currently referencing this buffer.
int ref_count;

// Index/reference to the current frame in the buffer pool.
int cur_fb_idx;
```
