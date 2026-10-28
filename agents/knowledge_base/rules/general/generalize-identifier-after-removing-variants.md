---
name: libaom/generalize-identifier-after-removing-variants
description: When defining parallel variants, qualify all counterparts symmetrically; when refactoring or pruning variants, remove obsolete distinguishing qualifiers from remaining identifiers
rubric: readability
severity: should_fix
---

**Rule.** When introducing or defining parallel, component-specific,
directional, or plane-specific variants (such as luma and chroma handlers, or
left and above buffers), qualify all counterparts symmetrically (e.g.,
`get_luma_qmlevel` alongside `get_chroma_qmlevel`) rather than leaving one
counterpart with an unqualified generic name (`get_qmlevel`). Conversely, when
refactoring to eliminate directional counterparts (such as above vs. left) or
alternative variants so only a single entity remains, remove now-redundant
qualifiers (such as `_left`, `_above`, or `_primary`) and name the remaining
variable, field, or function generically (e.g., rename `ref_mv_bank_left` to
`ref_mv_bank`).

**Why.** Leaving one counterpart with an unqualified generic name alongside a
specialized variant creates ambiguity about what each identifier operates on.
Conversely, retaining obsolete directional or variant qualifiers when
counterpart structures or modes have been removed misleads readers by implying
that alternative variants still exist, obscuring data structure roles and
relationships.

## Examples

Bad:

```cpp
// Asymmetric naming across parallel plane handlers:
int (*get_qmlevel)(int, int, int);
int (*get_chroma_qmlevel)(int, int, int);

// Retaining obsolete directional qualifier after 'above' bank removed:
typedef struct {
  REF_MV_BANK ref_mv_bank_left;
} MACROBLOCKD;
```

Good:

```cpp
// Symmetric naming across parallel plane handlers:
int (*get_luma_qmlevel)(int, int, int);
int (*get_chroma_qmlevel)(int, int, int);

// Simplified generic name after counterpart removal:
typedef struct {
  REF_MV_BANK ref_mv_bank;
} MACROBLOCKD;
```
