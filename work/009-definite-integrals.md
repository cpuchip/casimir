# CAS work unit 009: exact definite integrals

**From:** abacus · **To:** casim · **Status:** ready.
- It's casim's proposal #5189 (candidate 2), measured by a throwaway prototype in #5380.
- It builds on CAS 006 (integrals, accepted) and CAS 008 (exact values, accepted at casim 8bd3e43, abacus
  docs/evidence/2026-10-09-cas-008-review.md).
- Casimir v1.0's scope is proposed to Michael on the roadmap (casimir-v1); this unit is in it either way.

## Why

The calculator's own ∫ gives a definite integral as a number. A student also wants the exact value: ∫SIN(X) from 0 to
π is 2, ∫1÷X from 1 to 3 is LN(3), and ∫SQRT(X) from 0 to 4 is 16÷3. CAS 006 finds F, and CAS 008 evaluates it at
exact points. This unit puts the two together, and refuses any interval on which the integral isn't an ordinary one.

## Rules

1. **The API:**
   - `casim_defint(eqn, var, lo, hi, model, angle, out, cap, arena, arena_cap)`, also reachable through casim_run as
     CASIM_DEFINT.
   - casim_req carries one replacement text today. This unit adds the second bound as a field; the firmware side
     (the struct's use, the key and its messages) is soroban's next CAS unit.
   - The CLI: `casim [--model …] [--cap N] defint "<t>" X LO HI RAD|DEG|GRAD`.
   - The integrand follows CAS 006's rule 1: an equation is refused as BAD_ARG.
2. **The integrand:** exactly CAS 006's scope, as typed, with rule 6a's spellings. Anything integrate refuses is
   refused with the same status (NOT_INTEGRABLE, TOO_LONG, NO_ROOM), and nothing new is integrated.
3. **The bounds:**
   - each one is parsed as subst parses its replacement, in the same model (the stamp, CAS 004);
   - **a bound must be an exact constant:** numbers, π, and what CAS 008 evaluates (SQRT(2), LN(2), π÷4, EXP(1)).
     A bound holding var, or any other letter, is BAD_ARG. Symbolic bounds (∫ from 0 to B) are a later extension:
     the interval check below needs numbers.
   - A bound with no value (LN(0), 1÷0) is UNDEFINED, CAS 008's status. A bound that isn't real is BAD_ARG.
   - lo > hi is allowed: the answer is F(hi) − F(lo) as written, so the sign comes out reversed. lo = hi gives 0.
4. **The interval check, on the integrand as typed (casim's prototype lesson: walking F refused TAN on [0, π÷4]):**
   - **The principle:** the answer is printed only when the integrand is proven real and finite at every point of
     the closed interval [min(lo, hi), max(lo, hi)], as the core would evaluate it. An endpoint counts: 1÷X on
     [0, 1] is refused.
   - **What fails it,** for u linear in var (CAS 006 only integrates linear arguments):
     - u to a negative power (1÷X, 1÷(aX+b), X^(−2), 1÷SQRT(X)): u = 0 anywhere on the interval;
     - u to a non-integer power (SQRT(X), X^(1÷3), (aX+b)^(3÷2)): u < 0 anywhere on the interval. Measure the
       core first: if its y^x gives a real answer for a negative base with an odd-denominator exponent, say so, and
       this unit follows the core. Otherwise u ≥ 0 is the rule, with u = 0 allowed only for a positive power;
     - TAN(u): COS(u) = 0 anywhere on the interval, in the angle mode. The poles are u = π÷2 + kπ in RAD,
       90 + 180k in DEG and 100 + 200k in GRAD.
   - **A pole or edge whose position depends on another letter** ~~(1÷(A×X+1))~~ can't be placed, so it is refused as
     BAD_ARG. A constant multiplier with letters (A×X^2) is fine: it has no pole. *Amended at the build (casim #5568,
     abacus #5571): rule 2 runs first, and CAS 006 refuses a letter inside a linear argument, so 1÷(A×X+1) is
     NOT_INTEGRABLE and no CAS 006 integrand reaches this path; it stays, for a wider scope later.*
   - **Refusal: a new status, CASIM_IMPROPER.** The integral is improper on this interval (a pole or domain edge on
     it), or the check couldn't prove it isn't. The firmware maps it to a message in its next CAS unit.
   - **Conservative at the edge:** the check uses the bounds' values from the abn evaluator. When a pole or edge is
     within a relative 1E-20 of the interval, it counts as on it. A refusal near the edge is allowed; a value
     printed across a pole is never allowed.
5. **The answer:** F(hi) − F(lo), built by the canonical constructors, so CAS 008's exact values apply.
   - **ABS and SGN of an exact constant fold**, when the abn evaluator decides its sign beyond the 1E-20 edge:
     ABS(−3) is 3, and ABS(COS(π÷4)) is 0.5×SQRT(2). Without this, F(3) for 1÷X would print LN(ABS(3)).
     It's a new case in fn(), so it applies to every operation: list its blast radius.
   - There is no "+C", since a definite integral has none.
   - TOO_LONG past 191 bytes, as CAS 006.
   - **Not in this unit:** combining logs (LN(6) − LN(2) stays; it isn't LN(3)) and rationalising (LN(0.5×SQRT(2))
     stays). Both are exact and right; whether to reduce them is a casimir-v1 question for Michael.
6. **Verification:** F is CAS 006's verified antiderivative (derive-back), and the interval check guarantees F is
   continuous on the interval (its LN(ABS(u)) and TAN forms come only from integrands the check passed). So F(hi) −
   F(lo) is the integral. casim adds no new check; abacus's hidden set judges every value by quadrature.
6a. **lo = hi on a pole or edge** (abacus #5571; the build already did this): the closed interval is the single point
    lo, and rule 4's principle holds there, so ∫1÷X from 0 to 0, ∫1÷SQRT(X) from 0 to 0, ∫SQRT(X) from −1 to −1 and
    TAN(X) from 90 to 90 in DEG are IMPROPER. An edge a positive power allows gives 0: ∫SQRT(X) from 0 to 0 is 0.
7. **Bounds, decision 68:**
   - flash measured, against the prototype's 2.7 KB plus the plumbing, TAN's DEG and GRAD poles, and the ABS/SGN fold;
   - frames ≤ 224 B on the new helpers;
   - the worst stack is within the depth guard's 12,288 B;
   - *At the build (casim 621516e): 12,032 B, +32 on derive's 46-deep nest, so 256 B are left. The next unit that
     touches derive's path recalibrates the depth guard (CASIM_DEPTH_MAX 47 → 46, so the deepest nest accepted is 45)
     as part of that unit (abacus #5571; the guard's value is 47 in src/casim.c, the nest that sets the worst stack 46).*
   - the arena is measured;
   - S3 times are measured, and the worst is under 1 s or the unit says what it costs.

## Acceptance

- **Visible examples,** work/009-examples.txt, written by casim from the rules:
  - ∫SIN(X) on [0, π] = 2; in DEG on [0, 180] = 360÷π; in GRAD on [0, 200] = 400÷π;
  - ∫1÷X on [1, 3] = LN(3); on [−3, −1] = −LN(3);
  - ∫SQRT(X) on [0, 4] = 16÷3;
  - ∫EXP(2×X) on [0, LN(2)] = 1.5;
  - ∫X^2 on [0, 1] = 1÷3, and on [1, 0] = −1÷3;
  - lo = hi gives 0;
  - ∫TAN(X) on [0, π÷4] (the answer, not a refusal);
  - ∫A×X on [0, 2] = 2×A;
  - **IMPROPER:**
    - 1÷X on [−1, 1] and on [0, 1];
    - 1÷SQRT(X) on [0, 1];
    - SQRT(X) on [−1, 1];
    - TAN(X) on [0, π÷2] in RAD, on [0, 90] in DEG, and on [0, 100] in GRAD;
    - 1÷(2×X−1) on [0, 1];
  - **BAD_ARG:** a bound with X, a bound with another letter~~, and 1÷(A×X+1)~~ (1÷(A×X+1) is NOT_INTEGRABLE by rule 2:
    N0023, amended at the build, abacus #5571);
  - UNDEFINED: a bound of LN(0);
  - NOT_INTEGRABLE passed through (X×LN(X));
  - the STU stamp.
- **The ABS/SGN fold's blast radius:** every changed check in all 16 suites, listed, as CAS 008's was.
- **Hidden, by abacus:**
  - CAS 006's integrand generator, with random exact bounds: integers, rationals, multiples of π, SQRT(2), LN(2),
    in all three modes, and with lo > hi sometimes;
  - every printed answer is judged against mpmath's quadrature at 60 digits, relative 1E-24;
  - every refusal must be IMPROPER exactly when abacus's own pole analysis puts a pole or edge on the closed
    interval. A near-edge refusal within 1E-20 is allowed and counted apart;
  - where every F term lands on CAS 008's table, the answer must hold no SIN, COS or TAN of a constant.
- **Mutants:**
  - an endpoint pole allowed (the open interval checked);
  - TAN's poles in DEG or GRAD placed as in RAD;
  - F(lo) − F(hi);
  - a fractional power's domain edge ignored;
  - ABS's fold with the wrong sign;
  - the interval check walking F instead of the integrand.
- **The device:** the S3 bench, byte-identical to the host, with time, stack, arena and flash.
