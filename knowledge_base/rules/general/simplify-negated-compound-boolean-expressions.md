---
name: libaom/simplify-negated-compound-boolean-expressions
description: When writing boolean conditionals with outer negations, apply De Morgan's laws to simplify compound expressions into direct terms or chained inequality checks
rubric: readability
severity: nit
---

**Rule.** Simplify negated compound boolean expressions (such as `!(A && B)` or
`!(A || B)`) by applying De Morgan's laws to distribute the negation into direct
sub-conditions (`!A || !B` or `!A && !B`). Specifically, when excluding multiple
discrete values, rewrite negated disjunctions of equality comparisons (such as
`!(x == A || x == B)`) as direct chained inequalities (`x != A && x != B`).
Avoid wrapping multi-term conjunctions or disjunctions in an outer logical NOT
operator when equivalent direct comparisons can be evaluated.

**Why.** Negating compound boolean expressions with an outer `!` forces readers
to mentally invert nested terms and connectives, adding cognitive overhead.
Applying De Morgan's laws eliminates outer negations and redundant grouping
parentheses, yielding flat, direct relational checks that are easier to scan,
reason about, and maintain.

## Examples

Bad:

```cpp
// Bad: Negated compound conjunction
if (!(cpi->sf.mv_sf.hash_max_8x8_intrabc_blocks && bsize > BLOCK_8X8)) {
  bestsme = av1_intrabc_hash_search(cpi, xd, &fullms_params,
                                    intrabc_hash_info, &best_mv.as_fullmv);
}

// Bad: Negated disjunction of equality checks
if (cpi->sf.mv_sf.intrabc_search_level >= 1 &&
    !(bsize == BLOCK_4X4 || bsize == BLOCK_8X8 || bsize == BLOCK_16X16)) {
  av1_rd_pick_intra_sbuv_mode(cpi, x, &rate, &rate_tokenonly, &distortion,
                              &skippable, &bsse, bsize, best_rd);
}
```

Good:

```cpp
// Good: Distributed negation into direct disjunction
if (!cpi->sf.mv_sf.hash_max_8x8_intrabc_blocks || bsize <= BLOCK_8X8) {
  bestsme = av1_intrabc_hash_search(cpi, xd, &fullms_params,
                                    intrabc_hash_info, &best_mv.as_fullmv);
}

// Good: Flattened into direct chained inequality checks
if (cpi->sf.mv_sf.intrabc_search_level >= 1 &&
    bsize != BLOCK_4X4 && bsize != BLOCK_8X8 && bsize != BLOCK_16X16) {
  av1_rd_pick_intra_sbuv_mode(cpi, x, &rate, &rate_tokenonly, &distortion,
                              &skippable, &bsse, bsize, best_rd);
}
```
