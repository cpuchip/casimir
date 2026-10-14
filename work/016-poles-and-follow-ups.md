# CAS work unit 016: poles, ABS's corner, ∞−∞ of quotients, and CI on the public firmware

**From:** abacus · **To:** casim · **Status:** ready.
- This is the small follow-up named at CAS 014's and CAS 015's acceptance (work/014 rulings (a), (b) and (e); work/015
  follow-ups (1) and (2)), and casim's proposal #7916. That proposal measured each part on 4b91739 before it was written.
- The budget is decision 80's: 262,144 B for v1.0, with 170 KB used.

## Why

Three answers today are less than true:
- TAN(π÷2) is kept as if it had a value in six ops.
- X×ABS(X) at 0 is refused to order 1, where its series exists.
- 1÷X−1÷SIN(X) at 0, a textbook limit, is UNDECIDED.

And the firmware Casimir is pinned to is public now, so CI can test the pin itself.

## Rules

1. **Poles are UNDEFINED in every op, as 1÷0 already is.**
   - **The poles:** TAN where canon(COS(arg)) = 0, in every angle mode (TAN(π÷2), TAN(90) in DEG, TAN(100) in GRAD);
     LN and LOG of 0; ATANH of ±1. The check goes where a constant function is folded.
   - **The ops:** simplify, expand, subst, derive, integrate and collect, which keep these today. solve and factor
     answer UNDEFINED there too, not NOT_NUMERIC. taylor and limit already refuse.
   - **What doesn't change:** the domain exits with complex values (LN(-1), ASIN(2), SQRT(-4), (-8)^(1÷3)) stay
     symbolic, as now, for the calculator to value.
   - **Golden:** rows that hold a pole move to UNDEFINED. The build's report lists every moved row, and no other row
     moves.
2. **ABS's corner: a bounded factor times zero.** SGN(u) is bounded, so in a product with a factor that is 0 at a,
   SGN(u) at u = 0 gives 0 there.
   - X×ABS(X) at 0 to order 1 answers 0 (f'(0) = 0, which is true). Its order 2 stays UNDEFINED (f'' = 2×SGN(X) has
     no value at 0). ABS(X) at 0 to order 1 stays UNDEFINED.
   - UNDECIDED (CAS 015's status) is for an SGN(0) this rule can't clear. Taylor's UNDEFINED for a lone corner stays.
   - **The check against the truth:** each answered case is compared with the one-sided difference quotients at 50
     digits on both sides. That's abacus's spot check, with examples, at acceptance.
3. **∞−∞ of quotients at a finite point (limit).** When a sum's terms go to +∞ and −∞, put them over one denominator
   (canon's cancel route), then walk the quotient, so 0÷0 reaches L'Hôpital.
   - 1÷X−1÷SIN(X) at 0 answers 0, and 1÷LN(1+X)−1÷X at 0 answers 1÷2.
   - A combined form that outgrows the arena stays NO_ROOM until the series unit (below).
4. **The derivative text buffer stays 384 B on the calculator.** casim's measurement on 4b91739 (#7916, 600 cases per
   family): at the 8,192 B arena, 768 B answers no new case and loses 10, and 1,024 B loses 18, because both buffers
   come out of the arena. docs/language.md gets the table and the reason.
   - **Accepted as offered: 1,024 B when the arena is 16 KB or more.** That applies to the host and WebAssembly
     library, whose default arena is 65,536. At 64 KB it takes Taylor from 521 to 567 of 600, and nothing changes at
     8 KB.
   - A test runs both sizes, so the larger path doesn't go unjudged. The calculator's core builds at 8 KB, so the
     device never takes it.
5. **CI runs the firmware's tests at the pin.** A job fetches cpuchip/stu32-firmware at exactly CORE_PIN (by sha,
   depth 1), sets CORE_REPO and runs make test. If it can't fetch the pin, the job fails.

## Not in this unit: the series engine

NO_ROOM on composed forms (CAS 015's second set: 111 of 600) is the method, not the buffer. Repeated derivatives grow
faster than any buffer, and L'Hôpital multiplies that: 1÷X^2−1÷SIN(X)^2 at 0 is NO_ROOM even combined.
- **The structural fix:** truncated power series, with EXP, SIN, COS, LN(1+u), ATAN, SQRT(1+u) and the rest composed
  term by term to order n, over exact rationals and symbolic constants. Taylor and 0÷0 limits then cost n terms
  instead of n derivatives.
- It's its own unit, proposed after substitution and by-parts. Michael may rank it earlier.
- **The tension it carries:** the oracles of 014 and 015 (casim's, and abacus's series_truth.py) are built from series.
  If the engine moves to series, each oracle changes method to stay independent. That means values near the point at
  high precision and mpmath's numerical Taylor, which judge_cas014 already cross-checks with.

## Acceptance

- **Examples** (work/016-examples.txt): each pole in each op and angle mode, X×ABS(X) at orders 1 and 2, ABS(X) at
  order 1, and both ∞−∞ limits. Plus one of each complex-valued domain exit unchanged.
- **abacus's hidden sets rerun:**
  - the 014 set (h014) and both 015 sets (h015, h015b) give 0 failures;
  - the 46 UNDECIDED rows of h015 answer 0;
  - any other change is listed;
  - the ABS answers are spot-checked by difference quotients.
- **Mutants** (about 8):
  - each pole check dropped (TAN, LN/LOG, ATANH);
  - the SGN-product rule applied when the factor isn't 0;
  - the combine skipped;
  - the combine applied to a sum that isn't ∞−∞;
  - the arena threshold for the buffer off by one;
  - the CI job's pin taken from main.
- **The device:** no existing frame moves. The S3 bench is byte-identical to the host, and every earlier hidden set is
  rerun.
- **Size:** about a day of host work, by casim's estimate. Stop and report at twice that.

## Rulings during the build (abacus, 2026-10-14, casim #7950)

- **Fold the hyperbolic and ALOG constants: yes, in this unit.**
  - The values: SINH(0), TANH(0), ASINH(0), ATANH(0) and ACOSH(1) are 0; COSH(0) and ALOG(0) are 1. The fold goes in
    fn(), with the other exact values.
  - **Why it's in scope:** without it, rule 1's poles hide behind them (LN(SINH(0)), ATANH(COSH(0)),
    TAN(π÷2×COSH(0))). Taylor prints ATANH(0)+X+…, and a limit answers -0.5×SINH(0)÷COSH(0), right but unreduced.
  - **Golden:** each moved row is listed in the build's report.
  - **Mutant:** one, with the fold dropped.
- **ABS(X)−SQRT(X^2) at 0 to order 1, now UNDECIDED:** agreed. A corner cancelled by another hole isn't proven.
- **X×SQRT(X^2) at 0 to order 1.** Today it's UNDEFINED, because the formula X÷SQRT(X^2) has no value there, though
  f'(0) = 0. It is not the same as X÷X.
  - **The difference:** rule 7's typed-f clause is about the input itself. This 0÷0 arises only inside a derivative's
    formula, where it says "this formula can't tell", not "no derivative". A pole in a derivative (c÷0 with c ≠ 0,
    as SQRT(X)'s at 0) is proof and stays UNDEFINED.
  - **Ruled:** a 0÷0 that appears only in a derivative's formula, not in the typed f, answers UNDECIDED. Make it so in
    this unit if it's cheap; if it isn't, record it in docs/language.md as the known conservative case and leave it to
    the series unit, which answers it by series.
  - **The judge:** abacus's 014 judge then counts UNDECIDED as allowed on its UNDEFINED rows, as 015's does. UNDECIDED
    is never a wrong answer.

## Accepted (abacus, 2026-10-14), at 2833f14

**The host half, on f7a0664.** f7a0664 and 14f738c were built clean on fermion in a capped
container, and `make check` was green (310 checks). 14f738c reproduces CAS 015's acceptance answers byte for byte.
Every case ran through the CLI at the S3's 8,192 B arena, and the Taylor set again at 65,536.
- **The earlier hidden sets give 0 failures,** and every change against 14f738c is a refusal becoming a right answer:
  - Taylor (2,000 cases, seed 14014): at 8 KB, no row changes. At 64 KB, 122 change: 105 NO_ROOM become right answers
    and 17 NO_ROOM become TOO_LONG (rule 4's 1,024 B buffer). ANSWER rises 615 → 705 and NUMERIC 542 → 557.
  - Limits (1,500, seed 15015): all 46 UNDECIDED, every one 1÷X−1÷SIN(X) at 0, now answer 0 (rule 3).
  - Random 0÷0 compositions (600, seed 15017): NO_ROOM falls 111 → 67. 26 become UNDEFINED on no-limit rows and 18
    become right values, because the two-sided walk now compacts between its sides.
- **Rule 2 gets a set of its own** (abacus gen_abs seed 16016), since none of the hidden sets holds an ABS: 150
  functions with an ABS whose argument is 0 at the point, at orders 0 to 4, 750 rows.
  - **How it's judged:** an answer P of order n must leave (f(a±h) − P(a±h)) ÷ h^n going to 0 on both sides, at 150
    digits. An UNDEFINED must leave the two one-sided n-th coefficients different, against casim's own order n−1
    answer (itself checked).
  - **0 failures at both arenas:** 354 values right, 24 UNDEFINED right, 61 UNDEFINED above an UNDEFINED order.
  - **310 UNDECIDED, all sound:** 72 of them have a value (ABS(X)^3 and ABS(ATAN(X))^2 to order 2, for two), 50 are
    truly UNDEFINED, and 188 sit above an UNDECIDED order.
  - **Falsified:** casim's answered and UNDEFINED examples pass, and 6 planted wrong answers fail.
  - **abacus's own faults, fixed before the verdict:** the generator first wrote E(X−1)P at point 1, and the check
    first ran at 50 digits, which lost a 1e-72 remainder and gave 3 false failures on true UNDEFINEDs.
- **Poles, reproduced by hand:** TAN(π÷2), LN(0) and ATANH(1) in simplify and expand, TAN(90) in DEG and TAN(100) in
  GRAD in simplify, and a pole in derive, integrate, factor and solve all answer UNDEFINED. TAN(90) in RAD is kept, and
  LN(−1) stays symbolic.
- **The moved golden lines** (tests/golden/moved/cas-016.txt) were read one by one. The two new Taylor answers and
  (1−COS(X))^2÷X^4 → 0.25 agree with sympy.

**The device half.**
- 2833f14 returns do_solve's S3 frame to 288. The change only calls pw_refused when the status is already refused,
  where it returns at once otherwise. abacus read it as behavior-identical.
- casim's report (#8048):
  - **Frames:** none moved except lm_inf_minus_inf 128 → 96, whose test moved into lm_dominant. The threadchip
    seat's measurement was reproduced by size-s3.sh on lepton.
  - **Size:** text 173,367 (+3,627) of 262,144.
  - **Bench:** DEVICE PASS on the XIAO, with all 1,065 cases byte-identical to the host, CAS 016's 89 examples among
    them.
  - **Stack gate:** unchanged at 11,712 of 12,288.
- casim caught that the bench had no CAS 016 block before the run, and added one (071b056).
- abacus did not watch the device run.

**CI:** 7/7 on f7a0664 (run 38502774799) and on 2833f14 (run 38503999468). The firmware job fetches cpuchip/stu32-firmware
by sha at CORE_PIN 38d1931, which is public.

**Mutants:** 193 of 193 caught (casim). The list holds every mutant the brief names.

**Named, not fixed** (docs/language.md, 109b607):
- **For the series unit:**
  - SQRT(X^2) to order 1 is UNDECIDED, where the truth is UNDEFINED;
  - the 72 ABS rows above, which have values;
  - 1÷X^2−1÷SIN(X)^2 at 0, still NO_ROOM;
  - TAN(X)−1÷COS(X) at π÷2 from below, still UNDECIDED.
- **For the next unit that touches solve:** solve and factor's X÷0 is NOT_NUMERIC, the one ÷0 that isn't UNDEFINED.
