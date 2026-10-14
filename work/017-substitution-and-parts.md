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
2. **The input, and CAS 006's guarantee.** The path works on the integrand's canonical form, term by term.
   - integrate never prints an antiderivative defined where the integrand isn't. A canonical form can lose a typed
     hole (X^2÷X is X), so before anything runs, the **divisor rule** applies to every divisor in the typed text that
     holds X (÷'s right side, INV's argument, a base to a negative power, and divisors nested inside these). Each
     factor of its canonical form that holds X must still be a denominator somewhere in the canonical integrand.
     EXP and ALOG factors are exempt, because they are never 0.
     - **Refused (NOT_INTEGRABLE):** X^2÷X×EXP(X), SIN(X÷X×X), X÷(1000÷(SINH(π)×X)).
     - **Answered:** LN(X)÷X, 1÷(X×LN(X)), EXP(SQRT(X))÷SQRT(X), and a sum holding 3÷X beside X÷SQRT(X), whose answer
       has no value at 0 either.
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
     the rational path would have written LN(X^2+1).
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
  - The prototype moves 28: 25 to answers, 2 SIN(X)+1÷(X^2-1) rows at 8,192 to NO_ROOM, and 1 to UNDEFINED.
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
    deep in the search kept.
  - Substitution's check is reached by no input as built: G(u)×u′ rebuilds the term exactly whenever u was replaced
    exactly. So tests/ibcheck_check.c (make check) spoils G before it is integrated, and every substitution must then
    refuse or answer as built, never print the spoiled answer. This is CAS 013's ircheck_check pattern.
  - The top-level rational retry, skipped because the wrapper already ran it, saves time only once NO_ROOM from the
    search is a refusal, so it has no mutant.
- **The device:** no existing frame moves, the S3 bench is byte-identical, and every earlier hidden set is rerun.
- **Size:** about a day of host work for the answers, and half a day for the steps build. Stop and report at twice
  that.
