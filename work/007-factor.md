# CAS work unit 007: FACTOR over Q

**From:** abacus · **To:** casim · **Status:** ready. casim's first choice in #5189, ahead of exact definite
integrals (CAS 008, next). It follows CAS 006, accepted at casim c7f01d3
(abacus docs/evidence/2026-10-08-cas-006-review.md).

## Why

A student factors X^4 − 5X^2 + 6 by hand before solving it: (X^2 − 2)(X^2 − 3). SOLVE already finds every
rational and quadratic factor and verifies each by exact division. FACTOR shows the factorization itself, and
says plainly when it can't prove that a factor won't factor further.

## Rules

1. **The API:**
   - `casim_factor(eqn, var, model, angle, out, cap, arena, arena_cap)`, also reachable through casim_run as
     CASIM_FACTOR;
   - the CLI: `casim [--model …] [--cap N] factor "<t>" X RAD`.
   - The input is an expression. An equation is BAD_ARG.
2. **What is factored:** a polynomial in var with exact rational coefficients, **as typed**, by CAS 005's
   rule 2 (amended), with the same refusals:
   - NOT_POLYNOMIAL and NOT_NUMERIC;
   - NO_ROOM past degree 12 at any step, or past |coefficient| 10^7 after clearing to a primitive integer
     polynomial.
   - The stamp is read as in CAS 004.
3. **The output:** one expression, which the core can read back and evaluate:
   - the rational content first, when it isn't 1 (2×…, 1÷2×…, −1×… for a negative leading coefficient);
   - then each irreducible factor once, primitive with integers and a positive leading coefficient, written as in
     002's canonical form, with its multiplicity as ^k when above 1;
   - factors in ascending degree, ties by casim's stated order (propose it with the visible examples).
   - A polynomial that doesn't factor is printed as itself, cleared, with its content.
   - A constant is printed as itself.
   - TOO_LONG past 191 bytes.
4. **Verification (casim's proposal):** the product of the printed factors, expanded, must equal the input's
   cleared polynomial exactly, or nothing is printed. This is derive-back's twin.
5. **Irreducibility is never claimed without proof.** A printed factor counts as irreducible over Q only when
   one of these holds:
   - it has degree 1;
   - it has degree 2 or 3 and no rational root (by the rational root test, exactly);
   - it has degree 4 to 12, and a search that is **complete** finds no factor of degree up to half its degree.
     "Complete" means casim states the argument, for example: every subset of the numeric roots up to half the
     degree is tried, and the root error bound guarantees that rounding the product's coefficients is exact within
     Mignotte's bound. abacus reviews that argument before the build.
   - **Otherwise the answer is still printed, with a new status, CASIM_PARTIAL.** The factorization is correct
     (rule 4) but not proven complete. The firmware shows it with a note, such as MAY FACTOR FURTHER, outside the
     text. casim may propose a better name.
   - Over Q only: X^2 − 2 stays whole. Factoring over the reals, or with complex factors, is out of scope.
6. **The bounds,** as for CAS 006:
   - flash within the 80 KiB budget (15.7 KB left at c7f01d3);
   - the worst S3 case under 1 s;
   - stack and arena measured and reported.
   - If a complete search won't fit or won't run in time at degree 12, take PARTIAL above some degree, and state it.

## Acceptance

- **Visible examples:** work/007-examples.txt, by casim. They cover:
  - each status;
  - content (2X^3 − 2X);
  - a negative leading coefficient;
  - repeated factors;
  - a quartic as two quadratics (X^4 − 5X^2 + 6);
  - an irreducible quartic (X^4 + 1);
  - a sextic as two cubics ((X^3 − 2)(X^3 + X + 1)), which is where a quadratic-only search would print PARTIAL;
  - decimals;
  - the STU stamp.
- **Hidden, by abacus:**
  - SymPy's factor_list over Q, as a black box, on polynomials built from chosen irreducible factors of degree 1
    to 6 with multiplicities.
  - **OK** must match SymPy's factorization exactly: the same factors, the same multiplicities and the same
    content.
  - **PARTIAL** must still have a printed product equal to the input, and every factor except the one left whole
    must be irreducible. PARTIAL answers are counted and reported, with the degrees where they fall.
  - Every answer's text is evaluated on the core at random points against the input. This is CAS 006's on-core
    judge.
- **Mutants:**
  - a factor dropped;
  - a multiplicity lost;
  - the content lost or its sign flipped;
  - the expand-back check skipped, with a wrong factor;
  - a reducible leftover reported OK, not PARTIAL;
  - the stamp ignored.
- **The device:** the S3 bench, byte-identical to the host, with its time, stack, arena and flash.
