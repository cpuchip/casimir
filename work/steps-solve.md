# Show steps, 3: solve (abacus decision 71; design #6286, approved #6288)

The third op whose working casim_step pages (work/steps-derive.md and steps-integrate.md are the first two). Solve is
not a tree: it moves the equation to one side, clears it to a primitive integer polynomial, takes out the root 0, finds
its linear and quadratic factors over Q from numeric candidates and proves each by exact division, then solves each
factor. The steps are that route, as a student writes it.

## The steps

Depth 0 is the equation as it stands, `…=0`; depth 1 is each factor set to 0, `F=0: …`. In order:

- **BOTH SIDES** (an equation) or **POLYNOMIAL** (an expression): the equation moved to one side and multiplied out,
  `X^2÷2-X÷3=1` to `X^2÷2-X÷3-1=0`. Left out when it is the text as typed (`X^2-5×X+6=0`, or `X^3-X` for `X^3-X=0`).
  For EVERY X and NO SOLUTION this line, `0=0` or `-1=0`, is the reason, and the answer follows it.
- **INTEGER COEFFICIENTS:** the template multiplies or divides by what clears it, `(X^2÷2-X÷3-1)×6=0` (×k, ÷k, ×(-1) or
  ×(p÷q)); the result is the primitive polynomial with its leading coefficient positive, `3×X^2-2×X-6=0`. Left out
  when the move already gave that.
- **COMMON FACTOR:** the root 0 taken out, `X×(X^2-1)=0`, when something is left beside it.
- **LINEAR FACTOR**, **QUADRATIC FACTOR:** one factor of solve's divided out, to its multiplicity. The template is
  the division, `(X^3-3×X+2)÷(X+2)=X^2-2×X+1` (`÷(X-1)^2` for a factor twice); the result is the product so far
  with what is left, `(X+2)×(X^2-2×X+1)=0`. Left out when what was left is that factor itself (the line before
  already shows it). The factor is found numerically and proven by the division; no candidate test is a step.
- Then each factor = 0, at depth 1:
  - the root 0: `X=0`, with ` (MULT k)` for X^k;
  - **LINEAR:** `2×X-3=0: X=3÷2`, ` (MULT k)` for a factor to the k;
  - **QUADRATIC FORMULA:** the template has the formula's values, unsimplified: `3×X^2-2×X-6=0:
    X=(2-SQRT(76))÷6; X=(2+SQRT(76))÷6` (`X=-SQRT(8)÷2; X=SQRT(8)÷2` when b is 0); with a negative discriminant
    the template is the discriminant itself, `X^2+X+1=0: 1^2-4×1×1=-3<0` (a negative written in parentheses,
    `(-2)^2-4×3×(-6)`). The result has the roots as the answer prints them, `X=(1-SQRT(19))÷3; X=(1+SQRT(19))÷3`,
    or NO REAL ROOTS. The template is left out when it already prints the roots as the result does (d square-free,
    nothing to reduce, once): `X^2-X-1=0: X=(1-SQRT(5))÷2; X=(1+SQRT(5))÷2` is one line;
  - **LEFT:** a factor of degree 3 or more that solve leaves, `X^3-2×X-5=0: DEGREE 3 LEFT`.
- **ANSWER:** solve's own answer, the last step either way.

CASIM_STEPS_ONE_LINE leaves out the template lines (INTEGER COEFFICIENTS', the divisions, the formula's values and
the discriminant).

**The order** (abacus #6288, D1). Factors are listed, in the product lines and at depth 1 alike, in this order:
the root 0's factor X^k first; then the linear factors, ascending by their root; then the quadratic factors,
ascending by (a, b, c) of a X^2 + b X + c, compared in that order; a factor left (LEFT) is last. One factor
repeated is printed once with its multiplicity, `(X-1)^3` in a product, `X=1 (MULT 3)` at depth 1 (the hidden
set's `(MULT 5)` for a quadratic to the fifth: `(X^2-2)^5`, `X=-SQRT(2) (MULT 5); X=SQRT(2) (MULT 5)`).

**Printing** (D2). A polynomial in a step prints as a student writes it: descending powers, each coefficient an
integer or p÷q after its power, `X^2÷2-X÷3-1` (not 002's canonical `-1-X÷3+0.5×X^2`). Every side of every line
reads back through casim's own parser to the value the judge reads; the answer is solve's, unchanged.

## How

- casim_step calls do_solve itself, plain solve unchanged, with its answer in a SLOT at the arena's top (plain
  solve's out, and its bound). A printer then counts and prints the steps from what do_solve left in the arena, at
  the places it grabbed them: the moved coefficients (cleared again, which is pure) and solve_ws's factors; what is
  left is divided out again as the steps go. No records, no hooks in solve.
- Solve's frames on the S3 are rc4's, function by function (measured with -fstack-usage on the S3's compiler). The
  first build called put_solution, and its helpers, from the steps too: GCC cloned and inlined them differently and
  plain solve's frames moved (put_solution 400 to 176 bytes, factors 448 to 304). So do_solve is called whole, and
  div_lin, div_quad, square_free, quad_roots and put_root, each inlined into its one caller before, are always
  inline. casim_step's own frame is rc4's 224 bytes: its call to the steps has five arguments, all in registers
  (the S3 passes six), and GCC may not split them (noipa); a pointer call, the first try, had taken integrate's
  inlined steps out of casim_step and cost them 192 bytes.
- A step line is bounded by the caller's cap (a degree-12 polynomial multiplied out can pass 192 bytes); the answer
  keeps plain solve's bound, so the answer page is plain solve's out byte for byte, and a TOO_LONG answer makes
  every page TOO_LONG, as plain solve is. A step line past cap is that page's TOO_LONG and the count is unchanged
  (derive and integrate do the same: the count never depends on cap). k < 0 counts with any cap.
- Each page runs solve again (the API holds nothing between calls). On the S3 (36 solves paged): the slowest page is
  ×1.23 plain solve's time at the median, ×2.24 at most (V0012, a solve of under a millisecond); V0022 pages 27 steps
  at 144 ms each, and the slowest page of all is 308 ms, (2X-3)^4×(X^2-2)^4, whose plain solve takes 303 ms. The arena: plain solve's and
  512 bytes (the answer's slot, the printer's room), measured on the host over the corpus's 578.
- The bench's per-case stacks: the worst and its budget are unchanged (11,808 + 192 = 12,000 of 12,288). A few
  rows read differently from rc4's bench, both ways (−112 to +272 bytes), on derive's untouched steps too, and a
  different few in each of four builds; the same binary twice reads identically, the rows that move change with the
  bench binary alone (the same casim source under two bench builds: I0010 1,456 and 1,184, I0030 1,792 and 2,016),
  and a delay before the page moves none of six of them across 27 phases. Casim's frames are rc4's, function by function: the readings follow the bench
  binary, not casim. What in the binary moves them is not settled (+272 is more than one 192-byte tick frame).

## The oracle (abacus #6288)

1. Steps off is byte-identical: the golden corpus, solve's frames on the S3, the bench's per-case stacks (derive's
   and integrate's steps included; if casim_step's frame moves, the fix is a thin dispatcher, not a waiver).
2. Every step is right by value (scripts/ssteps-values.py, make check):
   - each depth-0 line's left side is the typed L-R times one constant, the same at every point (not merely nonzero
     at each);
   - a template equals its result (INTEGER COEFFICIENTS'), a division is exact at every point, a discriminant line's
     arithmetic is b^2-4ac of its factor and negative;
   - every root on any line makes its factor and the typed L-R zero, and a template's roots are its result's;
   - the depth-1 factors, each to its multiplicity, multiply back to the last product line; each factor line adds
     one factor; the order above holds;
   - each side of every line, read back by casim (subst at a point), has the judge's value.
3. The steps compose: the last step is plain solve's answer byte for byte; k runs 0..count−1; a refusal is plain
   solve's status.
4. The same page every time.
5. The stack: the bench's worst + one interrupt frame leaves at least 256 of 12,288 bytes.
6. Steps that don't fit fail softly: NO_ROOM, plain solve still answers; one case is shown.
7. The page cost on the S3, measured.
