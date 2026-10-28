---
name: libaom/document-state-machine-and-exploration-order
description: When documenting state machines and exploration loops, distinguish states from transitions, state whether ordering is required or arbitrary, and explain special-case transitions
rubric: readability
severity: nit
---

**Rule.** When documenting state machines, multi-step algorithms, or exploration
loops, accurately distinguish between states and state transitions (avoiding
referring to transitions simply as states), and explain whether the execution
order across states/passes is an algorithmic requirement or arbitrary.
Additionally, document the governing invariants or rationale behind initial
setup states and special-case transitions (such as why a transition only
executes on the initial pass before state modification).

**Why.** Imprecise comments that conflate states with transitions or omit the
algorithmic rationale for special-case passes obscure how the state machine
functions. Documenting whether step sequences are mandatory or arbitrary and
clarifying initial preconditions prevents regressions during refactoring and
performance tuning.

## Examples

Bad:

```cpp
// Adjust offsets and step sizes with this state machine
if (x_offset == 0 && y_offset == 0) {
  // State 0 -> State 1: special case
  // This state will only execute when step == block_size
  x_offset = step / 2;
} else if (x_offset == step / 2 && y_offset == 0) {
  // State 1 -> State 2
  x_offset = 0;
  y_offset = step / 2;
}
```

Good:

```cpp
// Adjust offsets and step sizes with this state machine.
// State 0 is needed because no blocks in pic_hash have been explored,
// so exploration requires accounting for zero x_offset and y_offset.
// State 0 must execute first, but the relative order of states 1, 2, and 3
// is arbitrary as long as no two adjacent blocks are explored consecutively.
if (x_offset == 0 && y_offset == 0) {
  // State 0 -> State 1: special case
  // This state transition will only execute when step == block_size
  x_offset = step / 2;
} else if (x_offset == step / 2 && y_offset == 0) {
  // State 1 -> State 2
  x_offset = 0;
  y_offset = step / 2;
}
```
