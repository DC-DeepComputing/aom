---
name: libaom/document-struct-fields-in-headers
description: When declaring or modifying structs, typedefs, member fields, scratch buffers, or sync state in headers, document algorithmic purpose, pipeline stages, validity constraints, ascending value ranges, and buffer layout units
rubric: readability
severity: should_fix
---

**Rule.** When declaring or modifying structs, typedefs, member fields, scratch
buffers, or synchronization state in header files, document purpose, lifecycle,
and constraints using Doxygen (`/*! ... */`, `/**< ... */`, `//! ...`) or inline
comments:

- **Struct and typedef lifecycle**: Specify the exact algorithmic purpose, pipeline stage (e.g., preprocessing, loop restoration, transform search), and function scope (e.g., intermediate buffers used within `av1_pick_filter_restoration()`) rather than vague subsystem labels. Clarify whether intermediate data on long-lived context structs (like `AV1_COMP`) is ephemeral scratch state or persistent across frames.
- **Integer flags and modes**: Document valid value ranges and boolean semantics (0 vs. 1). When documenting multi-valued options or extending binary flags, list all supported values in natural ascending numerical order (0, 1, 2) including 0 and default behavior. Describe concrete behaviors rather than binary negations (e.g., avoid "NOT use X") or stale "If true" phrasing. Do not duplicate enum definitions for typed enum members.
- **Validity and conditional usage**: When structs, members, flags, or synchronization primitives (e.g., `AV1EncRowMultiThreadSync`) are active only under specific conditions (such as particular reference frames like `LAST_FRAME`, `deltaq-mode=3`, operational modes, or pipeline passes), explicitly document those scope and validity conditions.
- **Pointers, buffers, and arrays**: For generic pointers, arrays, and scratch buffers (e.g., `buf`, `data`, `ptr`, or `*_alloc_size`), document stored data, memory layout (1D flattened array vs. strided grid vs. ping-pong buffers), indexing semantics, and allocated capacity with explicit measurement units (e.g., 'N elements long' or 'N bytes long' rather than ambiguous 'N long').
- **Semantic updates**: When modifying member semantics without renaming the field, update documentation comments to reflect current usage and add an attributed `TODO` (e.g., `TODO(aomedia:XXXX): ...`).

**Why.** C/C++ struct definitions, raw pointers, and integer fields convey
neither memory layouts, element/byte units, nor operational lifecycle
constraints. Generic subsystem descriptions mislead readers into assuming
scratch state is persistent across the pipeline, leading to invalid cross-stage
data reuse or memory bugs. Explicitly documenting algorithmic purpose, pipeline
stages, function lifecycles, ascending value ranges, buffer layouts, explicit
units, and validity conditions prevents stale reads of unpopulated fields,
out-of-bounds indexing, and configuration errors across codec passes.

## Examples

Bad:

```cpp
// 1. Generic struct/sync comments missing pipeline stage and function lifecycle:
/*!\brief Encoder parameters for synchronization of row-based multi-threading. */
AV1EncRowMultiThreadSync intra_row_mt_sync;

/*!\brief Buffers used during loop restoration search. */
typedef struct {
  RestUnitSearchInfo *rusi[MAX_MB_PLANE];
  int16_t *dgd_avg;
} AV1LrPickStruct;

// 2. Integer flag out of numerical order with binary negation:
/*!\brief Whether encoder should use fixed QP offsets.
 *
 * If a value of 1 is provided, encoder will use fixed QP offsets.
 * If a value of 2 is provided, encoder will use the same QP for all frames.
 * If a value of 0 is provided, encoder will NOT use fixed QP offsets.
 */
int use_fixed_qp_offsets;

// 3. Buffer pointers and arrays missing layout, validity, and capacity units:
typedef struct {
  double *buf;  // Generic pointer without layout (1D vs strided)
  int high_source_sad_lag[MAX_GF_INTERVAL];  // Missing mode validity
  uint32_t *hash_value_buffer[2];  // Ambiguous capacity: "AOM_BUFFER_SIZE long"
  int src_sad_blk_alloc_size;      // Ambiguous units (bytes vs uint64_t elements)
} SaliencyAndRefData;
```

Good:

```cpp
// 1. Explicit algorithmic stage, condition, and function scope:
/*!\brief Synchronization state for row-based multi-threading in the preprocess
 * stage (MB Wiener variance calculation) when deltaq-mode=3. */
AV1EncRowMultiThreadSync intra_row_mt_sync;

/*!\brief Intermediate scratch buffers allocated and used essentially as local
 * variables within av1_pick_filter_restoration(). */
typedef struct {
  RestUnitSearchInfo *rusi[MAX_MB_PLANE];
  int16_t *dgd_avg;
} AV1LrPickStruct;

// 2. Ascending numerical order (0, 1, 2) with concrete behaviors and default:
/*!\brief Controls how the encoder applies fixed QP offsets.
 *
 * 0 = Adaptive QP offsets chosen by rate control (default).
 * 1 = Fixed QP offsets from user array or cq_level.
 * 2 = No QP offsets applied (same QP for all frames).
 */
int use_fixed_qp_offsets;

// 3. Documented layout, ping-pong usage, mode validity, and explicit units:
typedef struct {
  /*! Stores values of the map in a 1D flattened array. */
  double *buf;
  /*! Flag for each frame [i] in lookahead buffer under is_one_pass_rt_lag mode. */
  int high_source_sad_lag[MAX_GF_INTERVAL];
  /*! Two ping-pong buffers, AOM_BUFFER_SIZE_FOR_BLOCK_HASH elements long. */
  uint32_t *hash_value_buffer[2];
  /*! Allocated size to store 64x64 SAD, in units of uint64_t elements. */
  int src_sad_blk_alloc_size;
} SaliencyAndRefData;
```
