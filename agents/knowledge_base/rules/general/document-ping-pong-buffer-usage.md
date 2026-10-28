---
name: libaom/document-ping-pong-buffer-usage
description: When declaring or allocating paired buffers used in an alternating (ping-pong) scheme, add an inline comment explaining their ping-pong role
rubric: readability
severity: nit
---

**Rule.** When declaring local array variables, struct members, or allocating
multi-element buffer sets used in an alternating (ping-pong or double-buffering)
scheme across loop iterations or processing stages, add an inline comment
explaining that the buffers operate in a ping-pong scheme. Clarify how indices
alternate between source and destination or consecutive rows/blocks.

**Why.** An unexplained array of two or three buffers leaves the allocation
intent ambiguous to readers. Documenting the ping-pong or alternating mechanism
clarifies why multiple identical buffers exist and prevents confusion during
maintenance or refactoring of buffer indexing and lifetime.

## Examples

Bad:

```cpp
// Array of two buffer pointers without explanation of their alternating role
uint32_t *block_hash_values[2] = { NULL };
for (int i = 0; i < 2; ++i) {
  block_hash_values[i] = aom_malloc(size * sizeof(*block_hash_values[i]));
}
```

Good:

```cpp
// Document that the two buffers are used for ping-ponging across iterations
uint32_t *block_hash_values[2] = { NULL };  // Two buffers used for ping-pong
for (int i = 0; i < 2; ++i) {
  block_hash_values[i] = aom_malloc(size * sizeof(*block_hash_values[i]));
}
```
