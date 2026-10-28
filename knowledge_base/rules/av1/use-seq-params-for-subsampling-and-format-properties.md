---
name: libaom/av1/use-seq-params-for-subsampling-and-format-properties
description: When querying chroma subsampling or stream format properties in AV1, retrieve them from SequenceHeader (cm->seq_params) rather than frame buffer structs
rubric: readability
severity: should_fix
---

**Rule.** In AV1 encoder and common code, retrieve sequence-level format
properties (such as subsampling_x, subsampling_y, or bit_depth) directly and
consistently from the canonical SequenceHeader (seq_params or cm->seq_params).
Do not query or mix secondary frame buffer structs (such as cpi->source or
YV12_BUFFER_CONFIG) for stream-level format properties.

**Why.** The SequenceHeader (seq_params) is the authoritative definition of
sequence and chroma subsampling properties in AV1. Frame buffer subsampling
fields are only populated from sequence parameters upon buffer allocation;
querying buffer structs directly or mixing sources across call sites introduces
lifecycle ordering dependencies, harms readability, and risks defects if buffer
configurations diverge.

## Examples

Bad:

```cpp
// Anti-pattern 1: Querying chroma subsampling from frame buffer struct
int subsampling_x = cpi->source->subsampling_x;
int subsampling_y = cpi->source->subsampling_y;

// Anti-pattern 2: Inconsistently mixing buffer structs with seq_params
av1_set_quantizer(
    cm, cpi->oxcf.q_cfg.qm_minlevel, cpi->oxcf.q_cfg.qm_maxlevel, qindex,
    cpi->oxcf.q_cfg.enable_chroma_deltaq, cpi->oxcf.q_cfg.enable_hdr_deltaq,
    cpi->oxcf.mode == ALLINTRA, cpi->source->subsampling_x,
    cpi->source->subsampling_y);
av1_setup_block_planes(xd, seq_params->subsampling_x,
                       seq_params->subsampling_y, num_planes);
```

Good:

```cpp
// Idiomatic: Querying directly from canonical seq_params
int subsampling_x = cm->seq_params->subsampling_x;
int subsampling_y = cm->seq_params->subsampling_y;

// Idiomatic: Consistent use of seq_params across related calls
av1_set_quantizer(
    cm, cpi->oxcf.q_cfg.qm_minlevel, cpi->oxcf.q_cfg.qm_maxlevel, qindex,
    cpi->oxcf.q_cfg.enable_chroma_deltaq, cpi->oxcf.q_cfg.enable_hdr_deltaq,
    cpi->oxcf.mode == ALLINTRA, seq_params->subsampling_x,
    seq_params->subsampling_y);
av1_setup_block_planes(xd, seq_params->subsampling_x,
                       seq_params->subsampling_y, num_planes);
```
