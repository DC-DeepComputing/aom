---
name: libaom/av1/start-valid-padding-strided-convolution-at-zero
description: When implementing CNN convolution or pooling with PADDING_VALID, start strided loops at index 0 rather than offsetting by (dim - 1) % stride
rubric: correctness
severity: should_fix
---

**Rule.** In CNN convolution, pooling, or layer traversal with `PADDING_VALID`,
start strided spatial loops strictly at index 0 (`h = 0`, `w = 0`). Do not
offset the initial loop index with modulo or phase calculations such as `(dim -
1) % stride`, which apply only to `PADDING_SAME` alignment schemes.

**Why.** VALID padding applies convolution filters strictly to unpadded input
coordinates starting from origin index 0. Offsetting the starting index by `(dim
- 1) % stride` skips valid border pixels at the start of the image and misaligns
the computed tensor output.

## Examples

Bad:

```cpp
for (int h = (in_height - 1) % layer_config->skip_height, u = 0;
     h < in_height - layer_config->filter_height + 1;
     h += layer_config->skip_height, ++u) {
  for (int w = (in_width - 1) % layer_config->skip_width, v = 0;
       w < in_width - layer_config->filter_width + 1;
       w += layer_config->skip_width, ++v) {
    process_valid_patch(input, h, w, u, v);
  }
}
```

Good:

```cpp
for (int h = 0, u = 0;
     h < in_height - layer_config->filter_height + 1;
     h += layer_config->skip_height, ++u) {
  for (int w = 0, v = 0;
       w < in_width - layer_config->filter_width + 1;
       w += layer_config->skip_width, ++v) {
    process_valid_patch(input, h, w, u, v);
  }
}
```
