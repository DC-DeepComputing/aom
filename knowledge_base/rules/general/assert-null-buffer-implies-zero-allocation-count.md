---
name: libaom/assert-null-buffer-implies-zero-allocation-count
description: When asserting invariants for dynamic buffers and tracked allocation counts, use assert(IMPLIES(ptr == NULL, count == 0)) rather than transient reallocation conditions or local flags
rubric: correctness
severity: should_fix
---

**Rule.** When asserting invariants or preconditions for dynamically allocated
buffers and accompanying capacity, dimension, or count tracking variables (such
as allocated_tiles, allocated_rows, or alloc_size), assert the fundamental
structural state invariant using assert(IMPLIES(ptr == NULL, allocated_count ==
0)) (or aom_assert). Do not couple the assertion to transient local variables,
allocation condition flags, or dynamic reallocation conditions (such as
allocated_count != required_count or allocated_count < required_count).

**Why.** An unallocated buffer pointer strictly implies zero capacity or
elements allocated. Asserting transient reallocation conditions or local flags
obscures the true data structure invariant and can pass even if the count holds
an invalid stale non-zero value when unallocated. If a buffer pointer is NULL
while its tracking count remains non-zero, subsequent reallocation checks may
evaluate to false, skipping necessary allocations and causing NULL-pointer
dereferences or memory corruption.

## Examples

Bad:

```cpp
// BAD: Couples assertion to transient reallocation conditions or local flags
const bool alloc_tile_data = cpi->allocated_tiles != tile_cols * tile_rows;
assert(IMPLIES(cpi->tile_data == NULL, alloc_tile_data));
assert(IMPLIES(cpi->tile_data == NULL,
               cpi->allocated_tiles != tile_cols * tile_rows));
assert(IMPLIES(cpi->tile_data == NULL,
               cpi->allocated_tiles < tile_cols * tile_rows));
```

Good:

```cpp
// GOOD: Asserts the fundamental structural invariant that NULL implies zero count
assert(IMPLIES(cpi->tile_data == NULL, cpi->allocated_tiles == 0));
const bool alloc_tile_data = cpi->allocated_tiles != tile_cols * tile_rows;
if (alloc_tile_data) {
  av1_alloc_tile_data(cpi);
}
```
