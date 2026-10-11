# CAS work unit 013: integrals of rational functions

**From:** abacus · **To:** casim · **Status:** ready.
- It's casim's proposal (#6679, completed by #6681), from a measured C prototype in scratch over 93ce2ca.
- It's next in v1.0's order after CAS 012 and its steps (both accepted, abacus docs/evidence/2026-10-10-cas-012-review.md).
- The budget is decision 80's: 262,144 B for v1.0.

## Why

On the shelf, five problems integrate a rational function (Thompson XIX 9–13). Four are within this unit: 9, 10 and 12
are LN only, and 13 has an ATAN. XIX 11 has letter coefficients and is out of reach. It's also the step after partial
fractions in every calculus course.

## Rules

1. **When:** integrate's table runs as today. Only when the table refuses (NOT_INTEGRABLE) does the rational path run.
   - The hook sits in casim_integrate's wrapper, not inside do_integrate, so do_integrate's frame (144 B) doesn't move.
   - A mixed integrand (SIN(X)+1÷(X^2-1)) stays NOT_INTEGRABLE.
2. **The input:** a quotient n÷d of polynomials in var, as CAS 012 reads it.
   - **Integrate's status stays NOT_INTEGRABLE whenever the rational path can't answer** (casim #6685, which corrects
     this rule's first wording). Another letter, NOT_POLYNOMIAL, a bound passed, and PARTIAL all keep integrate's
     NOT_INTEGRABLE, so old inputs keep their statuses.
   - The one exception is NO_ROOM: the arena ran out on an input that partfrac answers with more room.
   - The only status changes are refusals that become answers. Their golden lines are listed in
     tests/golden/moved/cas-013.txt.
3. **The path:**
   - partfrac runs whole on n÷d, on the arena it needs;
   - each term of its structured result (the factors, numerators and polynomial part, not its printed text) is
     integrated by the forms in rule 4;
   - before the terms are integrated, the factors, numerators and q are copied down and partfrac's work area is
     freed, so the arena fits (rule 9).
4. **The forms in:**
   - the polynomial part, by powers;
   - c ÷ (aX+b)^j for any j: LN(ABS(aX+b))÷a for j = 1, and the power rule for j ≥ 2;
   - (pX+q) ÷ (aX²+bX+c) to the first power with D = b² − 4ac < 0: an LN part and an ATAN part.
     - The LN prints without ABS, since the quadratic is positive everywhere.
     - ATAN carries the angle factor in DEG and GRAD (π÷180 and π÷200), as integrate's table does for its angle
       functions.
5. **The forms refused, as documented limits** (NOT_INTEGRABLE), each with a visible example:
   - a quadratic factor with D > 0 that isn't a square (X²−2): its LN form leaves radicals that the check can't close,
     and ATANH is real only between the roots;
   - a quadratic factor to a power ≥ 2 (XI 15's (X²+X+1)²): the reduction formula's terms don't combine under expand.
   - A second, substitution-based check could lift both. It is a follow-on, brought as its own design if demand shows,
     with its degree bound written down.
6. **The check, before anything prints:** each term's antiderivative is checked against its own term by integrate's
   closes(): derive it back, apply the check-only rewrites, expand the difference to 0. Together with partfrac's
   rule-7 identity, the term checks prove F′ = n÷d. A term that doesn't close means the whole answer is refused, never
   printed unchecked.
7. **The printed form:** integrate's canonical form, one answer style per op. The terms are joined in partfrac's order,
   polynomial part first. No "+C", as with integrate's other answers.
8. **No rows:** integrate answers for the reduced function, as it does for its other forms, and the values a common
   factor removed aren't printed. This is a choice: a student who wants the domain asks CANCEL or PARTFRAC.
9. **Bounds, decision 68 with decision 80's budget:**
   - **Frames:** no existing frame moves. The prototype measured do_irational at 240 B and ir_term at 192 B. On the stack
     the path is partfrac's own (bench worst about 3 KB) plus about 240 B. The masked bench gives the worst through the
     keys (abacus #6675's gate).
   - **Arena:** the textbook worst is within 8,192 B. The prototype needed up to 9,416 B on ATAN cases before the
     copy-down in rule 3. Past 8,192 the answer is NO_ROOM, never an overrun.
   - **Flash:** the prototype core is +3,010 B, and the estimate for the unit is 4 to 5 KB, steps excluded. Stop and
     report at twice that.
   - **Time:** the S3's worst, as before.
10. **Steps, as a second build of this unit after the answers are accepted:** integrate's walk for a rational integrand
    is partfrac's pages, then each term's ∫ and its CHECK. The rule ids come from the block at 256 and up, added only at
    the end.

## Acceptance

- **Visible examples,** work/013-examples.txt, written by casim from the rules:
  - XIX 9, 10, 12 and 13;
  - 1÷(X²−1); (X⁴+1)÷(X³+1); 1÷(X²+X+1);
  - a repeated linear factor to the 3rd power;
  - a cancelling case;
  - ATAN in DEG and GRAD;
  - both refused forms;
  - a mixed integrand;
  - NOT_NUMERIC, NO_ROOM and TOO_LONG.
- **Hidden, by abacus:**
  - random n÷d over linear powers and D < 0 quadratics;
  - judged by F′ = f at rational points with mpmath at 80 digits, in RAD, DEG and GRAD;
  - ABS on every LN of a linear factor, and none on a D < 0 quadratic;
  - NOT_INTEGRABLE exactly on rule 5's forms and on mixed integrands;
  - the shelf's XIX 9, 10, 12 and 13 against Thompson's printed answers, after abacus checks those answers.
- **Mutants:**
  - a term's check skipped;
  - ABS dropped on a linear factor's LN;
  - the angle factor dropped in DEG;
  - an ATAN argument off by its 2a;
  - a refused form let through;
  - the hook moved inside do_integrate, caught by the new frame-table gate below.
- **A frame-table gate in make check** (casim #6685's (a)):
  - host gcc -fstack-usage is compared with a recorded table (tests/frames/host.su);
  - an existing function whose frame changes or disappears fails, and a new function is listed;
  - its control is a planted frame change, which must go red;
  - it makes "no existing frame moves" a check for every unit from here on;
  - the S3's own table stays casim's per-unit record.
- **The device:** the S3 bench byte-identical to the host. Golden byte-identical for every other op, and every earlier
  hidden set rerun.
