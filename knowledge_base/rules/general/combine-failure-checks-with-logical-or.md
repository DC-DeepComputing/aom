---
name: libaom/combine-failure-checks-with-logical-or
description: When checking multiple failure conditions or return codes, combine them with logical OR (||) rather than AND (&&) so any failure triggers error handling
rubric: correctness
severity: blocking
---

**Rule.** When checking multiple failure conditions (such as a non-zero function
return code, a NULL pointer, an unexpected output length, or an out-of-bounds
size), combine the checks using logical OR (`||`) rather than logical AND
(`&&`). Ensure that any individual failure condition triggers the error-handling
or early-return path.

**Why.** Using logical AND (`&&`) between distinct failure checks requires all
failure conditions to be true simultaneously before executing the error-handling
branch. This allows single-point failures (such as a helper returning an error
code while the output length check passes) to silently bypass error handling and
propagate corrupted state.

## Examples

Bad:

```cpp
// Using logical AND requires both conditions to fail before reporting an error,
// allowing a single failure (e.g., encode error) to pass undetected.
if (aom_uleb_encode(obu_size, length_of_obu_size, buff_ptr,
                    &coded_obu_size) != 0 &&
    coded_obu_size != length_of_obu_size) {
  return AOM_CODEC_ERROR;
}
```

Good:

```cpp
// Using logical OR ensures that any failure condition triggers the error path.
if (aom_uleb_encode(obu_size, length_of_obu_size, buff_ptr,
                    &coded_obu_size) != 0 ||
    coded_obu_size != length_of_obu_size) {
  return AOM_CODEC_ERROR;
}
```
