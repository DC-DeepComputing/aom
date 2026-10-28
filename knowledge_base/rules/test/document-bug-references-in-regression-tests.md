---
name: libaom/test/document-bug-references-in-regression-tests
description: When adding regression tests for bug fixes, reference the associated bug tracker or issue ID in an explanatory comment or test name
rubric: readability
severity: nit
---

**Rule.** When adding a regression test for a bug fix, include a reference to
the associated bug report or issue tracker ID (such as aomedia:<id>, b/<id>, or
crbug.com/<id>). Place the bug reference either in an explanatory comment
preceding the test when using a descriptive test name, or directly in the test
name (such as Buganizer<id> or OssFuzz<id>) when a descriptive name is not
applicable.

**Why.** Linking regression tests to bug tracker entries ensures traceability
back to the original bug report, reproduction steps, or crash details, allowing
future maintainers to understand why the test exists and what specific failure
mode it guards against.

## Examples

Bad:

```cpp
// BUG TRIGGER: size_align=32 causes w,h=32 while d_w,d_h=16
// This mismatch causes buffer overflow in av1_copy_and_extend_frame()
// if the fix is not present.
TEST(EncodeAPI, BufferOverflowOnUnalignedDisplaySize) {
  ...
}
```

Good:

```cpp
// Bug aomedia:480978101: size_align=32 causes w,h=32 while d_w,d_h=16
// This mismatch causes buffer overflow in av1_copy_and_extend_frame()
// if the fix is not present.
TEST(EncodeAPI, BufferOverflowOnUnalignedDisplaySize) {
  ...
}

// Alternatively, name the test directly after the tracking bug ID:
TEST(EncodeAPI, Buganizer480978101) {
  ...
}
```
