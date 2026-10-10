# Show steps, 1: derive (abacus decision 71; design #6155, approved #6157)

Michael's card (casimir-v1, 2026-10-09): "I'd like show steps to go forwards and backwards in display, so a student
could walk it and see. Animation would be awesome too." Casimir supplies the steps; the firmware pages and animates
them. This unit does derive; integrate follows with the same recorder, and solve is a unit of its own (a stage
recorder, not a tree).

## The design

- **A step** is a derive node's (before, after, depth), recorded as deriv() returns, in one 8-byte arena unit
  linked backwards. Leaves (a number, a letter, π) make none. The rule is not stored: it is read from before's shape
  when the step is printed. deriv() stays inline (always_inline), so the recursion keeps its frame: deriv_ is 64
  bytes a level on the S3, and the recorder note() is a leaf called as a node returns.
- **The walk** is textbook order, top-down and left to right. Post-order with depths fixes the tree, so the printer
  rebuilds the order without the recursion keeping any state. Each node gives up to two lines:
  - the **template**, the rule applied with its parts still to do: `d/dX(X^2×SIN(X)) = d/dX(X^2)×SIN(X)+X^2×d/dX(SIN(X))`;
  - the node's parts, each walked the same way;
  - the **result**: `d/dX(X^2×SIN(X)) = 2×X×SIN(X)+X^2×COS(X)`.

  A node whose parts are all leaves has no template (`d/dX(SIN(X)) = COS(X)` is one line). A subtree without X is
  one step, CONSTANT: `d/dX(…) = 0`, and nothing below it. Two lines is the default; the flag CASIM_STEPS_ONE_LINE
  gives the result lines only, in the same order: the answer is the last step either way (abacus #6252, one rule
  for both ops and both modes; until then one line put the answer first).
- **The API** is stateless: `casim_step(rq, k, flags, &info, out, cap, arena, arena_cap)` runs the op with steps on
  and prints step k. info carries the count, the depth, the line (template or result) and a rule id;
  `casim_rule_name(id)` gives an uppercase English name a caller may override. out holds only
  `d/dX(before) = after`. Paging is k ± 1; the arena needs no lifetime between calls.
- **Rules:** SUM, CONSTANT, CONSTANT MULTIPLE, PRODUCT, POWER (u^n), EXPONENTIAL (a^u), GENERAL POWER (u^v), CHAIN
  (f(u), u not X), one per function of X (SIN, LN, …), and BOTH SIDES (an equation).

## The oracle (abacus #6157)

1. **Steps off is byte-identical.** The golden corpus does not move; every arena and stack figure is unchanged with
   steps off.
2. **Every step is right by value.** A result line's after is d/dX(before) at random points; a template line equals
   its result by value. abacus's hidden judge uses mpmath from a fresh seed; the public one (scripts/step-values.py)
   is in make check.
3. **The steps compose.** The root's result line is byte-identical to plain derive's output for the same request,
   and k runs 0..count−1 with no gap. (casim: the root's result line is the last step, with one line a rule or two,
   since the walk states the problem first and ends on the answer; #6252 made one line end there too.)
4. **The same page every time.** Page k twice gives identical bytes.
5. **The stack is pinned.** deriv stays inline, the S3 bench shows the worst stack unchanged at 11,808 bytes, and a
   mutant that un-inlines it is caught. (casim: caught in make check by scripts/inline-check.sh, a gate on the
   compiler's stack-usage report with a planted noinline control, so CI catches it without the chip.)
6. **Steps that don't fit fail softly.** Steps on and out of arena: casim_step is NO_ROOM, plain derive still works.
   One case is shown.
7. **Page cost on the S3.** The worst case per page for derive is measured on the bench.
