---
name: libaom/avoid-stack-array-buffers-for-loop-temporaries
description: When processing intermediate values within loop iterations, use a single local variable instead of allocating stack array buffers indexed by the loop counter
rubric: safety
severity: should_fix
---

**Rule.** Do not allocate temporary stack array buffers when intermediate values
are only produced and consumed within a single loop iteration. Use a single
local variable scoped to the loop instead.

**Why.** Allocating multi-element stack arrays for iteration-local temporaries
increases stack frame size, harms cache locality, and hinders register
allocation. Using a single scalar or vector variable enables direct register
allocation and clarifies that values do not persist across iterations.

## Examples

Bad:

```cpp
int16x8_t temp_output[32];
for (int i = 0; i < height; ++i, j += step) {
  temp_output[i] = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(output)));
  temp_output[i] = vaddq_s16(temp_output[i], in[j]);
  vst1_u8(output, vqmovun_s16(temp_output[i]));
  output += stride;
}
```

Good:

```cpp
for (int i = 0; i < height; ++i, j += step) {
  int16x8_t temp_output = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(output)));
  temp_output = vaddq_s16(temp_output, in[j]);
  vst1_u8(output, vqmovun_s16(temp_output));
  output += stride;
}
```
