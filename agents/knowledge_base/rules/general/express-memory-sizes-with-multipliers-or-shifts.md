---
name: libaom/express-memory-sizes-with-multipliers-or-shifts
description: When defining memory, buffer, or stack size constants, express values using clear multiplier arithmetic or bit shifts rather than raw hexadecimal literals
rubric: readability
severity: nit
---

**Rule.** When declaring constants or macros for memory, stack, or buffer sizes
in KiB, MiB, or GiB, express the value using explicit binary unit arithmetic
(such as 1024 * 1024 or 256 * 1024) or bit shifts (such as 1 << 20) rather than
raw hexadecimal literals (like 0x100000).

**Why.** Arithmetic expressions like 1024 * 1024 or bit shifts like 1 << 20 make
standard binary size units immediately recognizable and self-documenting,
eliminating ambiguity and avoiding mental conversion errors associated with raw
hexadecimal literals.

## Examples

Bad:

```cpp
// Opaque hexadecimal literals for stack or buffer limits
const size_t kMinStackSize = 0x100000;
#define MAX_BUFFER_SIZE 0x10000000
```

Good:

```cpp
// Clear binary multipliers or bit shifts
const size_t kMinStackSize = 1024 * 1024;  // 1 MiB, or (1 << 20)
#define MAX_BUFFER_SIZE (256 * 1024 * 1024)  // 256 MiB
```
