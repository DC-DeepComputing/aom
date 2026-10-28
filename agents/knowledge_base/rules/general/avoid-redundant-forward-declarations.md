---
name: libaom/avoid-redundant-forward-declarations
description: When defining functions or function templates whose definitions precede all call sites, omit redundant forward declarations unless required for mutual recursion
rubric: readability
severity: nit
---

**Rule.** Do not include redundant forward declarations for functions or
templates when all call sites appear after their full definitions and mutual
recursion is not needed. Define functions and templates directly without
preceding prototype declarations.

**Why.** Unnecessary forward declarations add boilerplate, create duplicate
signatures that can drift during refactoring, and obscure file structure without
providing any compiler or architectural benefit.

## Examples

Bad:

```cpp
template <typename D>
HWY_ATTR hn::Vec<hn::BlockDFromD<D>> BlockReduceSum(D int_tag, hn::VFromD<D> v);

template <size_t NumBlocks>
struct BlockReduceTraits { /* ... */ };

template <typename D>
HWY_ATTR hn::Vec<hn::BlockDFromD<D>> BlockReduceSum(D int_tag, hn::VFromD<D> v) {
  return BlockReduceTraits<int_tag.MaxBlocks()>::ReduceSum(int_tag, v);
}
```

Good:

```cpp
template <size_t NumBlocks>
struct BlockReduceTraits { /* ... */ };

template <typename D>
HWY_ATTR hn::Vec<hn::BlockDFromD<D>> BlockReduceSum(D int_tag, hn::VFromD<D> v) {
  return BlockReduceTraits<int_tag.MaxBlocks()>::ReduceSum(int_tag, v);
}
```
