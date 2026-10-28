---
name: libaom/use-precise-qp-terminology
description: When commenting on QP, qindex, or delta_q, explicitly state numerical direction or visual quality impact instead of ambiguous phrases like 'high-quality qindex', 'boost deltaq', or 'QP levels'
rubric: readability
severity: nit
---

**Rule.** In code comments, documentation, and CLI argument descriptions
(`ARG_DEF`), refer to quantization parameters explicitly as `QP`, `QP values`,
`qindex`, or `delta_q` rather than imprecise terms like `QP levels`. When
describing quantization adjustments, offsets (`delta_q`, `delta_qindex`,
`qindex_offset`), or trade-offs, explicitly distinguish numerical index
magnitude from perceptual image quality:
1. Avoid ambiguous phrases such as 'high-quality qindexes', 'low-quality QP', 'boost deltaq', 'chroma boost', or 'lower/higher end'.
2. State the explicit numerical parameter direction (e.g., 'low qindex', 'delta_q decrease') and/or the explicit perceptual quality impact (e.g., 'high visual quality', 'finer quantization', 'improved chroma quality relative to luma', 'lower quality end').

**Why.** Quantization parameters (`QP`, `qindex`, `delta_q`) are continuous
numerical indices inversely related to reconstructed image quality: lower index
values represent finer quantization and higher visual fidelity, whereas higher
values represent coarser quantization and lower quality. Ambiguous phrases like
'high-quality qindex', 'boost deltaq', 'chroma boost', 'QP levels', or 'lower
end' confuse whether numerical magnitude or perceptual quality is increasing or
decreasing, obscuring the intended behavior of rate control and quantization
adjustments.

## Examples

Bad:

```cpp
// 1. Ambiguous qindex quality and chroma offset descriptions
// Constant chroma decrease with gradual ramp-down for very high-quality qindexes.
if (qindex < LOW_QINDEX_THRESH) {
  chroma_delta_q = -clamp((quant_params->base_qindex / 2) - 14, 0, 16);
}

// 2. Ambiguous CLI "QP levels" and trade-off phrasing
.enable_cdef = ARG_DEF(NULL, "enable-cdef", 1, "Enable only for high QP levels"),
// Boost deltaq and factor in chroma subsampling when necessary.
extra_cfg->enable_chroma_deltaq = 1;
// Takes a small SSIMULACRA2 hit on the lower end.
```

Good:

```cpp
// 1. Explicit numerical magnitude and visual quality impact
// Constant chroma delta_q decrease (improved chroma quality) with gradual
// ramp-down for very low qindexes (high visual quality).
if (qindex < LOW_QINDEX_THRESH) {
  chroma_delta_q = -clamp((quant_params->base_qindex / 2) - 14, 0, 16);
}

// 2. Precise parameter terminology and quality scale
.enable_cdef = ARG_DEF(NULL, "enable-cdef", 1, "Enable adaptively on frame qindex"),
// Adjust chroma delta_q (boost chroma quality) when necessary.
extra_cfg->enable_chroma_deltaq = 1;
// Takes a small SSIMULACRA2 hit on the lower quality end.
```
