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
7. **The printed form:** the canonical form of the whole sum, exactly as integrate prints every other answer, one answer
   style per op.
   - The checked terms are rebuilt as one sum before printing. Each term is still checked by closes() first (rule 6),
     with the arena reset after each.
   - No "+C".
   - This corrects the first wording, "joined in partfrac's order" (casim #6709): it disagreed with the canonical
     form whenever 002's order differs from partfrac's, as I0024's did.
8. **A cancelling integrand is refused (NOT_INTEGRABLE),** keeping CAS 006's guarantee (casim #6707, which corrects this
   rule's first wording, "answers for the reduced function"):
   - integrate never prints an antiderivative that's defined where the integrand isn't;
   - so when partfrac's gcd isn't 1 (something cancelled), the rational path refuses;
   - X^2÷X, (X^2+X)÷X and (X^2-1)÷((X-1)×(X+2)) stay NOT_INTEGRABLE, while 1÷(X^2-1) and the other reduced forms
     answer;
   - a student who wants the reduced function's integral cancels first (CANCEL or PARTFRAC), and then sees the
     removed values as rows.
   - The 0.9.0 release's published guarantee ("an exact answer, or a status that says why not") reads the same way.
   - **Narrowed with CAS 017's NO_ROOM fix (abacus #8386, #8388):** the refusal holds for every method only where a
     removed value is a true hole, so only there does it stay NOT_INTEGRABLE before CAS 017's search runs. Each "X=a"
     row of partfrac is put into the rational path's answer, and a value there means a hole (a "p=0" row counts too).
     (7×X−7)÷(X^2−2×X+1) cancels X−1, but its answer has no value at X=1, so nothing is a hole: it still answers
     3.5×LN(ABS(-2×X+X^2+1)), as abacus confirmed. Every other cancelling integrand goes on to CAS 017's search.
   - **And a "p=0" row only where it has a real root (abacus #8423):** a removed factor with no rational root and no
     real root (solve's NO REAL ROOTS: X^2+6) is no hole, so (X^4+7×X^2+6)÷(X^2+6) goes on to the search and answers.
     One with real roots (X^2−2), or one solve leaves PARTIAL (X^3−2), still counts.
   - **Amended: the reduced function, where nothing removed is a hole (abacus #8494 item 2, #8510).** Where a factor
     cancelled and no removed value is a hole, the reduced function is defined exactly where the integrand is, so
     its antiderivative keeps CAS 006's guarantee. The rational path then answers for cancel's reduced form, before the
     search: (24×X^3+43×X^2+61×X+7)÷((3×X^2+5×X+7)×(X+1)) is 8×(X+0.125)÷(X+1), so 8×X−7×LN(ABS(X+1)), where it was
     NOT_INTEGRABLE at every arena.
     - **Why before the search:** one answer per input at every arena. After it, (X^3+X^2+X+1)÷(X^2+1) printed
       X+0.5×X^2 where the search ran out (8,800 B) and the search's longer form, equal, at 64 KB.
     - **What moved:** answers the search gave become the reduced form's: (7×X−7)÷(X^2−2×X+1) is 7×LN(ABS(X−1)), was
       3.5×LN(ABS(−2×X+X^2+1)), and (X^4+7×X^2+6)÷(X^2+6) is X+X^3÷3 at 64 KB. NO_ROOM ones answer. Where the reduced
       form's rational path refuses, the search runs as before; a hole still refuses (X^2÷X).
     - **Steps:** the reduced form's walk after a COMMON FACTOR template, ∫(typed)dX = ∫(reduced)dX; the root's line
       names the typed integrand, so the last step is integrate's answer. The template is hidden in one line.
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
    the end. The answers were accepted at 62bc1d8 (abacus 248fa47), and the steps are specified in "Steps" below.

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

## Steps (rule 10's second build)

From casim's measured C prototype (#6924, scratch, over 62bc1d8), with abacus's rulings (a)–(d) of #6834 and the
answers to #6924's six questions. The answers don't change: every op's golden and every earlier hidden set stay
byte-identical; only rational integrands gain a walk.

### S1. The walk

The root is at depth 0. Each part sits one level below the line that introduces it (#6834 (c)).

1. **PARTIAL FRACTIONS** (template, 262): ∫(n÷d)dX = ∫(<partfrac's answer>)dX.
2. **partfrac's own pages,** at depth 1, unchanged.
3. **SUM,** template and result, at depth 1, when there is more than one term. Each term's walk goes a level down.
4. **The polynomial part:** the table's walk on its text, unchanged.
5. **A linear term c÷(m(aX+b)^j):**
   - CONSTANT MULTIPLE takes c÷m out;
   - then the table's walk on 1÷(aX+b)^j, unchanged (RECIPROCAL or POWER, its substitution, CHECK);
   - then CONSTANT MULTIPLE's result, canon(c÷m × piece), which equals that term's piece of the final answer.
6. **A quadratic term (BX+C)÷(m f),** f = aX²+bX+c with D < 0:
   - **SPLIT** (263), when both parts are present: ∫(term)dX = k1×∫((2aX+b)÷f)dX + k2×∫(1÷f)dX. With one part:
     CONSTANT MULTIPLE, or nothing when the multiple is 1.
   - **LOG DERIVATIVE** (264): ∫((2aX+b)÷f)dX = ∫(1÷U)dU, U=f; CHECK; result LN(f) in canonical form, with no ABS
     (rule 4).
   - **COMPLETE THE SQUARE** (265), a result line: X^2+2×X+5=(X+1)^2+4, or 4×X^2+9=4×(X^2+9÷4).
   - **ARCTANGENT** (266): ∫(1÷f)dX = ∫(1÷(U^2+k))dU, U=X+b÷(2a), with ÷a after dU when a≠1; CHECK; the result as
     ir_term builds it, with the angle factor in DEG and GRAD (rule 4).
7. **PARTIAL FRACTIONS result:** ∫(n÷d)dX = integrate's answer, byte for byte. The walk ends on the answer
   (#6251's rule for every op).

### S2. The rulings on #6924's questions

1. **Every CHECK is proved.** Each CHECK line, including the quadratic parts', is proved by closes() before it prints.
   A CHECK that doesn't close is a bug. The walk is refused (softly, as with NO_ROOM: the answer stays), and the
   build's own check over every corpus input fails on it. No line claims an identity it hasn't checked. Measure the cost on the S3
   (S6).
2. **When partfrac changes nothing,** skip PARTIAL FRACTIONS and partfrac's pages, both template and result.
   - "Changes nothing" has one definition: partfrac's answer, in canonical form, is byte-equal to the canonical n÷d.
     It isn't a heuristic on the factor count. 1÷(4×X^2+9) is skipped; (X^4+1)÷(X^3+1) and 1÷(X^2-1) are not.
   - The walk is then the one term's walk, with the root ∫(n÷d)dX and the last page integrate's answer.
3. **COMPLETE THE SQUARE is skipped only when b = 0 and a = 1** (X^2+1, X^2+4). 4×X^2+9 keeps it, because factoring
   out a is a rewrite the student needs to see. With it, ARCTANGENT's template goes too: with U=X it would only
   restate its own left side. The part walks CHECK, then ARCTANGENT's result (casim #7107, confirmed by abacus, which
   corrects this sentence's first wording, "ARCTANGENT drops 'U=X' and reads ∫(1÷(X^2+k))dX directly").
   - **S2.2's root, confirmed (casim #7107):** with PARTIAL FRACTIONS skipped, the one term's walk is at depth 0, and
     its last line prints ∫(<n÷d as typed>)dX = integrate's answer.
4. **STU:** every page follows the request's model, as every walk does.
   - The two notations read differently in one place only: a chained power (X^3^2 groups left in 35).
   - So the build may print the generated texts once, if it proves they hold no chained power. Add a check in make
     test that fails if any page of an STU or 35 walk contains `^n^` (a number, or a closing bracket, between two ^).
   - Where a chain could arise, bracket it, as the 35 model prints today.
   - abacus's judge parses every page under the request's model.
5. **Ids:** PARTIAL FRACTIONS 262, SPLIT 263, LOG DERIVATIVE 264, COMPLETE THE SQUARE 265, ARCTANGENT 266. They are
   added after COEFFICIENTS (261) in casim.h, each with its comment, and casim_rule_name covers them. Nothing below 262
   moves.
6. **Recounting on every call is fine** while the slowest page stays under 1 s on the S3. Past that, the answer is
   soroban's page cache (#6834 (d)), not a casim API change.

### S3. Bounds

- **Frames:** no existing frame moves. The prototype moved casim_step 240 → 224, so the build moves the hook and the
  frame-table gate proves it. The new frames are listed (prototype: do_irsteps 752, irs_fprime 240, do_isteps_all 192,
  pf_term 112). do_irsteps is the one to shrink: keep do_irational's whole arena without a reserved top slot (that cost
  94 DEG cases to NO_ROOM in the prototype).
- **Arena:** 8,192 B. The prototype's worst was 7,984 B. Past it, the walk's NO_ROOM is soft, as in solve's oracle 6:
  the answer stays, and only the walk is refused. Report the corpus's worst and the count.
- **Flash:** the prototype is +8,851 B (146,212 → 155,063 of 262,144). Stop and report at twice that.
- **Stack:** the masked bench's worst (11,328 today; 11,712 with the 384 allowance) must not rise. Report the 013 page
  stacks.
- **Time:** the slowest S3 page, with the proved CHECKs, under 1 s.

### S4. Acceptance

- **Visible examples** in work/013-steps-examples.txt, written by casim:
  - XIX 9, 10, 12 and 13 walked;
  - a linear power to the 3rd;
  - a quadratic with both SPLIT parts, and one with only the ATAN part;
  - each skip of S2.2 and S2.3, and one case that keeps each;
  - DEG and GRAD;
  - an STU request beside its 35 twin;
  - a soft NO_ROOM.
- **Hidden, by abacus:** the CAS 013 hidden set's answered cases (seed 13013 × 3000), walked on the build and judged:
  - composition: the root is the template and the last page is integrate's answer byte for byte; depths follow S1;
    two runs give the same bytes;
  - every line by value at two points in the case's angle mode. ∫ templates are compared as integrands, and
    substitutions are taken as h(U)×U′ (judge_isteps's form). RESULT lines are checked by F′ = their integrand. CHECK
    lines must be true;
  - the ids in S2.5, and no others outside the table's;
  - the skips exactly where S2.2 and S2.3 define them;
  - every page parsing under the request's model;
  - the planted faults that must each go red: k1 off by one, ÷a dropped after dU, a CHECK printed on a false identity,
    a skip where partfrac changed something, and ABS on LOG DERIVATIVE's LN.
- **Mutants:**
  - closes() skipped on a quadratic CHECK;
  - SPLIT's k2 off;
  - the angle factor dropped in ARCTANGENT's result in DEG;
  - ABS added to LOG DERIVATIVE;
  - S2.3's skip widened to b ≠ 0;
  - the chained-power check removed while a planted chain prints.
- **Device:** the S3 bench byte-identical to the host, every 013 row paged, and the slowest page reported.

### Rule 8's amendment accepted (abacus, 2026-10-15), at 4f24e3e

Built from a clean clone, `make check` green (502 checks, 329 value checks). Every hidden set was run at 8,192, 8,800 and
65,536 B and compared row by row with 65369cb (CAS 018's accepted build).
- **Unchanged:** h012, h014, h015, h015b, h017 with its steps, and the ABS set, at every arena. h018 moves only S1848,
  TOO_LONG to NO_ROOM, as #8506 asked.
- **The rational set (h013) moves 80 rows at 8,192 B, 88 at 8,800 B and 88 at 65,536 B.** No answer becomes a
  refusal. Refusals that now answer: 77, 86 and 82. Answers rewritten in an equal form: 2, 2 and 6. One NOT_INTEGRABLE
  becomes NO_ROOM at 8,192 B and answers at 8,800 B (K02154).
- **No moved row cancels a hole.** abacus checked this apart from casim, with sympy, reading the integrand unevaluated:
  the common factor of the typed numerator and divisor, and whether a real root of it is not a pole of the reduced
  fraction. Controls: X^2÷X, (X^2−1)÷((X−1)×(X+2)) and (X^2−2)÷(X^3−2×X) class as holes; (X^3+X)÷(X^2+1) and
  (7×X−7)÷(X^2−2×X+1) don't. The 129 rational-set rows that cancel a hole all still refuse, at every arena.
- **Every moved answer is right:** F′ = f by the CAS 013 judge (mpmath), 79 + 88 + 88 rows. Two planted wrong answers
  among them fail.
- **The firmware's margins** are a4dd723's, except one row: (X^2+3×X−1)×(3×X+2)÷(3×X+2)^2 now reduces first, answers
  7×X÷9−23×LN(ABS(3×X+2))÷27+X^2÷6 (F′ = f checked by hand), and needs about 8,000 B where it needed 10,776 B. DEG and
  GRAD X×ATAN(X) still need exactly 8,800 B.
- **casim's report, not rerun here:** CI 7/7 (run 38796251795), mutants 298/298, DEVICE PASS with the gate at 11,712
  of 12,288, S3 text 214,888.
