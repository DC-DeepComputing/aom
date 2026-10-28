---
name: libaom/standardize-on-modern-rvv-intrinsics
description: 'When writing RISC-V Vector (RVV) SIMD optimizations, target modern RVV v0.12+ intrinsics directly rather than writing incomplete #if __riscv_v_intrinsic version guards without fallbacks'
rubric: correctness
severity: blocking
---

**Rule.** In RISC-V Vector (RVV) SIMD code, target modern RVV v0.12+ intrinsics
directly rather than writing incomplete `#if __riscv_v_intrinsic == 11000` /
`#elif` preprocessor ladders. If conditional support for legacy intrinsic
versions is necessary, ensure all toolchain versions (including undefined or `<
11000`) are handled with a valid `#else` fallback branch.

**Why.** Incomplete preprocessor ladders like `#if __riscv_v_intrinsic == 11000
#elif __riscv_v_intrinsic >= 12000` lack an `#else` fallback and cause
compilation failures on toolchains where `__riscv_v_intrinsic` is undefined or
less than 11000. In libaom, RVV optimizations standardize on the v0.12+
intrinsics specification (with explicit rounding modes like `__RISCV_VXRM_RNU`),
eliminating dead or broken version-branching complexity.

## Examples

Bad:

```cpp
#if __riscv_v_intrinsic == 11000
  return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, vl);
#elif __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, __RISCV_VXRM_RNU, vl);
#endif
```

Good:

```cpp
return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, __RISCV_VXRM_RNU, vl);
```
