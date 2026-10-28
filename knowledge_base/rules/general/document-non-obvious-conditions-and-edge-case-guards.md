---
name: libaom/document-non-obvious-conditions-and-edge-case-guards
description: When writing non-obvious conditional checks, edge-case guards, boundary exclusions (such as MINQ + 1), sentinels, or bypass branches, document their rationale and invariants in comments
rubric: readability
severity: nit
---

**Rule.** When introducing or modifying non-obvious conditional checks,
edge-case guards, threshold delays, sentinel value exclusions (such as checking
`> 0` to filter uninitialized state), literal sentinel arguments (such as `-1`
for threshold boundaries), offset limits or excluded extreme boundaries in
clamping or bounding (such as `MINQ + 1` or `MAX_OFFSET - 1` rather than
standard constants), or special-case bypass branches (such as fast paths
omitting standard processing or cleanup), add an inline or preceding comment
explaining the underlying rationale and invariants. Document why specific
boundary, offset, or sentinel values exist and what domain rationale,
upstream/downstream state, dropped frame conditions, or mode constraints
necessitate them, rather than relying solely on commit descriptions or bug
trackers.

**Why.** Commit descriptions and issue trackers are detached from source code
during routine maintenance and refactoring. Non-standard boundary offsets in
clamping, bounding, or threshold conditions look like accidental off-by-one bugs
to reviewers and maintainers, while unexplained sentinel checks and fast-path
bypasses risk being misinterpreted, misoptimized, or inadvertently removed
during future maintenance.

## Examples

Bad:

```cpp
// Example 1: Non-obvious threshold delay / bypass condition without explanation
if (!frame_is_intra_only(cm) && rc->frames_since_key > 1 &&
    (!cpi->ppi->use_svc ||
     svc->current_superframe > (unsigned int)svc->number_temporal_layers) &&
    !change_target_bits_mb && !cpi->rc.rtc_external_ratectrl) {
  p_rc->baseline_gf_interval = (MIN_GF_INTERVAL + MAX_GF_INTERVAL) / 2;
}

// Example 2: Boundary offset / intentional exclusion without rationale
int sb_qindex = AOMMAX(base_qindex - boost, MINQ + 1);
int chroma_offset = clamp(offset, MIN_OFFSET + 1, MAX_OFFSET);

// Example 3: Sentinel check and sentinel argument without explanation
if (rc->rc_1_frame * rc->rc_2_frame == -1 && rc->q_1_frame > 0) {
  rc->q_1_frame = (rc->q_1_frame + rc->q_2_frame) / 2;
}
if (check_buffer_below_thresh(cpi, buffer_level, -1)) {
  rc->drop_count_consec++;
}

// Example 4: Special-case branch bypassing general cleanup without rationale
if (partition_search_type == VAR_BASED_PARTITION && !keep_best && !keep_none) {
  FREE_PMC_NODE(pc_tree->none);
}
```

Good:

```cpp
// Example 1: Documented threshold delay and state bypass condition
// Delay clamping for SVC until after num_temporal_layers so layers are set.
if (!frame_is_intra_only(cm) && rc->frames_since_key > 1 &&
    (!cpi->ppi->use_svc ||
     svc->current_superframe > (unsigned int)svc->number_temporal_layers) &&
    !change_target_bits_mb && !cpi->rc.rtc_external_ratectrl) {
  p_rc->baseline_gf_interval = (MIN_GF_INTERVAL + MAX_GF_INTERVAL) / 2;
}

// Example 2: Documented boundary offset explaining why MINQ is excluded
// Variance Boost operates in lossy domain; MINQ (lossless mode) is excluded.
int sb_qindex = AOMMAX(base_qindex - boost, MINQ + 1);
// MIN_OFFSET represents an invalid neutral state and is intentionally excluded.
int chroma_offset = clamp(offset, MIN_OFFSET + 1, MAX_OFFSET);

// Example 3: Documented sentinel check and argument meaning
// Check rc->q_1_frame > 0 in case it was uninitialized due to dropped frames.
if (rc->rc_1_frame * rc->rc_2_frame == -1 && rc->q_1_frame > 0) {
  rc->q_1_frame = (rc->q_1_frame + rc->q_2_frame) / 2;
}
// Pass -1 for drop_mark to test if buffer level drops below 0 (<= -1).
if (check_buffer_below_thresh(cpi, buffer_level, -1)) {
  rc->drop_count_consec++;
}

// Example 4: Documented invariant for bypassing extended partition cleanup
// Avoid freeing extended partition nodes because they are not allocated
// when partition_search_type is VAR_BASED_PARTITION.
if (partition_search_type == VAR_BASED_PARTITION && !keep_best && !keep_none) {
  FREE_PMC_NODE(pc_tree->none);
}
```
