---
name: libaom/avoid-aggregation-names-for-intermediate-temporaries
description: When naming intermediate temporary variables or SIMD registers, avoid aggregation names like sum, acc, or total unless accumulation occurred; use neutral descriptors like temp
rubric: readability
severity: should_fix
---

**Rule.** Do not name intermediate, scratch, or staging variables or SIMD vector
registers with aggregation terms such as 'sum', 'acc', 'accum', or 'total' when
they merely hold unsummed vector operands, shifted, buffered, or reordered
values. Reserve aggregation names for variables that store the result of
arithmetic addition or accumulation operations; otherwise, use neutral, accurate
descriptors such as 'temp', 'temp_0', or descriptive stage identifiers.

**Why.** Naming intermediate staging values or reordered registers 'sum', 'acc',
or 'total' misleads readers by falsely implying arithmetic addition or
accumulation dependencies that do not exist, obscuring the actual data flow and
semantic role of the variable.

## Examples

Bad:

```cpp
// Misleadingly named 'sum' despite only holding an intermediate unsummed vector
uint8x8_t sum;
sum = t0;
t0 = t7;
t1 = vext_u8(sum, t7, 1);
```

Good:

```cpp
// Accurately named temporary variable
uint8x8_t temp_0;
temp_0 = t0;
t0 = t7;
t1 = vext_u8(temp_0, t7, 1);
```
