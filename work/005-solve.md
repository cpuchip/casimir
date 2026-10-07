# CAS work unit 005: exact polynomial solving

**From:** abacus · **To:** casim · **Status:** ready. The order is the plan's (exact polynomial solving, then simple
integrals), as casim proposed (#4838). It follows CAS 004, accepted at the firmware's 6dc0f18 (abacus
docs/evidence/2026-10-07-cas-004-review.md).

## Why

A student solves x² − 5x + 6 = 0 by factoring. The calculator's SOLVE finds one root numerically, near a guess.
Casimir can give every root exactly, and say when it can't: 2 and 3; (5 ± √13)/2; ½. This unit is the host
library and CLI. The device's key is a later firmware unit, as CAS 004 was.

## Rules

1. **The API:**
   - `casim_solve(eqn, var, model, angle, out, cap, arena, arena_cap)`, also reachable through casim_run as
     CASIM_SOLVE;
   - the CLI: `casim [--model …] [--cap N] solve "<t>" X RAD`.
   - `L=R` is solved as L − R = 0. An expression with no `=` is solved as expr = 0.
2. **What counts as solvable:** after canonicalizing (002), the equation must be a polynomial in var with numeric
   coefficients.
   - Decimals become exact rationals (0.25 is 1/4). Every coefficient is cleared to integers, exactly.
   - Otherwise there are two new statuses:
     - NOT_POLYNOMIAL: var inside a function, under a root, in a denominator, or a negative or fractional power;
     - NOT_NUMERIC: another letter in a coefficient.
   - Both are refusals that change nothing.
3. **Exact roots (casim's proposal):**
   - every rational root (by the rational root test on the integer-cleared coefficients, or any method that is
     verified exactly);
   - degree 1 and 2 in closed form, with irrational quadratic roots as p ± q√r: r a square-free integer above 1,
     and p, q rational;
   - higher degrees reduced by dividing out each rational root found, so a cubic with one rational root leaves a
     quadratic, which is solved in closed form.
   - **Out of scope:** complex roots, and the cubic and quartic formulas.
4. **Every root is verified exactly** before it is printed: the polynomial at that root is 0 in exact rational
   arithmetic (for p ± q√r, with r exact too). A candidate that fails verification is never printed.
5. **The output** (propose its exact text; this is a starting shape):
   - one root per item, `X=2`, separated by a fixed separator, in ascending order of value;
   - each distinct root once, its multiplicity shown when above 1;
   - rationals as `a÷b` in lowest terms, or an integer;
   - p ± q√r in 002's canonical form, both signs given as two roots.
   - **Completeness must be visible:**
     - **SOLVED:** every real root was found;
     - **PARTIAL:** an irreducible factor of degree 3 or more is left, and its degree is reported;
     - **NO REAL ROOTS:** for example a quadratic with a negative discriminant, which is out of scope as complex.
   - A student must never read a partial list as the whole answer.
6. **The bounds:**
   - Over 191 bytes is TOO_LONG.
   - The degree (propose a limit, for example 12) and the coefficients' size: over a stated bound after clearing
     is NO_ROOM, never a slow search.
   - The worst case on the S3 is under 1 s at those bounds (casim's bench), or the unit says what it costs.
   - The flash cost is measured and reported, within the 64 KB in all.
7. **The model and angle** are as CAS 004: casim_run's model reads the equation's stamp. A power chain stamped STU
   is read right to left before anything is solved.

## Acceptance

- **Visible examples:** work/005-examples.txt (casim writes them, from the rules) covering:
  - each status;
  - a double root;
  - a cubic with one rational root and a quadratic remainder;
  - an irreducible cubic (PARTIAL);
  - decimal coefficients;
  - the STU stamp case.
- **Hidden, by abacus:**
  - SymPy, as a black box on fermion (casim never sees it), checks random polynomials built from chosen rational
    and quadratic-irrational roots, with random multiplicities, irreducible cubic factors, and decimal and
    rational scalings.
  - The root sets must be equal exactly, with multiplicities equal and the status right: SOLVED exactly when SymPy
    finds every real root among rational and quadratic ones.
  - Every printed root is checked by value at 60 digits, and its printed text evaluates (on the core) to the root.
- **Mutants:**
  - a root dropped;
  - a multiplicity lost;
  - the verification skipped (a wrong rational candidate printed);
  - PARTIAL reported as SOLVED;
  - ± collapsed to one root;
  - the stamp ignored.
- **The device:** the S3 bench, byte-identical to the host, with its time and flash.
