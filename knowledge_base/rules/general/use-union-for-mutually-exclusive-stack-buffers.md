---
name: libaom/use-union-for-mutually-exclusive-stack-buffers
description: When allocating large, mutually exclusive local stack buffers (such as 8-bit vs. 16-bit arrays), place them in a union to minimize stack frame size
rubric: safety
severity: should_fix
---

**Rule.** When a function requires large local stack scratch buffers that are
used in mutually exclusive execution paths (such as separate 8-bit and
16-bit/HBD processing branches), wrap them in an anonymous or named union
instead of declaring separate local arrays. This ensures both buffers share the
same stack allocation.

**Why.** Large stack buffers (such as MAX_SB_SQUARE arrays) consume substantial
stack memory (e.g., 16 KB for 8-bit and 32 KB for 16-bit). Declaring separate
local variables for mutually exclusive paths needlessly inflates the stack frame
size, risking stack overflow on resource-constrained platforms or in deeply
nested call trees.

## Examples

Bad:

```cpp
// Declaring separate buffers allocates 48 KB on the stack even though only one path is taken
DECLARE_ALIGNED(16, uint16_t, dclevel16[MAX_SB_SQUARE]);
DECLARE_ALIGNED(16, uint8_t, dclevel8[MAX_SB_SQUARE]);

if (is_high_bitdepth)
  memset(dclevel16, 0, sizeof(dclevel16));
else
  memset(dclevel8, 0, sizeof(dclevel8));
```

Good:

```cpp
// Sharing memory via union limits stack consumption to the maximum single buffer (32 KB)
union {
  DECLARE_ALIGNED(16, uint16_t, dclevel16[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, dclevel8[MAX_SB_SQUARE]);
} dclevel;

if (is_high_bitdepth)
  memset(dclevel.dclevel16, 0, sizeof(dclevel.dclevel16));
else
  memset(dclevel.dclevel8, 0, sizeof(dclevel.dclevel8));
```
