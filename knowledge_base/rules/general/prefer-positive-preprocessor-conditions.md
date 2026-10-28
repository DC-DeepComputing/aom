---
name: libaom/prefer-positive-preprocessor-conditions
description: 'When writing preprocessor #if/#else blocks for configuration flags or macros, test the positive condition first instead of negating with #if !'
rubric: readability
severity: should_fix
---

**Rule.** When writing preprocessor conditional blocks with both an `#if` and an
`#else` branch for configuration flags or macros (such as `CONFIG_*` or feature
definitions), test the positive condition first (`#if CONFIG_FOO`) rather than
the negated condition (`#if !CONFIG_FOO`). Reserve `#if !...` or `#ifndef` for
standalone guards without an `#else` block.

**Why.** Placing the active or enabled feature logic in the primary `#if` block
and the disabled fallback in the `#else` block makes the code straightforward to
read and avoids confusing negative-logic inversions.

## Examples

Bad:

```cpp
#if !CONFIG_NEW_TX_PARTITION_EXT
  sub_txw = txw >> subtx_shift.rows[i];
  sub_txh = txh >> subtx_shift.cols[i];
#else
  sub_txw = txw >> subtx_shift.cols[i];
  sub_txh = txh >> subtx_shift.rows[i];
#endif
```

Good:

```cpp
#if CONFIG_NEW_TX_PARTITION_EXT
  sub_txw = txw >> subtx_shift.cols[i];
  sub_txh = txh >> subtx_shift.rows[i];
#else
  sub_txw = txw >> subtx_shift.rows[i];
  sub_txh = txh >> subtx_shift.cols[i];
#endif
```
