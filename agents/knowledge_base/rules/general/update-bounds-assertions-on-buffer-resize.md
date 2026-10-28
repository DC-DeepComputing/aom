---
name: libaom/update-bounds-assertions-on-buffer-resize
description: When resizing or refactoring buffer allocations, update all intermediate dimension and bounds assertions to match the new buffer capacity
rubric: correctness
severity: should_fix
---

**Rule.** When refactoring, resizing, or changing the allocation backing of a
buffer (such as moving from a fixed stack array to a shared or heap buffer),
update all corresponding bounds and dimension assertions (e.g.,
assert(intermediate_height < limit)) to match the new allocation capacity and
exact worst-case bounds. Do not retain stale bounds assertions sized for
previous buffer dimensions.

**Why.** Assertions that test intermediate dimensions or indices against
obsolete buffer capacities will either fail to detect out-of-bounds writes when
a buffer is downsized or trigger false assertion failures when layout
assumptions change.

## Examples

Bad:

```cpp
// Buffer storage changed from stack array to heap buffer with new capacity:
uint8_t *temp = comp_pred;
// ...
// Stale assertion still checks against previous stack buffer capacity:
assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
```

Good:

```cpp
// Buffer storage changed from stack array to heap buffer with new capacity:
uint8_t *temp = comp_pred;
// ...
// Assertion updated to match the new buffer allocation capacity:
assert(intermediate_height < (MAX_SB_SIZE + SUBPEL_TAPS));
```
