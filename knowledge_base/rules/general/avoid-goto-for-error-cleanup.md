---
name: libaom/avoid-goto-for-error-cleanup
description: When handling errors or allocation failures in C functions, avoid goto jumps to error labels; release resources directly or extract a dedicated cleanup helper
rubric: readability
severity: should_fix
---

**Rule.** Do not use goto statements jumping to error or cleanup labels for
resource deallocation and failure handling. Free dynamically allocated resources
directly at failure sites or encapsulate repetitive cleanup logic in a dedicated
static helper function called before returning or raising an error.

**Why.** Libaom avoids goto jumps for error handling in favor of structured
control flow in C. Extracting dedicated cleanup helper routines or freeing
resources directly at failure detection points keeps control flow explicit and
avoids jumping across variable scopes or fragile cleanup cascades.

## Examples

Bad:

```cpp
if (!block_hash_values[k][j]) {
  mem_alloc_failed = true;
  goto Error;
}
// ...
Error:
  for (k = 0; k < 2; k++) {
    for (j = 0; j < 2; j++) {
      aom_free(block_hash_values[k][j]);
    }
  }
```

Good:

```cpp
static void free_block_hash_buffers(uint32_t *block_hash_values[2][2],
                                    int8_t *is_block_same[2][3]) {
  for (int k = 0; k < 2; k++) {
    for (int j = 0; j < 2; j++) aom_free(block_hash_values[k][j]);
    for (int j = 0; j < 3; j++) aom_free(is_block_same[k][j]);
  }
}
// ...
if (error || !av1_hash_table_create(&intrabc_hash_info->intrabc_hash_table)) {
  free_block_hash_buffers(block_hash_values, is_block_same);
  aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR, "Memory allocation error");
}
```
