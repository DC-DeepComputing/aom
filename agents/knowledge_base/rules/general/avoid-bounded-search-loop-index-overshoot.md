---
name: libaom/avoid-bounded-search-loop-index-overshoot
description: When searching over bounded parameter ranges or quantizer indices (such as MINQ to MAXQ), ensure loops evaluate the maximum bound without overshooting past MAX
rubric: correctness
severity: blocking
---

**Rule.** When iterating over or searching across bounded parameter ranges,
lookup tables, or quantizer indices (such as MINQ to MAXQ or [0, QINDEX_RANGE -
1]), ensure search loops evaluate the maximum valid bound without overshooting
past MAX.
1. Do not use an exclusive condition (such as qindex < MAXQ) that prematurely terminates before evaluating MAXQ (255) when searching or converting metrics (e.g., via av1_convert_qindex_to_q()).
2. Do not let for (i = MIN; i <= MAX; ++i) search loops terminate naturally past MAX (leaving i == MAX + 1 or MAXQ + 1) if the loop index is returned or used directly as a bounded parameter or table index.
Instead, use a search condition that stops advancing at MAX (e.g., while (qindex < MAXQ && condition) qindex++;), clamp the final returned index with AOMMIN(index, MAX) or clamp(), or handle the out-of-range fallback explicitly.

**Why.** In AV1, MAXQ (255) is a valid quantizer index, while QINDEX_RANGE is
defined as MAXQ - MINQ + 1 (256). Using an exclusive upper bound (qindex < MAXQ)
drops MAXQ from evaluation, causing off-by-one errors when searching or mapping
maximum quantizer values. Conversely, a for (qindex = MINQ; qindex <= MAXQ;
++qindex) loop that breaks on matching leaves qindex equal to MAXQ + 1 (256) if
no element satisfies the condition. Returning or passing this overshot index to
subsequent functions or table lookups causes out-of-bounds array access, invalid
codec parameter states, or memory corruption.

## Examples

Bad:

```cpp
// Anti-pattern 1: Exclusive upper bound skips evaluating MAXQ (255)
for (i = MINQ; i < MAXQ; ++i) {
  if (av1_convert_qindex_to_q(i, bit_depth) >= qstart) break;
}

// Anti-pattern 2: Loop overshoots to MAXQ + 1 (256) when no entry matches
for (qindex = MINQ; qindex <= MAXQ; ++qindex) {
  if (av1_convert_qindex_to_q(qindex, bit_depth) >= q) break;
}
return qindex;  // Out of bounds (256) if q exceeds max table value!
```

Good:

```cpp
// Pattern 1: While loop stops advancing at MAXQ, capping index at MAXQ
int qindex = MINQ;
while (qindex < MAXQ && av1_convert_qindex_to_q(qindex, bit_depth) < q) {
  qindex++;
}
return qindex;

// Pattern 2: Clamp or guard index if using for loop search
for (qindex = MINQ; qindex <= MAXQ; ++qindex) {
  if (av1_convert_qindex_to_q(qindex, bit_depth) >= q) break;
}
return AOMMIN(qindex, MAXQ);
```
