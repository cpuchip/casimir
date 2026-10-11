CAS 013 (work/013-rational-integrals.md) later gave rational integrands an answer; I0024 moved with it.

# CAS work unit 006: simple indefinite integrals

**From:** abacus · **To:** casim · **Status:** ready. The plan's order (exact solving, then simple integrals), on
casim's proposal #5117. It follows CAS 005, accepted at casim 3bae969 for the host and 666d403 for the S3 bench
(abacus docs/evidence/2026-10-08-cas-005-review.md).

## Why

A student integrates 3x² + cos(2x) by the table: x³ + sin(2x)/2. The calculator's ∫ gives a number between two
limits, never the antiderivative. Casimir can give the antiderivative exactly for the table's forms, and refuse
everything else. This unit is the host library and CLI. The device's key is a later firmware unit, as for CAS 004
and 005.

## Rules

1. **The API:**
   - `casim_integrate(eqn, var, model, angle, out, cap, arena, arena_cap)`, also reachable through casim_run as
     CASIM_INTEGRATE;
   - the CLI: `casim [--model …] [--cap N] integrate "<t>" X RAD|DEG|GRAD`.
   - The input is an expression. An equation (with `=`) is refused as BAD_ARG.
2. **What is integrated (casim's scope, #5117):**
   - linearity: sums, and constant multiples, including constants that hold other letters (A×X^2 gives
     A×X^3÷3);
   - x^n for any rational n ≠ −1, as x^(n+1)÷(n+1), including SQRT(X), X^(1÷3) and 1÷X^2;
   - 1÷X as LN(ABS(X)), and 1÷(aX+b) as LN(ABS(aX+b))÷a;
   - EXP, SIN, COS, TAN, ALOG, SINH, COSH and (aX+b)^n, each of a linear argument aX+b only, by the linear
     substitution;
   - a polynomial times one of these, only where expanding makes every term one of the forms above.
   - **Out of scope:** integration by parts, other substitutions, partial fractions of degree ≥ 2, definite
     integrals. The core's own ∫ already does definite integrals numerically.
   - **Anything else is NOT_INTEGRABLE** (a new status), a refusal that changes nothing.
3. **The angle mode,** as CAS 001 rule 3. The core evaluates trig in the angle mode, so the antiderivative must
   carry the factor:
   - in DEG, ∫SIN(u) is −COS(u)×180÷π÷a;
   - in GRAD, the factor is 200÷π;
   - RAD has no factor.
   - TAN and the hyperbolic functions follow the same rule. SINH and COSH are not angle functions, so they carry no
     factor in any mode.
4. **Every answer is verified exactly before it is printed (casim's proposal):**
   - F is differentiated by casim's own derive, in the same angle mode;
   - the result must simplify to exactly the integrand's canonical form (002).
   - If it doesn't, the answer is NOT_INTEGRABLE and nothing is printed. A correct antiderivative that the check
     can't close is a refusal, never a wrong answer.
   - **Settled by prototype (casim #5123, accepted #5124).** The comparison is (F′ − f), canonicalized and
     expanded, which must be exactly 0. The DEG and GRAD factors close. Two rewrites apply **inside this comparison
     only**, never in 002's canonical form or any operation's output:
     - SGN(u)×ABS(u)^(−1) → u^(−1), for the same canonical u. Both sides are undefined at u = 0, and the identity
       holds everywhere else.
     - TAN(u) → SIN(u)×COS(u)^(−1), on both sides. They are undefined at the same points.
     - Each rewrite is a mutant: without it, LN and TAN answers are refused. A dropped ABS passes the derivative
       check on x > 0, so abacus's quadrature judges ABS.
5. **The output:**
   - one expression, F, in 002's canonical form, **with no "+C"**. On the calculator C is a variable register, so
     "+C" in the text would be read as the stored C when the result is evaluated or stored. The firmware unit
     that adds the key shows "+C" beside the answer, as a label outside the text.
   - TOO_LONG past 191 bytes, whatever cap is.
6. **The model and the domain:**
   - casim_run's model reads the stamp, as in CAS 004. A power chain stamped STU is read right to left first.
   - The integrand is judged **as typed**, as CAS 005's rule 2 amended. Nothing is simplified away before the
     form is recognised, so X^2÷X is NOT_INTEGRABLE rather than X^2÷2. The student typed a function undefined at
     0, and this unit doesn't decide what its antiderivative means. That is the conservative reading. casim
     proposes otherwise if it costs a common textbook form.
6a. **Equivalent spellings (abacus #5180, on casim #5179).** The principle comes from CAS 005: refuse only where
   combining the typed powers would hide a point where the typed function is undefined.
   - **A power of a power with a whole-number outer exponent is combined:** (X^r)^n = X^(r×n), where n is a whole
     number. The domain is unchanged, so SQRT(X)^3 and X^(1÷2)^3 (in model 35) integrate as X^(3÷2). A fractional
     outer exponent is refused, since (X^2)^(1÷2) is ABS(X).
   - **INV(u) is 1÷u** (so INV(X^2) is 1÷X^2), and **SQ(u) is u^2.**
   - **Products and quotients of powers of X combine,** and are refused only when an X stands in a denominator
     and the combined exponent is 0 or more: X^2÷X and X^2÷SQRT(X) hide 0. So 1÷X÷X, X÷X^3 and SQRT(X)÷X
     integrate: their domains are the same as X^(−2) and X^(−1÷2). For a linear argument, the same rule applies if
     the same code gives it; otherwise those are refused.
   - **COS(π×X) stays refused in this unit,** since a and b are rational. Arguments in rational multiples of π are
     a later extension.

7. **The bounds:**
   - **Flash: casim's budget is raised to 80 KiB (81,920 B)**, casim's option A in #5117, ruled here by abacus.
     The 64 KiB was our own discipline, not the chip's limit: the app partition is 7 MB with about 1.7 MB free.
     -O2 stays, so the CAS 005 times and stacks stand.
   - The worst stack for any operation stays within the depth guard's 12,288 B. The firmware's integration unit
     sizes its calc task from soroban's measured high-water mark (abacus #5106).
   - The worst case on the S3 is under 1 s, or the unit says what it costs.
   - Flash, stack and arena are measured and reported, as in CAS 005's bench.

## Acceptance

- **Visible examples:** work/006-examples.txt, written by casim from the rules. They cover:
  - each form in rule 2, and each status;
  - DEG and GRAD factors;
  - 1÷X and 1÷(aX+b);
  - a refusal of each out-of-scope kind: X×LN(X) (by parts), X×EXP(X^2) (a non-linear substitution),
    1÷(X^2−1) (partial fractions), and SIN(X)^2;
  - a constant with another letter;
  - the STU stamp case.
- **Hidden, by abacus:**
  - SymPy, as a black box on the reviewer's host (casim never sees it), builds integrands from rule 2's forms, with random
    coefficients, linear arguments and angle modes, plus out-of-scope integrands.
  - Each printed F is judged twice by value, relative 1E-24 at random points where everything is real:
    - F′ by mpmath's numeric derivative must equal the integrand;
    - F(x1) − F(x2) must equal mpmath's quadrature of the integrand from x2 to x1, on an interval with no
      singularity. This catches a missing ABS, where the derivative check alone would pass on x > 0.
  - Each refusal must be NOT_INTEGRABLE exactly when the integrand is outside rule 2 as typed.
- **Mutants:**
  - the ÷a of the linear substitution dropped;
  - the sign of ∫SIN lost;
  - ABS dropped from LN;
  - the DEG factor lost;
  - the verification skipped, with a wrong F printed;
  - the stamp ignored;
  - "+C" printed.
- **The device:** the S3 bench, byte-identical to the host, with its time, stack, arena and flash.
