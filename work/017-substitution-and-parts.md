# CAS work unit 017: integration by substitution and by parts

**From:** casim · **To:** abacus · **Status:** ready (abacus #8051: casim writes the unit, abacus writes the hidden set
and the Accepted section).
- It's the next item on v1.0's list after CAS 016.
- Measured on a throwaway prototype over d68e200 (scratch), before this file was written.
- The budget is decision 80's: 262,144 B for v1.0, with 173,367 used.

## Why

Today integrate answers the table's forms and rational functions, and nothing else. Every one of these is
NOT_INTEGRABLE at d68e200:
- **Substitution:** X×EXP(X^2), SIN(X)^2×COS(X), LN(X)÷X, 1÷(X×LN(X)), X×SQRT(X^2+1), X^2×(X^3+1)^5,
  EXP(X)÷(1+EXP(X)), ATAN(X)÷(1+X^2), X÷SQRT(1-X^2).
- **By parts:** X×EXP(X), X×SIN(X), X^2×COS(3×X), LN(X), X×LN(X), LN(X)^2, ATAN(X), ASIN(X), EXP(X)×SIN(X).

These are the two methods every calculus course teaches after the table. The prototype answers 32 of a 38-integral
textbook list in each angle mode, within 3,520 B of arena. The six it refuses are named in rule 7.

## Rules

1. **When.** integrate's table runs first, then CAS 013's rational path, both as today. Only when both refuse
   (NOT_INTEGRABLE) does this path run.
   - The hook sits in casim_integrate's wrapper, as 013's does, so do_integrate's and do_irational's frames don't move.
   - Every answer the table or the rational path gives today is unchanged.
   - **Inside this path, a constant factor comes out before the rational path runs** (added after abacus's h017,
     #8121): the rational path takes rational coefficients only, so π, LN(2), another letter, or DEG's and GRAD's
     180÷π and 200÷π in ATAN's u′ refused it. The factors free of X that aren't numbers come out, the rest goes
     through the rational path, and the constant goes back on each term of its answer (X^2÷(X^2+1)÷π is
     X÷π−ATAN(X)÷π; ATAN(X+1) in DEG is ATAN(X+1)+X×ATAN(X+1)−90×LN(2×X+X^2+2)÷π). The integrand itself goes
     through this too, since the wrapper's rational path refused it for the constant.
2. **The input, and CAS 006's guarantee.** The path works on the integrand's canonical form, term by term.
   - integrate never prints an antiderivative defined where the integrand isn't. A canonical form can lose a typed
     hole (X^2÷X is X), so before anything runs, the **divisor rule** applies to every divisor in the typed text that
     holds X (÷'s right side, INV's argument, a base to a negative power, and divisors nested inside these). Each
     factor holding X of 1÷d's canonical form must still be a denominator somewhere in the canonical integrand.
     EXP and ALOG factors are exempt, because they are never 0. The bases come from 1÷d, not d, because a sum
     there carries no constant factor, as in the integrand: 3×X^2+3 alone is canonical, but 3×(X^2+1) is how it
     divides (abacus's h017, #8121, found 33 refusals of this shape).
     - **Refused (NOT_INTEGRABLE):** X^2÷X×EXP(X), SIN(X÷X×X), X÷(1000÷(SINH(π)×X)).
     - **Answered:** LN(X)÷X, 1÷(X×LN(X)), EXP(SQRT(X))÷SQRT(X), and a sum holding 3÷X beside X÷SQRT(X), whose answer
       has no value at 0 either; EXP(ATAN(X))÷(3×X^2+3), ATAN(X)^3÷(2×(X^2+1)) and 15×X^2÷(5×X^3+2), whose
       constants are typed in the divisor.
   - A domain edge is not a hole: 1÷SQRT(X) is 2×SQRT(X) today, and EXP(SQRT(X))÷SQRT(X) is 2×EXP(SQRT(X)) here.
   - **The domain rule** (added after the cold review): canon also drops domains without a division. So every typed
     LN, LOG, ASIN, ACOS, ATANH, ACOSH or TAN (for its poles), and every SQRT or power that isn't a whole number,
     whose argument or base still holds X after canon, must survive in the canonical integrand.
     - **Refused:** LN(X)−LN(X)+X, 0×LN(X)+X, EXP(LN(X)), X^2×TAN(X)^0, X^(−A)×X^A×X.
     - **Kept:** (X−X)^4.5, whose base is 0 for every X.
     - A constant base of 0 or less to a power holding X, as typed (0^X, (−2)^X), is refused: it has no value for most
       X. Canon would fold 0^X×0^(−X) to 1.
   - **What the procedure can't take** (refused before it starts, and for every piece it integrates):
     - a literal canon keeps as typed (1.2.0, a number past 2^62), whose printed copy would point into a buffer that
       goes;
     - SGN, ABS (except in LN(ABS(…)), the table's own form) and the opaque names (IP, FP) holding X, whose derivative
       derive takes as 0. A part or a check would otherwise treat them as constants: SGN(X)×EXP(X) was answered with a
       jump at 0 before the review.
     - LN(ABS(u))'s derivative SGN(u)×ABS(u)^−1 is written u^−1, as the check writes it, so LN(ABS(X)), and the
       table's own answers, can be integrated again.
   - **Other letters** are constants (CAS 006). But no answer may divide by an expression of another letter that the
     integrand doesn't already divide by, because that letter could be 0. This is checked on every substitution's G
     and on the whole answer.
     - **Refused:** EXP(A×X×Y) in Y, as the table refuses EXP(A×X). Also X×EXP(A×X^2)÷B (A), and SIN(A×X)×COS(X),
       whose cycle divides by A^2−1.
     - **Answered:** X×EXP(X^2)÷B, and EXP(X)×SIN(A×X), whose A^2+1 is never 0. A denominator that is a sum of even
       powers and products of them with a positive constant passes.
     - A hole in another letter's constant follows the table's precedent since CAS 006 (X×Y÷Y in X is 0.5×X^2): canon
       makes the constant first.
3. **Substitution** (derivative divides).
   - **The candidates for u:** each function holding X, each function's argument, and each power's base, up to 12, in
     tree order.
   - **For each candidate:** take u′. Skip it when u′ has no X, since that's the table's linear case. Form
     q = term ÷ u′, and replace u in q by a new letter U (the first of U, V, W, T, S, R, Q not in use).
     - When X is left and u = a×X^k + b, X is rewritten from U: X^(jk) becomes ((U−b)÷a)^j, as in X^3×SQRT(X^2+1).
     - When X is still left, the candidate is refused.
   - **Integrate and substitute back:** G(U) is integrated by this whole procedure in U (the table, the rational
     path, then these rules), and U is replaced by u.
   - **The check:** G is checked in U where it was integrated. The chain rule's G(u)×u′ must expand to exactly the
     term. Both hold, or the candidate is refused.
4. **By parts** (LIATE).
   - **Choosing u:** a product of two or more factors holding X, or a log or inverse function on its own, is parted
     with u = the first factor by LIATE: a log (LN, LOG, or a power of one), then an inverse function (ATAN, ASIN, ACOS,
     ASINH, ACOSH, ATANH), then algebraic, then trigonometric (SIN, COS), then exponential (EXP, ALOG, SINH, COSH).
     dv is the rest, and v = ∫dv by the whole procedure.
   - **The general step:** ∫ = u×v − ∫v×u′, with the second integral by the whole procedure. The check is that
     (u×v)′ = term + v×u′ expands to exactly 0, and ∫v×u′ is checked where it was made. (A whole-answer check can't
     close here: F′ − f leaves fractions over different denominators, as in X×ATAN(X), which expand doesn't combine.
     CAS 013 proves its answers from checked terms in the same way.)
   - **A polynomial against EXP, SIN, COS, SINH or COSH of a linear argument goes round** until u's derivative is 0:
     the sum of (−1)^j u^(j) V_(j+1), each V the table's ∫ of the one before. It's a loop, not recursion, so X^9×EXP(X)
     costs no depth (X^10×EXP(X)'s answer is past 191 bytes: TOO_LONG). The whole answer is checked by differentiating it.
   - **Two steps that come back to k×term** (EXP×SIN, EXP×COS, with k a constant not 1) give
     ∫ = (u×v − u2×v2) ÷ (1 − k). It's checked before the division as (u×v − u2×v2)′ = (1 − k)×term. That's what makes
     DEG and GRAD close, where 1 − k is 1 + π^2÷32400.
5. **Bounds on the search.**
   - Substitution and parts nest at most 4 deep. Each level counts against the depth guard, so a deep input leaves
     less room for nesting, and the stack gate holds.
   - At most 160 tries of the procedure per integrand. The prototype's worst textbook case took 59.
   - Each failed try gives its arena back.
6. **The answer.**
   - The canonical form of the sum of the terms' answers, as integrate prints every answer.
   - No "+C". Summands free of X are dropped, since an antiderivative is defined only up to one (back-substitution
     leaves them: X^2×ATAN(X)'s −1÷6).
   - **Statuses:**
     - NOT_INTEGRABLE when no rule answers.
     - NO_ROOM when a term of the input's own sum needed the rational path and it ran out (SIN(X)+1÷(X^2−1) at
       8,192 B, which answers at 64 KB). Room running out deeper in the search (the arena or the depth guard) refuses
       that try, and the search goes on.
     - **Amended after soroban's repin (abacus #8359):** when nothing answers and any try ran out (the arena or the
       depth guard), the status is NO_ROOM, never NOT_INTEGRABLE: CAN'T INTEGRATE would say no method exists where
       the calculator only ran short. X×ATAN(X) at 8,192 B is NO_ROOM (it answers from 8,592 B). A refusal that no try
       ran out on stays NOT_INTEGRABLE at every arena (EXP(X^2), SIN(X)÷X).
     - UNDEFINED when the canonical integrand holds a pole (CAS 016 rule 1: TAN(Y)×LOG(X−X)^SGN(X+7), today
       NOT_INTEGRABLE).
7. **Named limits, refused (NOT_INTEGRABLE), each with a visible example:**
   - **Powers and products of SIN, COS and TAN of one argument** (SIN(X)^2, COS(X)^2, X×COS(X)^2, TAN(X)^2) need the
     power-reduction and Pythagorean identities, which canon doesn't apply. That's a trigonometric-integrals unit, if
     wanted, and Michael's call through a card.
   - **Inverse-function forms the table doesn't hold:** 1÷SQRT(1−X^2) (ASIN), 1÷COS(X)^2 (TAN). They're new table
     entries, for the same card.
   - **At the calculator's 8,192 B arena, the rational path doesn't fit inside a substitution or a part.** partfrac's
     work area takes 7,328 B for any input. So X×ATAN(X), whose ∫v×u′ is 0.5×X^2÷(X^2+1), answers at 16 KB and is
     NOT_INTEGRABLE at 8 KB. ATAN(X) still answers at 8 KB, by substitution, as −0.5×LN(ABS(X^2+1))+X×ATAN(X), where
     the rational path would have written LN(X^2+1). DEG and GRAD are the same since the constant factor comes out
     (rule 1): ATAN(X+1) and X×ATAN(X) answer at 16 KB.
   - **defint** keeps the table alone, as CAS 013 left it. Bringing the rational path and this one into defint is its
     own follow-up.
8. **Steps.** For an integrand only this path answers, casim_step gives one page for now: the answer, with the rule
   ANSWER. The steps never refuse what integrate answers.
   - The working pages (SUBSTITUTION: u, du and ∫G(U)dU; BY PARTS: u, dv, v and ∫v du; the CHECK) are a second build
     of this unit after the answers are accepted, as CAS 013's were. Their rule ids are added at the end.
9. **Bounds, decision 68 with decision 80's budget.**
   - **Frames:** no existing frame moves. The new frames are reported from the S3, and the recursion's worst stack
     through the keys is measured on the bench (a 4-deep nest).
   - **Arena:** the textbook list needs at most 3,520 B. Past the arena, NO_ROOM or NOT_INTEGRABLE as in rule 6, never
     an overrun.
   - **Flash:** the prototype is +9.5 KB on the host, with a trace still in it. The estimate is 8 to 10 KB on the S3,
     with the stop at 20.
   - **Time:** the S3's worst on the bench.

## Acceptance

- **Visible examples** (work/017-examples.txt):
  - each rule-3 and rule-4 form above, in RAD, plus DEG and GRAD for the trig ones;
  - each rule-2 refusal and answer, and each rule-7 limit;
  - the NO_ROOM and UNDEFINED statuses of rule 6.
- **The oracle** (scripts/integrate-values.py, in make check), by a different method than closes():
  - casim's answer is differentiated numerically, a central difference at 60 digits with h = 10^-25, at seeded points
    where the integrand has a value, and must equal the integrand there to 40 digits;
  - the seeded families:
    - substitution: k×G(u)×u′, with G from EXP, SIN, COS, U^n, 1÷U, SQRT(U), ATAN(U) and u from a×X^k+b, SIN, COS,
      EXP, LN, SQRT and ATAN of X;
    - parts: polynomial × EXP/SIN/COS/SINH/COSH of aX+b, X^n×LN(aX+b), LN(X)^m, ATAN/ASIN of aX+b times X^n, and
      EXP(aX)×SIN/COS(bX);
  - and integrands with no elementary antiderivative (EXP(X^2), SIN(X)÷X, EXP(X)÷X, SQRT(SIN(X))), which must refuse;
  - planted wrong answers (a sign flipped, a factor of 2, a dropped term) must be refused by the judge first.
- **Golden:** the rows that move are refusals that now answer, plus the statuses in rule 6. Each is listed in
  tests/golden/moved/cas-017.txt with its before and after.
  - The prototype moves 28: 25 to answers, 2 SIN(X)+1÷(X^2-1) rows at 8,192 to NO_ROOM, and 1 to UNDEFINED. The
    constant factor (rule 1) moves one more to NO_ROOM at 8,192: a rational function of Y times X^4, integrated in
    Y, which answers at 16 KB. 29 in all.
  - No other row moves.
- **Mutants** (about 12):
  - substitution's check skipped;
  - parts' check skipped;
  - the cycle's 1 − k as 1 + k;
  - the loop's alternating sign dropped;
  - the divisor rule off;
  - the letter rule off;
  - back-substitution's 1÷k as k;
  - LIATE with LN last;
  - the depth bound off by one;
  - NO_ROOM from any depth;
  - steps refusing what integrate answers;
  - after the review: the can't-take refusals off, the domain rule off, the answer's letter guard off, and NO_ROOM
    deep in the search kept;
  - after abacus's h017: the divisor rule's bases taken from d instead of 1÷d, the constant factor left in the
    rational path's text, and the constant put back on the whole answer instead of term by term.
  - Substitution's check is reached by no input as built: G(u)×u′ rebuilds the term exactly whenever u was replaced
    exactly. So tests/ibcheck_check.c (make check) spoils G before it is integrated, and every substitution must then
    refuse or answer as built, never print the spoiled answer. This is CAS 013's ircheck_check pattern.
  - The top-level rational retry, skipped because the wrapper already ran it, saves time only once NO_ROOM from the
    search is a refusal, so it has no mutant.
- **The device:** no existing frame moves, the S3 bench is byte-identical, and every earlier hidden set is rerun.
- **Size:** about a day of host work for the answers, and half a day for the steps build. Stop and report at twice
  that.

## Accepted (abacus, 2026-10-14), at 279436b

**The hidden set** (abacus's gen_cas017.py, seed 17017, 2,000 integrands, written while the unit was built): SUB 766,
PARTS 388, LIMIT8 124 (X^n×ATAN(aX), which rule 7 lets refuse at 8 KB), NONEL 256, LIMIT 96, HOLE 370; DEG or GRAD
on about half the trig cases. The judge differentiates each answer numerically (mpmath at 90 digits, a central
difference with h = 1e-30, within 1e-35 at up to 8 seeded points), and requires a real value at each point with every
function and power on the way real (CAS 006's guarantee). Before any answer existed it was falsified: sympy's 233
true antiderivatives pass; −F, 2F and a dropped term fail on every case, and a stripped ABS on every case where the
LN's argument can be negative.

**Held at 2222628, on refusals only.** 0 wrong answers, but two refusals the unit didn't name: a constant in a
divisor blocked substitution (33 cases, EXP(ATAN(X))÷(3×(X^2+1))), and ATAN by parts refused in DEG and GRAD at every
arena (29 cases, ATAN(X+1), X×ATAN(X)). 279436b fixed both (rules 1 and 2 above).

**At 279436b,** built from a clean clone, `make check` green (310 checks, and 478 in integrate-values), every case
run through the CLI at 8,192 B and at 65,536 B:
- **0 wrong answers at both arenas.** HOLE 370 and NONEL 256 all refused.
- **Answers right:** 1,079 at 8 KB and 1,261 at 64 KB (2222628: 1,046 and 1,199). Against 2222628 only the held rows
  moved: the 33 answer at both arenas, the 29 answer at 64 KB, and 25 other DEG and GRAD answers changed form only,
  each judged right.
- **Refusals left:** 182 at 8 KB only, X^n×LN(aX+b), LN(aX+b), ATAN(aX+b) and X^n×ATAN(kX) in every angle mode: rule
  7's third limit, as named. At both arenas only 17, all (X^2+1)^(−3÷2) from substitution with u = ATAN(X), where
  sympy folded COS(ATAN(X)) into a form outside this unit. That is the generator's fault, and those rows count as
  LIMIT.
- **By hand:** EXP(ATAN(X))÷(3×(X^2+1)) is EXP(ATAN(X))÷3, 15×X^2÷(5×X^3+2) is LN(ABS(X^3+0.4)), ATAN(X+1) in DEG at
  64 KB is ATAN(X+1)+X×ATAN(X+1)−90×LN(2×X+X^2+2)÷π, and X^2÷(3×X)×EXP(X), X^2÷X×EXP(X) and (X−1)÷((X−1)×(X+2))
  still refuse.
- **The earlier hidden sets:** partial fractions (5,000), Taylor (2,000, at both arenas), limits (1,500 and 600) and
  CAS 016's ABS set (750, both arenas) are byte-identical to CAS 016's 2833f14. The rational-integral set (3,000)
  changes 32 rows against 2833f14:
  - 24 EITHER rows, a SIN(X) beside a rational function, go NOT_INTEGRABLE → NO_ROOM, rule 6's status at 8 KB;
  - 5 rows CAS 013's rule 8 refused now answer, (7×X−7)÷(X^2−2×X+1), (−4×X+8)÷(3×(X−2)^3) and three like them,
    and so do 3 EITHER rows: each right by F′ = f and the domain. In the 5 the cancelled factor is still a denominator,
    so there's no hole. Rule 2's divisor rule is the general form of rule 8's intent ("never an antiderivative
    defined where the integrand isn't"), and it answers these where the rational path alone refuses them.
- **The moved golden lines** (tests/golden/moved/cas-017.txt), 29, were read one by one. (3^7)^ASIN(π) now answers,
  which is the long-standing rule for a symbolic constant, and one answer prints 0^SIN(0.5) for 0; both are left as
  they are.

**The device half** (casim's report; abacus did not watch the device run): no existing frame moved, 2 new
(ib_const, ib_inv); S3 text 184,030 of 262,144 (+10,663 over 2833f14); DEVICE PASS on the XIAO, 1,170 of the bench's
1,173 cases on the chip (3 older rows carry a status the bench has no code for), the deep-nest peak 5,440 B and the
stack gate's worst 11,712 of 12,288, both unchanged. **CI** 7/7 on 279436b (run 38574304157). **Mutants** 214 of 214
caught (casim).

**Named, not fixed (form, not value):**
- substitution prints the LN of u with u's X coefficient made 1, so (7×X−7)÷(X^2−2×X+1) is
  3.5×LN(ABS(X−0.5×X^2−0.5)), where a student writes 7×LN(ABS(X−1)); and SIN(X)+(9×X−9)÷(X^2−2×X+4) prints
  LN(ABS(X−0.5×X^2−2)), an ABS that argument needs (it is negative everywhere) but that CAS 013's form,
  LN(−2×X+X^2+4), doesn't;
- ATAN(X) keeps LN(ABS(X^2+1)) at 8 KB where 64 KB drops the ABS.

## Steps (rule 8's second build)

From casim's measured C prototype (scratch, over 8671ba7), the same shape as CAS 013's: integrate's walk for an
integrand only this path answers, composed from the walks that exist, by text. The answers don't change apart from the
two forms in S2.6; every other op's golden and every earlier hidden set stay byte-identical.

### S1. The walk

The walk finds the search's own choice for the integrand (the term at depth 0): substitution, parts, the cycle, or a
constant factor out. It prints that choice and walks each piece by calling integrate's steps on the piece's printed
text, as the search integrated it. As built, the plan replays ib's order at depth 0 (the table, the rational path with
a constant factor, substitution, parts), with only substitution's u and G noted during the search: a note inside
parts moved ib's frame (240 → 256 on the host, measured), and so did a second call site for the table's or the
rational path's helpers (240 → 224). The root is at depth 0, and each piece one level below the line that
introduces it.

1. **A sum** (the table and the rational path refused it whole): SUM, template and result; each term's walk a level
   down, whichever path answers it (the table's, CAS 013's, or this one).
2. **SUBSTITUTION** (267), template: ∫(f)dX = ∫(G)dU, U=u. Then ∫(G)dU's walk, CHECK, and the result. U is printed
   as the answer's LN has it (S2.6): w = s×u in whole coefficients, with G rescaled to G(U÷s)÷s, so 15×X^2÷(5×X^3+2)
   reads ∫(1÷U)dU, U=5×X^3+2, and its piece's walk ends on LN(ABS(U)).
3. **BY PARTS** (268), template: ∫(f)dX = u×v-∫(w)dX, U=u, V=∫(dv)dX, with w = v×u'. Then the walk of ∫(dv)dX (how
   v was found), the walk of ∫(w)dX, CHECK, and the result. The polynomial loop is parts again: ∫(w)dX's own walk is
   the next round.
4. **The cycle:** BY PARTS's template and ∫(dv)dX's walk; a second BY PARTS template a level down, ∫(w)dX =
   u2×v2+(-k)×∫(f)dX, with its ∫(dv2)dX's walk; then **CYCLE** (269), a result line at depth 1: ∫(f)dX =
   (u×v-u2×v2)÷(1-k), with 1-k as a number; CHECK; the result.
5. **CONSTANT MULTIPLE** (the existing id): a constant factor that isn't a number (rule 1's ib_const, DEG's 180÷π)
   comes out, ∫(f)dX = k×∫(f÷k)dX, and the rest is walked.
6. **The last page** is integrate's answer, byte for byte, under the rule of the root's line.

### S2. Decisions

1. **Every CHECK is proven by the search's own checks** (rule 5): G checked in U and G(u)×u' = f; (uv)' = f + w with
   ∫w checked; the loop and the cycle differentiated whole. closes() can't prove some of them (fractional powers from
   back-substitution), so it isn't re-run. The CHECK reads d/dX(F) = f, with f the request's text as typed.
2. **A piece's walk is integrate's on its text,** so a piece may take another route than the search took inside
   (a smaller arena, the tries counted afresh). Every line is still true, and judged by value.
3. **Nesting:** at most 3 levels of this walk (IB_WALKS). Below that, a piece is one line, ∫(w)dX = W (ANSWER), as
   rule 8's page is today. Each nested walk is a fresh call with its own depth guard, so without a cap the stack
   grows with the nesting: X^9×EXP(X) took 14,816 B on the host uncapped, and 7,728 B with the cap.
4. **The walk never refuses what integrate answers, and never walks what it refuses** (rule 8). Integrate's own answer
   is asked first, and its refusal is the steps' (the letter rule refuses SIN(A×X)×COS(X), whose parts the plan
   would otherwise make). When a piece's walk refuses, or the texts don't fit, the walk falls back to rule 8's one
   page: a soft NO_ROOM, as CAS 013's S3 has it, but with the answer's page kept. ~~A page past cap is TOO_LONG with
   the whole count, however deep its piece.~~ Held by abacus (#8227, a14dd08): 13 walks refused at cap 192 where
   integrate answers. As built after the hold:
   - **At a page cap,** each walk measures its own lines (templates, CHECK, CYCLE, its answer line) at the cap before
     its pieces run, in the arena they will use. A line past cap makes the walk its one page. A piece's walk gets the
     cap in the flags' bits above its level (IB_LIM), so its count and its pages agree. The count depends on the cap
     for these walks only, and the count's call takes the cap its pages will use (src/casim.h). Where the one page is
     past cap too (I0837 and I0983: 197 and 203 bytes, 169 and 171 characters), it is TOO_LONG on one page, not half
     a walk.
   - **The count's room:** below cap 384 the count borrowed 384 bytes of the arena for integrate's text, and at 8 KB
     that was what LN(X+1)'s rational path (7,328 bytes) lacked: it now asks again on the whole arena.
   - A page of an inner walk that isn't this unit's (the table's, CAS 013's) is not measured. istep-values' cap check
     (150 and 192 bytes, both arenas, every family walk) found none past cap where the one page fits.
5. **Letters:** U is substitution's own (the first of U, V, W, T, S, R, Q not in the integrand). BY PARTS's two labels
   are the first two of that list free in the integrand and not its variable, so inside a piece in U they are V and W
   (LN(ATAN(X))÷(5×X^2+5)); past the list, any free letter from Z down. A piece's labels may reuse a letter of the
   walk above it (U=LN(V) under V=ATAN(U)), since each walk is integrate's on its own text.
6. **Two answer forms change (abacus's Accepted section names them):**
   - substitution's LN(ABS(u)) is written as LN(ABS(w)), w being u with whole coefficients, none in common, and its
     highest power's positive, on the answer once (not inside the search, whose arena it would take: the cold review
     measured answers lost at 1,900 to 3,100 B), and the answer as it was when the form doesn't fit. The discriminant
     is exact, and where b^2 or 4ac passes 63 bits the LN keeps its ABS (a wrapped product once dropped it: a wrong
     answer). They differ by a constant. With no ABS when w is a quadratic with no real root, or u
     is never negative, as CAS 013 writes them: (7×X−7)÷(X^2−2×X+1) is 3.5×LN(ABS(-2×X+X^2+1)),
     15×X^2÷(5×X^3+2) is LN(ABS(5×X^3+2)), SIN(X)+(9×X−9)÷(X^2−2×X+4) in DEG is
     4.5×LN(-2×X+X^2+4)-180×COS(X)÷π, and ATAN(X) at 8 KB is -0.5×LN(X^2+1)+X×ATAN(X), as at 64 KB.
   - Golden moves 11 rows, all in rows 017 added: 7 LN(ABS(X^2+1)) → LN(X^2+1) (ATAN(X), X^2×ATAN(X) at 8 KB) and 4
     LN(ABS(EXP(X)+1)) → LN(EXP(X)+1). Each is judged by value; abacus reruns h013 and h017.
   - The ABS rule, as abacus ruled (#8149, #8151): exactly CAS 013's for linear and quadratic arguments and for sums
     of terms each ≥ 0 (even powers, EXP, COSH) with one > 0; any other argument keeps its ABS (a spare one on a
     rootless argument of degree 3 or more is allowed; one is never dropped where there is a root).
7. **STU:** every text is printed under the request's model; CAS 013's S2.4 check (no `^n^` on any page) covers
   these pages too.
8. **Ids:** SUBSTITUTION 267, BY PARTS 268, CYCLE 269, added after ARCTANGENT (266), each with its comment, in
   casim_rule_name. Nothing below 267 moves.

### S3. Bounds

- **Frames:** no existing frame moves (the frame-table gate, and the S3).
- **Stack:** the masked bench's worst must not rise (11,712 with the allowance). The bench pages every 017 row,
  including the deep-nest rows. Prototype, on the host: the walk's worst is 7,728 B against integrate's 5,248
  (X^5×EXP(X), LN(X)^4 beside a 40-deep nest 7,712).
- **Arena:** 8,192 B. The prototype walks 52 of the 52 examples integrate answers at 8 KB and all 64 at 64 KB. The
  only one-page rows are two at 2,000 and 1,500 B. No last page differs from integrate's answer.
- **Time:** every page recounts the walk: 0.1 to 0.5 ms a page on the host. The slowest S3 page must stay under 1 s.
- **Flash:** the prototype's walk is +477 B of host text. The estimate is 2 to 3 KB on the S3; stop and report at 6.

### S4. Acceptance

- **Visible examples** in work/017-steps-examples.txt, written by casim: each kind (substitution with and without
  back-substitution, parts with LN, ATAN and the polynomial loop, the cycle, a constant factor, a sum), DEG and GRAD,
  an STU twin, the level cap (X^5×EXP(X)), and the two form changes.
- **Hidden, by abacus:** h017's answered cases walked and judged as CAS 013's steps were: the root is the template
  and the last page is integrate's answer byte for byte; depths follow S1; RESULT lines by F' = their integrand;
  templates as integrands, substitutions as G(U)×U'; BY PARTS lines by f = (u×v)' − w; CHECK lines true; every page
  parsing under the request's model; two runs give the same bytes.
- **Mutants:** a CHECK with a wrong F; BY PARTS's w without u'; the cycle's 1−k sign; the level cap removed (the
  bench's stack and a host stack test); the fallback removed (a walk that refuses where integrate answers); the LN
  form's sign (w's highest power negative) and its ABS dropped where u can be negative.
  - As built (scripts/mutants.py, steps-* and lnform-*, 11): those, plus the labels taken from the integrand, a
    substitution's note taken without G(u)×u' checked, U not in whole coefficients, and the rootless test widened.
    tests/ibcheck_check.c spoils G two ways: in the search (LN(X^2)'s parts must show no SUBSTITUTION, and the
    refused try leaves no note), and in the plan's replay only, since after ib_unnote no search-side plant reaches
    the plan's check (X×EXP(X^2), LN(X)÷X and two more must then be one page, integrate's answer). The full run
    caught 233 of 234 before that second plant: steps-sub-note-unchecked survived, as no input reached its check.
  - From the cold review (review-*, 7, and steps-walk-not-asked): a refused substitution's note kept (it pointed into
    freed arena), whole coefficients unbounded (int64 overflow), the cycle's divisor bare, a sum's "+" dropped,
    BAD_ARG for a page too long, the LN form inside the search (answers lost at small arenas), labels exhausted, and
    the walk shown where integrate refuses.
  - From abacus's hold (hold-*, 5): the count's retry on the whole arena removed, the lines left unmeasured, a piece
    not given the cap, a sum's answer line measured as a term, and the level read with the cap's bits.
- **Device:** the S3 bench byte-identical to the host, every 017 row paged, the slowest page and the 017 pages' worst
  stack reported.

### Accepted (abacus, 2026-10-14): the steps at 3c3b6fb

**Held at a14dd08** (#8227): 13 walks refused where integrate answers. They were 6 at 8 KB (LN(X+k), X×LN(X+k): the
count borrowed 384 bytes of the arena) and 3 at 64 KB with a BY PARTS line past cap, and I0837 printed 6 pages, then
TOO_LONG. The hold also counted I0837's and I0983's one page as fitting. That was a character count: in bytes they are
197 and 203, past the 191 a page holds, so TOO_LONG on one page is the right answer there (#8234).

**At 3c3b6fb,** built from a clean clone, `make check` green (310 checks, and 491 in integrate-values), every h017
case walked through the CLI at caps 192 and 150, at 8,192 B and at 65,536 B:
- **Answers unchanged:** every hidden set, h012 to h017 and CAS 016's ABS set, at both arenas, is byte-identical to
  a14dd08. Against 279436b only forms move (h017: 82 rows at 8 KB, 13 at 64 KB; h013: 3), 0 wrong answers, and every
  LN argument that lost its ABS is positive for every real X.
- **Every page true:** each page is parsed and differentiated exactly (sympy, abacus's judge, shaped apart from
  istep-values). At cap 192 no walk fails at either arena. One corrupted page per walk: 1,026 of 1,028 caught at 8 KB,
  1,237 of 1,239 at 64 KB; the 2 missed only move the constant (LN(ABS(2×X)) as LN(ABS(4×X))), so they are still true.
- **The rule, checked:** where integrate answers, the steps are a walk whose every page fits and whose last page is
  integrate's answer, or TOO_LONG alone when the one page itself is past cap. The check fails a14dd08 on exactly the
  held rows and passes 3c3b6fb at cap 192 at both arenas. Only those rows moved: the 6 are now the ANSWER page at 8 KB.
  At 64 KB the 3 are now the ANSWER page, and I0837 is TOO_LONG on one page.
- **The contract change is accepted:** a 017 walk's count depends on the cap, so the count's call takes the cap its
  pages use (src/casim.h). Keeping the count cap-free would refuse where integrate answers. When the firmware pages
  steps, the count and the pages take the same cap.
- **Named, not held:**
  - At cap 150, one walk prints 16 pages and then TOO_LONG: −2×X÷(2×X^2−2)^3, whose PARTIAL FRACTIONS SUM line is 174
    bytes while its one page is 96. It is the older partial-fractions walk (the unmeasured inner walk this build
    names), the same at 279436b, and whole at 192.
  - "∫(COS(U))dU÷(1÷3)" would read better as ×3. That is CAS 006's line, so it is a follow-up.

**The device half** (casim's report; abacus did not watch it): DEVICE PASS, 1,172 cases, 017's 69 rows byte-identical
to the host; the deepest 017 page stack 6,064 B and the gate's worst 11,712, both of 12,288; S3 text 193,533. **CI**
7/7 on 3c3b6fb (run 38649110352, read by abacus). **Mutants:** casim's full run was at 116 of 239, all caught, when this was written.
