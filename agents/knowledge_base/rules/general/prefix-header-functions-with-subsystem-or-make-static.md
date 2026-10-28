---
name: libaom/prefix-header-functions-with-subsystem-or-make-static
description: Prefix non-static functions and header symbols with subsystem namespaces (av1_, aom_) and paired lifecycle verbs, but omit prefixes and redundant architecture suffixes from static helpers
rubric: readability
severity: should_fix
---

**Rule.** Prefix all non-static cross-file functions, header declarations,
header-defined inline helpers, extern constants, and global lookup tables with
their appropriate subsystem namespace prefix (e.g., av1_ for av1/ codec
internals, or aom_, aom_dsp_, aom_scale_, aom_mem_ for shared utilities and
public APIs) and architecture suffixes (e.g., _sse2, _ssse3, _sse4_1, _avx2,
_avx512, _neon, _sve, _rvv). Do not define av1/ codec-internal functions or
tables with an aom_ prefix.

Conversely, omit library/subsystem prefixes (such as av1_, aom_, vpx_, or cdef_), redundant architecture suffixes (such as _avx2, _sse2, or _neon), and internal qualifiers (such as _internal) from file-local static or static INLINE helper functions and private tables in .c source files or architecture-specific headers (e.g., convolve_avx2.h) when neighboring sibling helpers omit them. Reserve architecture suffixes for cross-file APIs, RTCD dispatch targets, or helper clusters where suffixes are explicitly used to differentiate coexisting ISA variants.

Exceptions and Lifecycle Conventions:
1. Retain subsystem prefixes on static helpers when directly paired with counterpart public routines (such as alloc/dealloc or init/free lifecycle pairs) to preserve naming symmetry.
2. For resource lifecycle functions, use consistent, symmetrical verb pairs (such as create/terminate, init/free, or alloc/dealloc, e.g., av1_create_workers paired with av1_terminate_workers).

**Why.** In C codebases without native namespaces, all non-static functions,
extern constants, and header declarations share a single global symbol namespace
across translation units and libraries. Subsystem prefixes (such as av1_ or
aom_) and architecture suffixes prevent link-time collisions and clearly
distinguish codec-specific algorithms (av1/) from shared library utilities
(aom_dsp/).

In contrast, file-local static functions and internal static inline helpers have internal linkage restricted to their translation unit or architecture-specific header (such as convolve_avx2.h or transpose_neon.h, where the SIMD architecture context is already established). Adding subsystem prefixes, redundant architecture suffixes, or _internal qualifiers to local helpers introduces unnecessary noise and causes naming inconsistency with neighboring helpers. Symmetrical lifecycle naming makes resource initialization and teardown predictable across callers.

## Examples

Bad:

```cpp
// In av1/common/obu_util.h (unprefixed non-static function in header):
int valid_obu_type(int obu_type);

// In av1/common/common_data.h (codec table wrongly using aom_ prefix):
SELECT_ANY_DATA const BLOCK_SIZE aom_ss_size_lookup[BLOCK_SIZES_ALL][2][2];

// In aom_dsp/x86/convolve_avx2.h (local helper has redundant arch suffix vs neighboring convolve_4tap):
static INLINE __m256i convolve_12tap_avx2(const __m256i *const s,
                                          const __m256i *const coeffs);

// In architecture file (static helper has redundant prefix, internal qualifier, and arch suffix):
static INLINE void aom_prepare_coeffs_12tap_neon(const InterpFilterParams *filter,
                                                int16x8_t *coeffs);
static void lpf_internal_14_neon(uint8x8_t *p6q6, uint8x8_t *p5q5);

// In av1/encoder/ethread.h (asymmetrical lifecycle verb paired with av1_create_workers):
void terminate_worker_data(AV1_PRIMARY *ppi);
```

Good:

```cpp
// In av1/common/obu_util.h (subsystem prefix applied):
int av1_valid_obu_type(int obu_type);

// In av1/common/common_data.h (prefixed with av1_ subsystem):
extern const BLOCK_SIZE av1_ss_size_lookup[BLOCK_SIZES_ALL][2][2];

// In aom_dsp/x86/convolve_avx2.h (local helper omits redundant arch suffix):
static INLINE __m256i convolve_12tap(const __m256i *const s,
                                     const __m256i *const coeffs);

// In architecture file (static helper omits redundant prefix, internal, and arch suffix):
static INLINE void prepare_coeffs_12tap(const InterpFilterParams *filter,
                                       int16x8_t *coeffs);
static void lpf_14_neon(uint8x8_t *p6q6, uint8x8_t *p5q5);

// In av1/encoder/ethread.h (symmetrical lifecycle verb matching av1_create_workers):
void av1_terminate_workers(AV1_PRIMARY *ppi);
```
