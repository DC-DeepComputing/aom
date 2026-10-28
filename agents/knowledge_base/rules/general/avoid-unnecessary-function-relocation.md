---
name: libaom/avoid-unnecessary-function-relocation
description: When modifying existing functions or code blocks, keep them at their original file positions instead of relocating them, preventing diff churn and history breakage.
rubric: change_scope
severity: nit
---

**Rule.** When modifying existing functions or code blocks, keep them at their
original locations within the source file rather than relocating them. Do not
move functions across a file in the same change that alters their logic; if
reordering functions is necessary, perform the relocation in a separate
refactoring change.

**Why.** Relocating a function while modifying its implementation turns an
in-place diff into a complete deletion and re-insertion, obscuring the
substantive logic changes during code review and cluttering git blame history.

## Examples

Bad:

```cpp
// Relocating read_frame_header_obu from line 273 to line 839 while modifying it:
-static uint32_t read_frame_header_obu(...) { ... }
...
+static uint32_t read_frame_header_obu(...) {
+  const uint32_t hdr_size =
+      av1_decode_frame_headers_and_setup(pbi, rb, trailing_bits_present);
+  const AV1_COMMON *cm = &pbi->common;
+  if (cm->show_existing_frame) {
+    *p_data_end = data + hdr_size;
+  }
+  return hdr_size;
+}
```

Good:

```cpp
// Modifying read_frame_header_obu in place at its original location:
 static uint32_t read_frame_header_obu(...) {
-  return av1_decode_frame_headers_and_setup(pbi, rb, data, p_data_end,
-                                            trailing_bits_present);
+  const uint32_t hdr_size =
+      av1_decode_frame_headers_and_setup(pbi, rb, trailing_bits_present);
+  const AV1_COMMON *cm = &pbi->common;
+  if (cm->show_existing_frame) {
+    *p_data_end = data + hdr_size;
+  }
+  return hdr_size;
 }
```
