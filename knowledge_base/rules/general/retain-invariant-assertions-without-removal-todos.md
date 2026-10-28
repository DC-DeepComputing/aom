---
name: libaom/retain-invariant-assertions-without-removal-todos
description: When adding invariant assertions (assert), retain them permanently and restrict temporary removal TODO comments strictly to transitional defensive fallback checks
rubric: readability
severity: nit
---

**Rule.** Do not add temporary TODO comments scheduling the removal of invariant
assertions (`assert(...)`), such as after a specific date or milestone; retain
invariant assertions as permanent debug sanity checks. When introducing
transitional defensive fallback checks alongside assertions, restrict scheduled
removal TODO comments strictly to the defensive branch rather than marking the
assertion itself for removal.

**Why.** Assertions document and enforce structural invariants in debug builds
at zero runtime cost in release builds (`NDEBUG`), protecting against
regressions and resource leaks. Placing removal TODOs on invariant assertions
risks premature loss of debug validation, whereas temporary defensive fallback
checks can be scheduled for deletion once stability is verified.

## Examples

Bad:

```cpp
// TODO(username): Remove this assertion after 2018-10-31.
assert(buf_idx < FRAME_BUFFERS);

// TODO(username): Remove the assertion and checking after 2019-03-01.
assert(buf->ref_count > 0);
if (buf->ref_count <= 0) return;
```

Good:

```cpp
// Retain invariant assertion permanently.
assert(buf_idx < FRAME_BUFFERS);

// Document invariant permanently; restrict TODO to the defensive fallback.
assert(buf->ref_count > 0);
// TODO(username): Remove the defensive checking after 2019-03-01.
if (buf->ref_count <= 0) return;
```
