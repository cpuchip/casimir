# CAS work unit 020: complex roots

**From:** casim · **To:** abacus · **Status:** reviewed by abacus (#9362: GO on the shape; the four questions ruled
and three additions, folded in below). The build waits for h020's hash (abacus #9354: this time the hidden set comes
first). casim writes the unit; abacus writes h020 and the Accepted section.
- Why now: complex roots are the last item on v1.0's list ("rational integrals ... sums and complex roots"), and
  finishing the list is what makes 1.0.0-rc6 honest (abacus #9354). The series unit is next.
- Measured on a throwaway probe against the core at CORE_PIN (38d1931): a root's text evaluates to the right complex
  value in all three models, and the quadratic is 3E-34 there (below).
- The budget is decision 80's: 262,144 B for v1.0, with 221,017 used at 8c9bbf3.

## Why

`solve "X^2+X+1"` says NO REAL ROOTS today, and `(X-2)×(X^2+1)` gives only X=2. The quadratic formula already
finds these factors exactly: solve finds each irreducible quadratic over Q, with its discriminant. It just doesn't
print a root when the discriminant is negative. A student in a complex-numbers unit writes (-1 ± i√3)/2.

## What the core reads (the probe: CORE_PIN 38d1931, then firmware main 6f12c13; eqn_eval)

- A complex literal is `x i y`: two decimals, the second may be negative (`1i2` is 1+2i, `1i-2` is 1−2i, `0i1` is i).
  The calculator displays a complex value in this same form (core/fmt.c, ab_format_complex).
- **A bare `i` in an equation is a variable, not √−1** (eqn_vars: i after anything but a digit or a point). So a root's
  text spells i as a literal: `0i1`.
- `SQRT(-3)` is a domain error in an equation (status 4), so √−3 can't be written that way.
- These evaluate to the right complex values, in models 35, STU and 33 alike: `(-1-SQRT(3)×0i1)÷2`,
  `(-1+SQRT(3)×0i1)÷2`, `0i1`, `-0i1`, `1i2÷3`, `(1-2×0i1)÷3`; and `0i1^2` is -1. With X set to the second,
  `X^2+X+1` is 3E-34 and `X×X+X+1` the same.
- **The same probe on firmware main's core** (stu32-firmware 6f12c13, the repin's, built from its clone in scratch):
  every form above, and `0i-1`, `1i-2`, `1i-2÷3`, `-SQRT(3)×0i1` and `2×SQRT(3)×0i1÷3`, evaluates right in models 35
  and STU; X^2+X+1 at the root is 3E-34 again. **In model 33 every complex form is refused (status 16).** CORE_PIN's
  core took them under 33 because it predates unit 041 A1 (a0c8fe9, between 38d1931 and main; abacus #9362, read in
  eqn.c:361, where the complex branch needs `!p->m33`). The run agrees with abacus's reading.

## Rules

1. **Scope: solve only, the roots of quadratic factors.** Where solve finds an irreducible quadratic over Q with a
   negative discriminant, it prints its two roots, a conjugate pair. Nothing else changes:
   - the input is as today: a polynomial with exact rational coefficients, and a complex literal in the input is
     still refused;
   - a factor of degree 3 or more is still PARTIAL, DEGREE n LEFT (no cubic or quartic formulas);
   - factor, partfrac and integrate stay over Q and the reals.
2. **When** (ruled #9362 (1)): a request bit, CASIM_COMPLEX (32), OR'd into the angle as →DEC's
   CASIM_PRINT_DECIMAL (16) is. With it, solve prints complex roots. Without it, every answer is byte for byte
   today's, NO REAL ROOTS included. The CLI's `--complex`, the JS wrapper's `complex: true`. The firmware decides
   where it is set (a CAS setting, probably: Michael's call, abacus puts it to him).
   - **Model 33 ignores it** (ruled #9362 (3)): NO REAL ROOTS there, byte for byte, because a 33s equation can't read
     `0i1` (firmware main refuses every form under 33, above).
   - **Any op accepts it**, and only solve reads it: a firmware that ORs it into every call (one CAS setting) gets
     every other answer unchanged.
   - **One boundary** (ruled #9362 (a)): the bit reaches only solve's three public paths: casim_run's CASIM_SOLVE,
     casim_solve, and casim_step's solve walk (do_ssteps). It goes to do_solve as a parameter of its own, not inside
     the angle, and start() takes it out of the angle for every op, as it takes →DEC's out. So no internal call can
     inherit it. The internal callers, each passing it off:
     - **ir_hole** (casim.c:5689): integrate's rule 8 and partfrac's cancelled factors call do_solve and read its
       NO REAL ROOTS for a "p=0" hole (abacus #8423). It passes the caller's angle today, so a bit left in the angle
       would turn a real hole test complex. This is the one internal do_solve call;
     - **quad_roots and put_root** have three callers, all of them solve's own: do_solve (casim.c:4725), and solve's
       steps' QUADRATIC FORMULA page and its result (casim.c:4996-5002, 5093-5099). They take the bit from
       do_solve's or do_ssteps' parameter, never from an angle;
     - **the real-root tests that aren't solve's** decide b^2 < 4ac for themselves and never read the bit:
       partfrac's SPLIT of a quadratic with no real root (casim.c:8502) and integrate's S2.6 tests (casim.c:9501,
       9547, 9564).
3. **The text of a root** (question 2), from the exact factor ax²+bx+c, d = b²−4ac < 0, −d = k²r with r
   square-free, the parts in lowest terms over D:
   - r = 1 (Gaussian rational): the core's own literal, `AiB`, then `÷D` when D isn't 1: X^2-2×X+5 gives `1i-2` and
     `1i2`; X^2+1 gives `0i-1` and `0i1`; 9×X^2-6×X+5 gives `1i-2÷3` and `1i2÷3`. It is the form the calculator
     displays a complex number in;
   - r > 1: `(A-B×SQRT(r)×0i1)÷D`, as today's real roots print `(A-B×SQRT(r))÷D`, with the same omissions (no `B×`
     for 1, no parentheses or `÷D` for D = 1, no `A` for 0): X^2+X+1 gives `(-1-SQRT(3)×0i1)÷2` and
     `(-1+SQRT(3)×0i1)÷2`; X^2+3 gives `-SQRT(3)×0i1` and `SQRT(3)×0i1`.
   - Every root's text evaluates on the core to the root (rule 5's check, as today).
4. **The answer line:**
   - the real roots first, ascending, as today; then the complex pairs, by real part, then the negative imaginary
     part first; a multiplicity above 1 as ` (MULT k)`, as today;
   - SOLVED when every factor has degree ≤ 2. With the bit, NO REAL ROOTS no longer occurs: X^2+1 is
     `SOLVED: X=0i-1; X=0i1`;
   - PARTIAL, DEGREE n LEFT as today, with the complex roots of the quadratics found;
   - past 191 bytes, TOO_LONG, as today. A root costs its text plus `; X=` (5 B). × and ÷ are 2 B each. So
     `(-1-SQRT(3)×0i1)÷2` is 21 B; a pair from 7-digit coefficients is about 45 B a root. Two quadratics fit easily;
     six (degree 12) usually don't.
5. **Show steps** (solve's walk, with the bit): QUADRATIC FORMULA's page shows the negative discriminant, as today
   (`0^2-4×1×1=-4<0`), then the pair, in place of NO REAL ROOTS.
6. **With →DEC and with rule 5's fallback** (ruled #9362 (b)): solve's root text doesn't go through the decimal
   printer (put_root writes integers, ÷ and SQRT(r); today `--decimal solve "X^2-0.25"` is `X=-1÷2; X=1÷2`, and
   `4×X^2-2` keeps `SQRT(2)÷2`). So a complex root prints the same with →DEC: the literal's parts stay integers, ÷D
   and SQRT(r) are kept, and it still evaluates to the root. Rule 5's fallback reruns with →DEC, gets the same text,
   and a complex answer past 191 bytes stays TOO_LONG, never DECIMAL_FORM. Turning the bit off gives the real roots,
   which may fit.

## The oracle (before the build)

- **On the core:** coreeval gains a complex value (ce_eval_val, ce_set_val). Every printed root, set as X, makes the
  equation's modulus < 1E-25 (today's tolerance), and the pair are conjugates.
- **By construction:** solve_test's seeded polynomials already include complex irreducible quadratics. With the bit,
  each one's two roots are printed and matched by value to the factor they came from, with its multiplicity. The
  roots counted with multiplicity add up to the degree when SOLVED.
- **Bit off:** the golden, every visible row and decimal.txt, byte for byte (no line moves). **Model 33 with the
  bit:** byte for byte the bit-off answer, over the golden's solve inputs.
- **Bit + →DEC:** every bit-on answer's text equals its →DEC text (rule 6), over the golden's solve inputs; and a
  TOO_LONG one stays TOO_LONG through casim_run's fallback.
- **The boundary:** integrate and partfrac with the bit OR'd in are byte for byte their bit-off answers over the
  golden's inputs (ir_hole's NO REAL ROOTS kept); so are every other op's.
- **Firmware main's core** (ruled #9362 (c), the device half): one root of each form (literal with and without ÷D,
  with and without A, SQRT with and without B and parentheses) evaluated on stu32-firmware main's core, models 35
  and STU, with the equation's modulus < 1E-25 there; and every form refused under 33. CORE_PIN stays where it is (a
  repin of the tests' core is its own change).
- **h020** (abacus's): exact roots, checked by substituting back; plus refusals.
- **Mutants** (about 12): a pair's sign dropped, i left out (the real part only), `0i1` replaced by a bare `i`, the
  parts not reduced, r not made square-free, the order flipped, the bit ignored by steps, the literal used for r > 1,
  NO REAL ROOTS printed with the bit; and from the review, **the bit honoured in model 33**, and **the boundary
  opened** (start() leaving the bit in the angle, so ir_hole inherits it).

## Cost (estimate, before the build)

- **S3 text, priced in two parts, each with its own stop line** (abacus #9360: steps walks and cross-cutting units
  have landed at 2–4.6× their estimates, answer-only units at 1.0–1.3×). The report goes out at the line, not after
  it. quad_roots and put_root are always inline, with three callers between them (solve, and two in solve's
  steps), so the branch is paid once per caller:
  - **the answer** (quad_roots' complex branch, put_root's two forms, the sort by real then imaginary part, the
    bit's plumbing through casim_run, the CLI and the JS option): **1.5 KB, stop at 3 KB**;
  - **the steps** (QUADRATIC FORMULA's page and the result page printing the pair): **1 KB, stop at 2 KB**.
  - So 2.5 KB together, against 41,127 B left for this unit, the series unit and the ratio shift.
- **Arena: about +200 B.** iroot gains the imaginary part for sorting (a decimal128, 16 B, for each of its 12
  slots). No new work area.
- **Stack:** solve isn't on the gate's path (the STU chain is), and put_root is a leaf.
- **Time:** none to speak of. The quadratic is already found; this prints it.

## Ruled by abacus (#9362)

1. The bit: yes. Off is byte for byte; the firmware decides where it is set.
2. The literal for r = 1: yes. It is the display's own x i y (core/fmt.c:394), and `1i-2÷3` mirrors today's `-2÷3`.
3. Model 33: the bit is ignored, NO REAL ROOTS byte for byte. Read by abacus in eqn.c; run on firmware main above.
   A mutant honours the bit in 33.
4. The screen is the firmware's: SOLVE's answer shows as a report (calc.c:3876, cas_rep), so nothing on the device
   parses a root back, and `0i1` shows as text until a firmware card says otherwise.
- Added: (a) one boundary for every internal caller, listed in rule 2, with a mutant; (b) →DEC and the fallback,
  rule 6, both in the oracle; (c) each form evaluated on firmware main's core, models 35 and STU.
- Costs and stop lines as priced: the answer 1.5 KB (stop 3), the steps 1 KB (stop 2).

## Out of scope

- Cubic and quartic formulas, and complex roots of irreducible factors of degree ≥ 3.
- Complex coefficients in the input; complex arithmetic in simplify, derive or integrate.
- Factoring over C (factor's (X-0i1)×(X+0i1)), and partfrac over C.
