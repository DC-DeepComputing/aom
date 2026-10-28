---
name: libaom/assert-or-document-spec-invariants
description: When handling logic constrained by specification requirements, invariants, or spec bugs and workarounds, document non-obvious rules with assert(), spec sections, and tracking bug links
rubric: readability
severity: nit
---

**Rule.** When branching on, parsing, or implementing logic constrained by
format modes or specification requirements, document or verify non-obvious
invariants using an explicit assert() or a clarifying comment citing the
relevant specification section. Additionally, when implementing workarounds for
specification bugs or quirks that require keeping non-intuitive logic, add an
explanatory comment that includes a reference link to the tracking issue (e.g.,
See also: https://crbug.com/...).

**Why.** Implicit specification constraints and spec-bug workarounds are
non-obvious to readers inspecting local branch logic. Explicit assertions,
specification section references, and tracking bug links document valid codec
states, prevent inadvertent 'fixes' of intentional workarounds, and provide full
historical context for future maintainers.

## Examples

Bad:

```cpp
// Example 1: Missing assertion and spec reference for format-specific invariant
if (!obu_header->has_size_field) {
  if (obu_size < obu_header->size) return AOM_CODEC_CORRUPT_FRAME;
  *payload_size = obu_size - obu_header->size;
}

// Example 2: Spec bug workaround comment without tracking issue link
// Note: In the AV1 spec, gm->wmmat[0] (horizontal) and gm->wmmat[1] (vertical)
// assignments are accidentally reversed, so we must keep this logic.
res.as_mv.row = gm->wmmat[0] >> GM_TRANS_ONLY_PREC_DIFF;
res.as_mv.col = gm->wmmat[1] >> GM_TRANS_ONLY_PREC_DIFF;
```

Good:

```cpp
// Example 1: Documented and asserted spec invariant (AV1 Section 5.2)
if (!obu_header->has_size_field) {
  assert(is_annexb);  // Size-less OBUs are only valid in Annex B format.
  if (obu_size < obu_header->size) return AOM_CODEC_CORRUPT_FRAME;
  *payload_size = obu_size - obu_header->size;
}

// Example 2: Spec bug workaround documented with tracking issue link
// Note: In the AV1 spec, gm->wmmat[0] (horizontal) and gm->wmmat[1] (vertical)
// assignments are accidentally reversed, so we must keep this logic.
// See also: https://crbug.com/aomedia/3328
res.as_mv.row = gm->wmmat[0] >> GM_TRANS_ONLY_PREC_DIFF;
res.as_mv.col = gm->wmmat[1] >> GM_TRANS_ONLY_PREC_DIFF;
```
