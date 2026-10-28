---
name: libaom/round-and-clamp-floating-point-pixel-results
description: When computing floating-point predictions, filter sums, or model outputs for integer pixel buffers, perform arithmetic in float precision and round and clamp results before casting
rubric: correctness
severity: should_fix
---

**Rule.** When processing floating-point prediction, interpolation, filtering,
or model tensor outputs destined for integer pixel buffers, perform all
intermediate calculations (such as downsampling, averaging, or convolution sums)
in floating-point precision before quantizing. When converting the final
floating-point values to integer pixel types (such as uint8_t or uint16_t),
explicitly round to the nearest integer (using round(), rint(), or lrintf()) and
clamp the result to the valid pixel range (using clip_pixel(),
clip_pixel_highbd(), clamp(), or fclamp()) before casting. Do not quantize
intermediate buffers prematurely or directly cast unrounded/unclamped float
sums.

**Why.** Quantizing floating-point data to integer types before completing
intermediate operations (such as downsampling, interpolation, or averaging)
causes premature precision loss and compounding quantization noise. Furthermore,
directly casting floating-point results to integer types truncates towards zero
rather than rounding to the nearest pixel value, causing systematic darkening
bias. Explicit clamping is essential because multi-tap interpolation filters or
neural model outputs can produce values outside the valid pixel range ([0, 255]
or high bit-depth ranges), causing severe underflow or overflow artifacts if
cast without bounds protection.

## Examples

Bad:

```cpp
// Bad Variant 1: Direct cast truncates float sum and fails to clamp filter overshoot/undershoot
sum = 0.0f;
for (int k = 0; k < filter_tap; k++) {
  sum += sample_arr[k] * (kernel[kernel_idx][k] / 128.0f);
}
pred_block[x + y * stride] = (uint8_t)sum;

// Bad Variant 2: Quantizing model output to uint8_t buffer before averaging loses fractional precision
uint8_t buffer[256];
copy_to_output(interpreter, BLOCK_16X16, buffer, 16);
for (int j = 0; j < 8; ++j) {
  for (int i = 0; i < 8; ++i) {
    comp_pred[j * comp_stride + i] =
        (2 + buffer[2 * i + 16 * 2 * j] + buffer[1 + 2 * i + 16 * 2 * j] +
         buffer[2 * i + 16 * 2 * j + 16] +
         buffer[1 + 2 * i + 16 * 2 * j + 16]) / 4;
  }
}
```

Good:

```cpp
// Good Variant 1: Round to nearest integer and clamp to valid 8-bit range [0, 255]
sum = 0.0f;
for (int k = 0; k < filter_tap; k++) {
  sum += sample_arr[k] * (kernel[kernel_idx][k] / 128.0f);
}
pred_block[x + y * stride] = clip_pixel((int)round(sum));

// Good Variant 2: Perform intermediate averaging in float precision, then clamp and cast
const float *output = interpreter->typed_output_tensor<float>(0);
for (int j = 0; j < 8; ++j) {
  for (int i = 0; i < 8; ++i) {
    const float total = output[2 * i + 16 * 2 * j] +
                        output[1 + 2 * i + 16 * 2 * j] +
                        output[2 * i + 16 * 2 * j + 16] +
                        output[1 + 2 * i + 16 * 2 * j + 16];
    comp_pred[j * comp_stride + i] =
        (uint8_t)fclamp(total / 4.0f, 0.0f, 255.0f);
  }
}
```
