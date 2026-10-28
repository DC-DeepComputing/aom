---
name: libaom/common/y4m-extension-tags-at-end-of-header
description: When formatting YUV4MPEG2 (Y4M) file headers, place vendor extension fields ('X' tags) at the end of the header after standard stream parameters
rubric: correctness
severity: should_fix
---

**Rule.** When formatting YUV4MPEG2 (Y4M) file headers, place vendor extension
fields (tags starting with 'X', such as 'XCOLORRANGE=...') at the end of the
header string after all standard stream parameters (width, height, framerate,
interlacing, and colorspace).

**Why.** The YUV4MPEG2 format convention and reference implementations (such as
FFmpeg and mjpegtools) expect standard header parameters before vendor extension
tags prefixed with 'X'. Placing extension tags at the end of the header ensures
maximum compatibility with third-party demuxers and parsers.

## Examples

Bad:

```cpp
// Placing vendor extension tag before standard color space parameter
snprintf(buf, len, "YUV4MPEG2 W%d H%d F%d:%d I%c%s %s\n", width,
         height, framerate->numerator, framerate->denominator, 'p',
         color_range, color);
```

Good:

```cpp
// Placing standard color space parameter before vendor extension tag
snprintf(buf, len, "YUV4MPEG2 W%d H%d F%d:%d Ip %s%s\n", width, height,
         framerate->numerator, framerate->denominator, color,
         color_range);
```
