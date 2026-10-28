---
name: libaom/aom_dsp/cast-subtracted-variance-term-to-unsigned
description: When computing variance from sum and *sse, cast the subtracted squared-mean term directly to unsigned int or uint32_t rather than casting the full difference
rubric: readability
severity: should_fix
---

**Rule.** In variance and subpel/OBMC variance functions returning unsigned int
or uint32_t, cast the subtracted squared-mean term ((int64_t)sum * sum) / N (or
>> shift) directly to unsigned int or uint32_t instead of wrapping the entire
*sse - ... difference in a cast.

**Why.** Casting the subtracted squared-mean term directly ensures the
subtraction with *sse is evaluated in unsigned 32-bit arithmetic, avoiding
implicit 64-bit signed promotion, mixed-signedness subtraction, and compiler
conversion warnings.

## Examples

Bad:

```cpp
return (unsigned int)(*sse - (((int64_t)sum * sum) / (W * H)));
```

Good:

```cpp
return *sse - (unsigned int)(((int64_t)sum * sum) / (W * H));
```
