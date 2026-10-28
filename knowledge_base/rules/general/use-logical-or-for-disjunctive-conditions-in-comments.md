---
name: libaom/use-logical-or-for-disjunctive-conditions-in-comments
description: When documenting conditional behavior or triggering modes in comments, use logical 'or' instead of colloquial 'and' for disjunctive alternative conditions
rubric: readability
severity: nit
---

**Rule.** When documenting conditional behavior, feature prerequisites, or
triggering modes in comments, use Boolean 'or' rather than 'and' to list
disjunctive alternatives so readers do not misinterpret them as requiring all
conditions simultaneously. Reserve 'and' strictly for conjunctive requirements
where all listed conditions must hold together.

**Why.** In technical documentation and doc comments, listing alternative
triggering conditions with colloquial 'and' creates ambiguity because readers
may interpret it as a Boolean conjunction requiring all conditions
simultaneously. Using precise logical connectives ensures comments accurately
reflect underlying control flow and feature prerequisites.

## Examples

Bad:

```cpp
/*!
 * In all-intra mode and tune IQ and SSIMULACRA2: it also sets the
 * `loop_filter_sharpness` syntax element in the bitstream.
 */
int sharpness;
```

Good:

```cpp
/*!
 * In all-intra mode or tune IQ or SSIMULACRA2: it also sets the
 * `loop_filter_sharpness` syntax element in the bitstream.
 */
int sharpness;
```
