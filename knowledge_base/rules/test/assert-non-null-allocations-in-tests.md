---
name: libaom/test/assert-non-null-allocations-in-tests
description: When dynamically allocating images, buffers, or objects in unit tests, assert that pointers are non-null using ASSERT_NE(ptr, nullptr) before dereferencing
rubric: readability
severity: should_fix
---

**Rule.** In unit tests, explicitly assert that dynamically allocated resources
and pointers (such as images from `aom_img_alloc()`, memory buffers from
`aom_malloc()`/`aom_memalign()`, or object handles) are non-null using
`ASSERT_NE(ptr, nullptr)` immediately after allocation. Do not dereference or
pass allocated pointers to test helpers without verifying allocation success.

**Why.** If resource allocation fails, proceeding with execution causes an
unhandled segmentation fault that terminates the test runner without diagnostic
context. Checking allocation with `ASSERT_NE(ptr, nullptr)` fails fast with a
clear GoogleTest error message identifying the failed allocation.

## Examples

Bad:

```cpp
aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
FillImage(raw, (uint8_t)(128 + f));
```

Good:

```cpp
aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
ASSERT_NE(raw, nullptr);
FillImage(raw, (uint8_t)(128 + f));
```
