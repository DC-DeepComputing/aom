---
name: libaom/consistent-struct-and-class-member-initialization
description: When declaring struct or class fields, do not mix in-class default member initializers with constructors or default-initialization helper functions
rubric: readability
severity: should_fix
---

**Rule.** Maintain consistent member initialization patterns across struct and
class definitions. Do not add in-class default member initializers (e.g., bool
field = false;) to isolated fields when the struct or class initializes its
members in a constructor or a dedicated default-initialization helper function.

**Why.** Mixing in-class default member initializers with constructor or
helper-based initialization is inconsistent, obscures where defaults are
established, and causes syntax errors in C code or dual C/C++ shared header
structs where in-class member initializers are invalid C.

## Examples

Bad:

```cpp
// Mixing in-class default initializer for one field while other fields rely on init helper
struct AV1RateControlRtcConfig {
  int width;
  int height;
  bool is_screen = false;
  int max_quantizer;
};

void AV1RateControlRtcConfigInitDefault(AV1RateControlRtcConfig *config) {
  config->width = 1280;
  config->height = 720;
  config->max_quantizer = 63;
}
```

Good:

```cpp
// Initializing all fields consistently in the default init helper or constructor
struct AV1RateControlRtcConfig {
  int width;
  int height;
  bool is_screen;
  int max_quantizer;
};

void AV1RateControlRtcConfigInitDefault(AV1RateControlRtcConfig *config) {
  config->width = 1280;
  config->height = 720;
  config->is_screen = false;
  config->max_quantizer = 63;
}
```
