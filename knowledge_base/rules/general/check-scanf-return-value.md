---
name: libaom/check-scanf-return-value
description: When parsing input with fscanf, sscanf, or scanf, verify the return value against expected conversion count before using parsed variables
rubric: correctness
severity: should_fix
---

**Rule.** Always check the return value of `fscanf()`, `sscanf()`, `scanf()`, or
related parsing functions against the expected number of successfully converted
fields before accessing output variables. Handle parse errors or `EOF`
immediately with appropriate error recovery or cleanup rather than proceeding
with uninitialized or partially assigned data.

**Why.** When parsing fails due to unexpected input formatting, I/O errors, or
EOF, `fscanf` and `sscanf` leave destination variables uninitialized or
unchanged. Accessing those variables without validating the return count leads
to reading uninitialized data and corrupted state.

## Examples

Bad:

```cpp
float val;
fscanf(pfile, "%f", &val);
ext_rate_sum += val;
```

Good:

```cpp
float val;
const int fields_converted = fscanf(pfile, "%f", &val);
if (fields_converted != 1) {
  assert(fields_converted == 1);
  fclose(pfile);
  return;
}
ext_rate_sum += val;
```
