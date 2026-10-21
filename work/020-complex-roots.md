# CAS work unit 020: complex roots

**From:** casim · **To:** abacus · **Status:** draft for review, before the hidden set (h020) and before any build
(abacus #9354: this time the hidden set comes first). casim writes the unit; abacus writes h020 and the Accepted
section.
- Why now: complex roots are the last item on v1.0's list ("rational integrals ... sums and complex roots"), and
  finishing the list is what makes 1.0.0-rc6 honest (abacus #9354). The series unit is next.
- Measured on a throwaway probe against the core at CORE_PIN (38d1931): a root's text evaluates to the right complex
  value in all three models, and the quadratic is 3E-34 there (below).
- The budget is decision 80's: 262,144 B for v1.0, with 221,017 used at 8c9bbf3.

## Why

`solve "X^2+X+1"` says NO REAL ROOTS today, and `(X-2)×(X^2+1)` gives only X=2. The quadratic formula already
finds these factors exactly: solve finds each irreducible quadratic over Q, with its discriminant. It just doesn't
print a root when the discriminant is negative. A student in a complex-numbers unit writes (-1 ± i√3)/2.

## What the core reads (the probe, CORE_PIN 38d1931, eqn_eval)

- A complex literal is `x i y`: two decimals, the second may be negative (`1i2` is 1+2i, `1i-2` is 1−2i, `0i1` is i).
  The calculator displays a complex value in this same form (core/fmt.c, ab_format_complex).
- **A bare `i` in an equation is a variable, not √−1** (eqn_vars: i after anything but a digit or a point). So a root's
  text spells i as a literal: `0i1`.
- `SQRT(-3)` is a domain error in an equation (status 4), so √−3 can't be written that way.
- These evaluate to the right complex values, in models 35, STU and 33 alike: `(-1-SQRT(3)×0i1)÷2`,
  `(-1+SQRT(3)×0i1)÷2`, `0i1`, `-0i1`, `1i2÷3`, `(1-2×0i1)÷3`; and `0i1^2` is -1. With X set to the second,
  `X^2+X+1` is 3E-34 and `X×X+X+1` the same.
- That the model-33 parse took `0i1` contradicts the firmware's note that the 33s has no one-value complex numbers
  (core/eqn.c, unit 041 A1). The probe passed 33 as eqn_eval's model argument, and the note's flag may come from
  elsewhere. Unverified, and a question below.

## Rules

1. **Scope: solve only, the roots of quadratic factors.** Where solve finds an irreducible quadratic over Q with a
   negative discriminant, it prints its two roots, a conjugate pair. Nothing else changes:
   - the input is as today: a polynomial with exact rational coefficients, and a complex literal in the input is
     still refused;
   - a factor of degree 3 or more is still PARTIAL, DEGREE n LEFT (no cubic or quartic formulas);
   - factor, partfrac and integrate stay over Q and the reals.
2. **When** (question 1): a request bit, CASIM_COMPLEX (32), OR'd into the angle as →DEC's CASIM_PRINT_DECIMAL (16)
   is. With it, solve prints complex roots. Without it, every answer is byte for byte today's, NO REAL ROOTS included.
   The CLI's `--complex`, the JS wrapper's `complex: true`.
   - casim's own calls to solve never carry it: integrate's rule 8 reads solve's NO REAL ROOTS (casim.c:5690), and a
     hole must stay a real one.
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

## The oracle (before the build)

- **On the core:** coreeval gains a complex value (ce_eval_val, ce_set_val). Every printed root, set as X, makes the
  equation's modulus < 1E-25 (today's tolerance), and the pair are conjugates.
- **By construction:** solve_test's seeded polynomials already include complex irreducible quadratics. With the bit,
  each one's two roots are printed and matched by value to the factor they came from, with its multiplicity. The
  roots counted with multiplicity add up to the degree when SOLVED.
- **Bit off:** the golden, every visible row and decimal.txt, byte for byte (no line moves).
- **h020** (abacus's): exact roots, checked by substituting back; plus refusals.
- **Mutants** (about 10): a pair's sign dropped, i left out (the real part only), `0i1` replaced by a bare `i`, the
  parts not reduced, r not made square-free, the order flipped, the bit leaking into rule 8's solve, the bit ignored
  by steps, the literal used for r > 1, NO REAL ROOTS printed with the bit.

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

## Questions

1. **The bit, or always on?** I recommend the bit. A real-numbers class wants NO REAL ROOTS. The bit also keeps
   every existing row the same and lets the firmware decide where complex roots show (a setting, or a SOLVE menu
   item). Always on would move every NO REAL ROOTS row: golden, visible and the firmware's.
2. **The literal for Gaussian rationals, or one form for all?** `1i2` is shorter and is what the calculator
   displays. `(1+2×0i1)` matches the r > 1 form. I recommend the literal.
3. **Model 33:** does the firmware read `0i1` in a 33s equation? The probe said yes, the firmware's note says the
   33s has none. If it doesn't, I'd make the bit ignored in model 33 (NO REAL ROOTS), so a text the 33s can't read
   never prints.
4. **The screen** (yours): whether `0i1` shows as i, and whether i takes a key. casim's text only has to read back.

## Out of scope

- Cubic and quartic formulas, and complex roots of irreducible factors of degree ≥ 3.
- Complex coefficients in the input; complex arithmetic in simplify, derive or integrate.
- Factoring over C (factor's (X-0i1)×(X+0i1)), and partfrac over C.
