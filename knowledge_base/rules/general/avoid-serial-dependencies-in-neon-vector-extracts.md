---
name: libaom/avoid-serial-dependencies-in-neon-vector-extracts
description: When generating multiple shifted vectors using ARM Neon vector extract intrinsics (vextq_* / vext_*), extract directly from base vectors rather than chaining intermediate results
rubric: safety
severity: should_fix
---

**Rule.** When generating multiple shifted vectors using ARM Neon vector extract
intrinsics (such as vextq_* or vext_*), extract each shifted vector directly
from the base source vectors using distinct offset indices. Do not sequentially
chain extract calls by passing intermediate shifted results into subsequent
extracts, which introduces serial instruction dependency chains and limits
superscalar pipeline throughput.

**Why.** Deriving shifted vectors directly from common base operands allows
independent vector extract instructions to execute in parallel across
superscalar execution ports. Chaining extracts sequentially forces each
operation to wait for the preceding result to complete, introducing false data
hazards and pipeline stalls.

## Examples

Bad:

```cpp
// Anti-pattern: Chaining extracts creates serial dependency stalls
const uint32x4_t _0111 = vextq_u32(vzero, _1111, 3);
const uint32x4_t _0011 = vextq_u32(vzero, _0111, 3);
const uint32x4_t _0001 = vextq_u32(vzero, _0011, 3);

const uint32x4_t _1110 = vextq_u32(_1111, vzero, 1);
const uint32x4_t _1100 = vextq_u32(_1110, vzero, 1);
const uint32x4_t _1000 = vextq_u32(_1100, vzero, 1);
```

Good:

```cpp
// Extract directly from base vectors with distinct offsets in parallel
const uint32x4_t _0111 = vextq_u32(vzero, _1111, 3);
const uint32x4_t _0011 = vextq_u32(vzero, _1111, 2);
const uint32x4_t _0001 = vextq_u32(vzero, _1111, 1);

const uint32x4_t _1110 = vextq_u32(_1111, vzero, 1);
const uint32x4_t _1100 = vextq_u32(_1111, vzero, 2);
const uint32x4_t _1000 = vextq_u32(_1111, vzero, 3);
```
