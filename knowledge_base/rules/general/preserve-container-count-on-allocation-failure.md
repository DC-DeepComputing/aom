---
name: libaom/preserve-container-count-on-allocation-failure
description: When allocation fails while inserting or appending to a container, return an error and preserve existing elements instead of resetting item count to zero
rubric: correctness
severity: should_fix
---

**Rule.** Do not reset a container or collection's element count to zero when
memory allocation fails while adding or appending an item. Return an error
indicator (such as -1 or an error status) and preserve existing valid elements
and count intact.

**Why.** Resetting the item count on an insertion failure corrupts container
state, causes previously allocated items to be leaked when destroying the
container, and can return an index that aliases an existing item.

## Examples

Bad:

```cpp
dictionary->strs[dictionary->num_strs] = malloc(len + 1);
if (!dictionary->strs[dictionary->num_strs]) {
  dictionary->num_strs = 0;
  return 0;
}
```

Good:

```cpp
dictionary->strs[dictionary->num_strs] = malloc(len + 1);
if (!dictionary->strs[dictionary->num_strs]) {
  return -1;
}
```
