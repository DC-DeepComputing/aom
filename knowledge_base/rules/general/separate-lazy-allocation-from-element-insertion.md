---
name: libaom/separate-lazy-allocation-from-element-insertion
description: When adding elements to lazily-allocated containers or hash buckets, handle setup in an initial null check and fall through to shared insertion and bounds validation
rubric: design
severity: should_fix
---

**Rule.** When adding elements to a lazily-allocated container, buffer, or hash
table bucket on first access, handle allocation, error checking, and initial
setup (e.g., `aom_vector_setup`) inside an initial `if (container == NULL)`
block. Allow control flow to fall through to a single unbranched insertion,
capacity check (e.g., `aom_vector_byte_size`), and error handling path after the
`if` block rather than duplicating insertion logic across `if` and `else`
branches.

**Why.** Duplicating element insertion, capacity checks, and error handling
across both `if (container == NULL)` and `else` branches creates redundant code
paths and increases maintenance risk, such as accidentally bypassing size bounds
validation during initial insertion or having the branches diverge when updated.

## Examples

Bad:

```cpp
if (p_hash_table->p_lookup_table[hash_value] == NULL) {
  p_hash_table->p_lookup_table[hash_value] =
      aom_malloc(sizeof(*p_hash_table->p_lookup_table[hash_value]));
  if (!p_hash_table->p_lookup_table[hash_value]) return false;
  if (aom_vector_setup(p_hash_table->p_lookup_table[hash_value], 10,
                       sizeof(block_hash)) == VECTOR_ERROR) {
    return false;
  }
  // Duplicated push_back; bypasses capacity check
  if (aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                           &block_hash) == VECTOR_ERROR) {
    return false;
  }
} else {
  if (aom_vector_byte_size(p_hash_table->p_lookup_table[hash_value]) <
      max_bytes) {
    if (aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                             &block_hash) == VECTOR_ERROR) {
      return false;
    }
  }
}
```

Good:

```cpp
if (p_hash_table->p_lookup_table[hash_value] == NULL) {
  p_hash_table->p_lookup_table[hash_value] =
      aom_malloc(sizeof(*p_hash_table->p_lookup_table[hash_value]));
  if (!p_hash_table->p_lookup_table[hash_value]) return false;
  if (aom_vector_setup(p_hash_table->p_lookup_table[hash_value], 10,
                       sizeof(block_hash)) == VECTOR_ERROR) {
    return false;
  }
}

// Single unbranched insertion and capacity check
if (aom_vector_byte_size(p_hash_table->p_lookup_table[hash_value]) <
    max_bytes) {
  if (aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                           &block_hash) == VECTOR_ERROR) {
    return false;
  }
}
```
