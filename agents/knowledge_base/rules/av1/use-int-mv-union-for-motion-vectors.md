---
name: libaom/av1/use-int-mv-union-for-motion-vectors
description: When storing, copying, or comparing motion vectors for equality or zero-initialization, use the int_mv union rather than separate row and col variables
rubric: readability
severity: should_fix
---

**Rule.** Use the int_mv union rather than separate row and col variables when
storing, passing, or comparing motion vectors. Use mv.as_int == other.as_int (or
mv.as_int = 0) for single 32-bit equality comparisons, zero-initialization, and
copies instead of operating on row and col members independently.

**Why.** The int_mv union packs 16-bit row and col coordinates into a 32-bit
as_int representation, enabling single 32-bit integer equality tests, zeroing,
and direct assignments. Managing separate row and col variables increases
boilerplate and requires redundant multi-field comparisons.

## Examples

Bad:

```cpp
int sb_me_mv_col;
int sb_me_mv_row;
// ...
if (search_state->frame_mv[mode][ref].as_mv.col == x->sb_me_mv_col &&
    search_state->frame_mv[mode][ref].as_mv.row == x->sb_me_mv_row) {
  // ...
}
```

Good:

```cpp
int_mv sb_me_mv;
// ...
if (search_state->frame_mv[mode][ref].as_int == x->sb_me_mv.as_int) {
  // ...
}
```
