---
name: libaom/avoid-integer-overflow-in-scaled-sentinel-constants
description: When defining sentinel or placeholder constants in scaled units such as kbps, verify values account for unit conversion factors to prevent integer overflow
rubric: correctness
severity: should_fix
---

**Rule.** When defining sentinel, placeholder, or boundary constants in scaled
units (such as kbps, KiB, or milliseconds), ensure the chosen value accounts for
unit conversion factors (e.g., * 1000 for bps or * 1024 for bytes) and profile
multipliers. Verify that calculations using the constant fit within target
32-bit integer ranges without causing integer overflow.

**Why.** Constants defined in scaled units are frequently converted to base
units through multiplication by scaling factors or profile multipliers. Choosing
sentinel values that are too large causes downstream calculations to exceed
32-bit integer capacity, resulting in integer overflow and undefined behavior.

## Examples

Bad:

```cpp
// Scaled in kbps; (1 << 26) * 1000 overflows 32-bit signed int (~67.1 billion)
#define UNDEFINED_RATE (1 << 26)

int bitrate_bps = rate_kbps * 1000;
```

Good:

```cpp
// Scaled in kbps; (1 << 21) * 1000 fits within 32-bit signed int (~2.1 billion)
#define UNDEFINED_RATE (1 << 21)

int bitrate_bps = rate_kbps * 1000;
```
