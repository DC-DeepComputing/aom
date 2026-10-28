---
name: libaom/use-local-enums-for-function-scoped-constants
description: 'When defining local constants or stack array dimensions in C, use block-scoped anonymous enums with kPascalCase names instead of #define or const int'
rubric: readability
severity: nit
---

**Rule.** When defining local integer compile-time constants (such as block
dimensions, array capacities, or counts) within C functions or local scopes,
declare them inside a block-scoped anonymous enum rather than using
function-level #define macros or const int variables. Name all enumerators using
a 'k' prefix in PascalCase/CamelCase (e.g., kBlockWidth, kBlockHeight,
kBlockArea) rather than all-caps abbreviations (e.g., BLK_W, BLK_H). Use these
anonymous enum constants when sizing local stack buffers to ensure integer
constant expressions and avoid Variable Length Array (-Wvla) compiler warnings.

**Why.** In C, preprocessor macros have translation-unit scope and leak outside
function bodies into subsequent code, risking naming collisions. Conversely,
const int declarations in C are const-qualified variables rather than true
integer constant expressions, causing compiler warnings for Variable Length
Arrays (-Wvla) when sizing stack arrays. Block-scoped anonymous enums confine
constants to the function while providing true compile-time constant
expressions, and kPascalCase naming aligns with libaom conventions while
avoiding macro collisions.

## Examples

Bad:

```cpp
static void estimate_screen_content(AV1_COMP *cpi) {
  // Bad 1: Function-level #define macros leak into the translation unit
#define BLK_W 16
#define BLK_H 16
#define BLK_AREA (BLK_W * BLK_H)
  uint8_t downconv_blk[BLK_AREA];

  // Bad 2: const int in C is not a compile-time constant, triggering VLA (-Wvla) warning
  const int kWidth = 16;
  const int kHeight = 16;
  uint8_t buffer[kWidth * kHeight];

  // Bad 3: ALL_CAPS abbreviations in anonymous enum
  enum { BLOCK_SIZE = 32 };
}
```

Good:

```cpp
static void estimate_screen_content(AV1_COMP *cpi) {
  // Good: Block-scoped anonymous enum with kPascalCase provides safe compile-time constants
  enum {
    kBlockWidth = 16,
    kBlockHeight = 16,
    kBlockArea = kBlockWidth * kBlockHeight,
    kBlockSize = 32,
  };

  uint8_t downconv_blk[kBlockArea];
  uint8_t buffer[kBlockWidth * kBlockHeight];
}
```
