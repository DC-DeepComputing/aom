---
name: libaom/use-recursive-template-reduction-for-power-of-two-blocks
description: When implementing compile-time power-of-two vector or block reductions, use recursive template specialization recursing on N / 2 rather than enumerating explicit specializations
rubric: design
severity: nit
---

**Rule.** When implementing compile-time power-of-two vector or block
reductions, prefer a recursive template pattern (recursing on `N / 2` with a
base-case specialization for `N == 1`) over writing separate explicit template
specializations for each power of 2. Validate the template parameter with
`static_assert` for powers of two (`(N & (N - 1)) == 0`).

**Why.** Enumerating explicit template specializations for each power of two (1,
2, 4, 8, etc.) duplicates pairwise reduction steps, bloats boilerplate, and
requires manual additions for larger vector or block counts. A recursive
template pattern cleanly expresses divide-and-conquer reductions, eliminates
repetitive code, and generalizes across arbitrary power-of-two dimensions.

## Examples

Bad:

```cpp
template <size_t NumBlocks>
struct BlockReduceTraits {};

template <>
struct BlockReduceTraits<1> {
  template <typename D>
  static hn::VFromD<D> ReduceSum(D d, hn::VFromD<D> v) { return v; }
};

template <>
struct BlockReduceTraits<2> {
  template <typename D>
  static hn::VFromD<hn::BlockDFromD<D>> ReduceSum(D d, hn::VFromD<D> v) {
    constexpr hn::BlockDFromD<D> block_tag;
    return hn::Add(hn::LowerHalf(block_tag, v), hn::UpperHalf(block_tag, v));
  }
};

template <>
struct BlockReduceTraits<4> {
  template <typename D>
  static hn::VFromD<hn::BlockDFromD<D>> ReduceSum(D d, hn::VFromD<D> v) {
    constexpr hn::Half<D> half_tag;
    constexpr hn::BlockDFromD<D> block_tag;
    auto vh = hn::Add(hn::LowerHalf(half_tag, v), hn::UpperHalf(half_tag, v));
    return hn::Add(hn::LowerHalf(block_tag, vh), hn::UpperHalf(block_tag, vh));
  }
};
```

Good:

```cpp
template <size_t NumBlocks>
struct BlockReduceTraits;

template <>
struct BlockReduceTraits<1> {
  template <typename D>
  static hn::VFromD<D> ReduceSum(D d, hn::VFromD<D> v) { return v; }
};

template <size_t NumBlocks>
struct BlockReduceTraits {
  static_assert(NumBlocks > 1, "NumBlocks must be > 1");
  static_assert((NumBlocks & (NumBlocks - 1)) == 0, "NumBlocks must be a power of 2");

  template <typename D>
  static hn::VFromD<hn::BlockDFromD<D>> ReduceSum(D d, hn::VFromD<D> v) {
    constexpr hn::Half<D> half_d;
    auto v_half = hn::Add(hn::LowerHalf(half_d, v), hn::UpperHalf(half_d, v));
    return BlockReduceTraits<NumBlocks / 2>::ReduceSum(half_d, v_half);
  }
};
```
