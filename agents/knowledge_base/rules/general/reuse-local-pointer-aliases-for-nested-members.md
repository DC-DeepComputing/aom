---
name: libaom/reuse-local-pointer-aliases-for-nested-members
description: Define and reuse local const variables or pointer aliases for repeated expressions, sizing calculations, and nested struct members rather than redundantly recomputing or re-dereferencing
rubric: readability
severity: nit
---

**Rule.** When an expression, dimension product (e.g., `kBlockWidth *
kBlockHeight`), or buffer sizing computation (e.g., across an allocation call
like `aom_calloc` and a size-tracking field) is repeated across multiple
calculations or statements within a function, calculate it once into a named
local variable (qualified with `const`). Similarly, when a nested struct,
subsystem info member, sub-object, or getter return value (such as `ppi =
ctx->ppi`, `cpi = ppi->cpi`, `cm = &cpi->common`, `xd = &x->e_mbd`, `mt_info =
&cpi->mt_info`, `cdef_info = &cm->cdef_info`, `oxcf = &cpi->oxcf`, or
`intra_row_mt_sync = &cpi->ppi->intra_row_mt_sync`) is accessed repeatedly,
introduce a local variable or `const` pointer alias (e.g., `const NestedType
*const alias = &parent->member;`). When a computed value, cached struct member,
or local pointer alias is established in scope, consistently access child fields
through that local variable rather than re-evaluating expressions or redundantly
re-dereferencing outer parent structs (such as `ctx->ppi->field`,
`cpi->common.field`, or `&cpi->mt_info.enc_row_mt`). Conversely, do not declare
redundant local pointer aliases for direct struct members (such as `error =
cm->error`) when the parent pointer is already directly in scope.

**Why.** Extracting repeated sub-expressions and aliasing nested struct members
eliminates redundant arithmetic, prevents maintenance hazards or sizing
divergence between duplicate calculations (such as mismatched allocated buffer
sizes versus recorded tracking sizes), reduces visual clutter from long
dereference chains, and eliminates redundant struct indirection. Accessing
fields consistently through existing local aliases keeps variable usage uniform
and readable. Conversely, avoiding unnecessary single-level pointer aliases
prevents cluttering local scopes with redundant variable declarations.

## Examples

Bad:

```cpp
// 1. Repeating computed sizing in allocation and tracking field:
CHECK_MEM_ERROR(cm, cpi->consec_zero_mv,
                aom_calloc((max_mi_rows * max_mi_cols) >> 2,
                           sizeof(*cpi->consec_zero_mv)));
cpi->consec_zero_mv_alloc_size = (max_mi_rows * max_mi_cols) >> 2;

// 2. Chaining repeated nested struct accesses without a local alias:
level = cm->cdef_info.cdef_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
sec_strength =
    cm->cdef_info.cdef_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;

// 3. Redundantly re-dereferencing outer parent structs when local aliases exist:
AV1_PRIMARY *ppi = ctx->ppi;
const AV1EncoderConfig *const oxcf = &cpi->oxcf;
ctx->ppi->use_svc = 1;
cpi->common.current_frame.frame_number = frame_num;
av1_set_quantizer(cm, q_cfg->qm_minlevel, q_cfg->qm_maxlevel,
                  oxcf->mode == ALLINTRA, cpi->oxcf.tune_cfg.tuning);

// 4. Redundant local pointer alias for a direct struct member:
struct aom_internal_error_info *error = cm->error;
if (setjmp(error->jmp)) {
  error->setjmp = 0;
}
```

Good:

```cpp
// 1. Sizing expression computed once into a const local variable:
const int consec_zero_mv_alloc_size = (max_mi_rows * max_mi_cols) >> 2;
CHECK_MEM_ERROR(cm, cpi->consec_zero_mv,
                aom_calloc(consec_zero_mv_alloc_size,
                           sizeof(*cpi->consec_zero_mv)));
cpi->consec_zero_mv_alloc_size = consec_zero_mv_alloc_size;

// 2. Introduce local const pointer alias for nested substructure:
const CdefInfo *const cdef_info = &cm->cdef_info;
level = cdef_info->cdef_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
sec_strength =
    cdef_info->cdef_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;

// 3. Consistently access members through established local aliases:
AV1_PRIMARY *ppi = ctx->ppi;
AV1_COMMON *cm = &cpi->common;
const AV1EncoderConfig *const oxcf = &cpi->oxcf;
ppi->use_svc = 1;
cm->current_frame.frame_number = frame_num;
av1_set_quantizer(cm, q_cfg->qm_minlevel, q_cfg->qm_maxlevel,
                  oxcf->mode == ALLINTRA, oxcf->tune_cfg.tuning);

// 4. Access direct member through existing in-scope pointer:
if (setjmp(cm->error->jmp)) {
  cm->error->setjmp = 0;
}
```
