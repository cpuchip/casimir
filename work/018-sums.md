# CAS work unit 018: sums

**From:** casim · **To:** abacus · **Status:** ruled (abacus #8247: go; rulings folded in below) (#8051: casim writes
the unit, abacus writes the hidden set and the Accepted section).
- It's the next item on v1.0's list after CAS 017.
- Measured on a throwaway prototype over 3c3b6fb's library (scratch), before this file was written.
- The budget is decision 80's: 262,144 B for v1.0, with 193,533 used.

## Why

casim has no sum. A calculus or discrete-math course asks for closed forms of three kinds, and each is a page of
hand work:
- **Polynomials:** Σ K, Σ K^2, Σ K^3 (Faulhaber), Σ (2×K−1).
- **Geometric, and polynomial times geometric:** Σ 2^K, Σ (1÷2)^K to ∞, Σ K×2^K, Σ K÷2^K to ∞, Σ EXP(K).
- **Telescoping rational functions:** Σ 1÷(K×(K+1)), Σ 1÷(K^2−1), Σ 1÷(K×(K+2)), Σ 1÷(K×(K+1)×(K+2)),
  Σ 1÷((2×K−1)×(2×K+1)), to N and to ∞.

The prototype answers all 18 rows of that list at the calculator's 8,192 B arena. The deepest needs 3,408 B beyond
partfrac's own work area. Σ 1÷K and Σ 1÷K^2 are refused, as rule 6 says.

## Rules

1. **The call.** `casim_sum(eqn, var, lo, hi, model, angle, out, cap, arena, arena_cap)` gives Σ eqn for var = lo, lo+1,
   …, hi.
   - CASIM_SUM is appended to casim_op, and casim_req carries lo in repl and hi in repl2, as defint does.
   - The CLI is `casim sum "<text>" K LO HI RAD|DEG|GRAD`.
   - **A new status, CASIM_NO_CLOSED_FORM, is appended** for a sum this unit can't close (rule 6). NOT_INTEGRABLE
     would show the wrong word. The firmware's text for it is Michael's, through a card (ruled, #8247). The bench
     gets a code for it.
2. **The bounds.**
   - lo is an integer constant, or an expression free of var. hi is the same, or "∞" (U+221E).
   - A numeric bound that isn't a whole number (1.5, π) is BAD_ARG, and so are −∞ and an hi holding var.
   - **Both bounds integers with hi < lo:** the sum is empty, and the answer is 0. At hi = lo − 1 the closed form gives
     0 by itself; below that it would give −Σ over hi+1..lo−1, which isn't the textbook's meaning.
   - A symbolic bound is a letter the answer may hold. A sum with a rational term (rule 5) and a symbolic lo is
     NO_CLOSED_FORM: its answer would divide by lo, which the input doesn't (integrate's divisor rule, CAS 017 rule 2).
3. **The input.** The summand's canonical form, expanded (canon keeps (q(K))×2^K as one product), term by term, in
   three kinds.
   - **Polynomial times geometric:** coef × var^j × base^(a×var+b), where coef and base are free of var, j ≤ 12 and a
     and b are rational; B is base^a. EXP(a×var+b) is the same, with B = EXP(a). A base that isn't a positive constant
     ((−2)^K, A^K) needs a and b whole, since (−2)^(K÷2) has no value at odd K: so Σ 2^(K÷2) is a geometric sum in
     B = SQRT(2) (abacus's point, #8247), and Σ (−2)^(K÷2) is NO_CLOSED_FORM. Terms with the same B are grouped, with up to 4 groups.
     A term free of var is a polynomial term with j = 0.
   - **Rational in var:** every other term is written as one n÷d and goes to partfrac (CAS 012) as text, first, on the
     whole arena. partfrac's work area takes 7,328 B for any input, so the sum's own nodes are made after it, in what
     is left. Up to 6 such terms. partfrac's polynomial part joins the first kind.
   - **Anything else** (SIN(K), K!, LN(K), 1÷SQRT(K)) is NO_CLOSED_FORM, unless rule 7 sums it.
   - **A typed hole canon would lose** (Σ K÷K, Σ K^2÷K) is NO_CLOSED_FORM, by CAS 017 rule 2's divisor and domain
     rules on the typed text, as integrate refuses them. Two differences: a constant base of 0 or less to a power of
     var is allowed ((−1)^K has a value at every whole K), and a positive constant to a power is never 0, as EXP isn't
     (K÷2^K). Rule 7's sums find a hole term by term. A hole canon keeps but partfrac cancels is one of partfrac's
     removed values ("K=0" for K÷(K^3+3×K^2+2×K)): in the range, UNDEFINED, as a pole is (rule 5).
4. **Polynomial times geometric, by an antidifference.**
   - **B = 1 (a polynomial):** F with F(k+1) − F(k) = q(k) is solved top down from q's leading coefficient (Faulhaber).
   - **B ≠ 1:** F(k)×B^k with B×F(k+1) − F(k) = q(k), F of q's degree, solved top down, each step dividing by B − 1.
   - **The check:** F(k+1) − F(k) − q (or B×F(k+1) − F(k) − q) expands to exactly 0. A rational B is solved and
     checked as it is. Any other B (EXP(1), SQRT(2)) is solved as W + 1, W a fresh letter, so the check's divisions
     are powers of W, which expand merges (by EXP(1) − 1 they don't); only then is W replaced by B − 1. (As built: the
     unit's first text multiplied the check by W^(d+1), which the merge makes unneeded; its mutant survived, and it
     went.) No input reaches the check as built, since the antidifference is solved exactly: tests/smcheck_check.c
     spoils F before it, and every sum must then refuse.
   - **A letter in B** was refused in the first text (the answer divides by B − 1, which could be 0). Michael ruled
     to answer it with its condition: rule 4b. B − 1 must otherwise be decidably nonzero: i^K, and a B that is 1 but
     not written so (SQRT(2)^K×SQRT(0.5)^K), are NO_CLOSED_FORM (the cold review's case: it divided by an exact 0).
   - The sum is F(hi+1) − F(lo), or with B, F(hi+1)×B^(hi+1) − F(lo)×B^lo.
5. **Rational, by telescoping.**
   - Each partial fraction must be c÷(a×var+b)^m with a and b rational; it is written as (c÷a^m)÷(var+s)^m.
   - The fractions are classed by (s mod 1, m). In each class the coefficients must sum to exactly 0, or the class
     doesn't telescope: NO_CLOSED_FORM. Shifts within a class are at most 12 apart.
   - A class telescopes to Σ over its fractions of c × Σ_{j<d} [1÷(hi+1+s0+j)^m − 1÷(lo+s0+j)^m], with s0 the class's
     lowest shift and d each fraction's distance from it.
   - **Poles:** a fraction's pole is var = −s. When −s is an integer in [lo, hi] (from lo up, for a symbolic or ∞ hi),
     the sum has no value: UNDEFINED. Σ 1÷(K×(K+1)) from 0 is UNDEFINED; from 1 it is 1 − 1÷(N+1).
   - **With a symbolic hi, this is the safe side, chosen:** Σ 1÷(K−5) from 1 to N is UNDEFINED, though it has a value
     for N < 5. The answer would be wrong for every N ≥ 5, and a formula can't carry the condition.
6. **Named limits (NO_CLOSED_FORM), each with a visible example:**
   - **Classes that don't telescope:** Σ 1÷K (the harmonic numbers) and Σ 1÷K^2 (ζ(2) at ∞). These need functions
     casim doesn't have, or ζ(2m) = rational × π^(2m) at ∞ alone, which is out of this build (abacus #8247).
   - **i in a ratio, or a ratio that is 1 but not written so** (rule 4). EXP(X×K), whose exponent's coefficient
     isn't a number (EXP(X)^K answers, with its row).
   - **Every other function of var** (SIN(K), LN(K)), unless rule 7 sums it.
   - **A negative or symbolic base to a non-whole multiple of var:** Σ (−2)^(K÷2) (rule 3).
7. **Integer bounds, summed term by term.** When both bounds are integers at most 64 terms apart, the terms are added
   exactly first, as canon makes them, each from the typed text with var replaced; when that sum doesn't fit out
   (past 191 bytes), rules 4 and 5's closed form, with rule 4b's rows, and TOO_LONG when there is none (abacus #8320
   (c): Σ X^K from 1 to 3 is X+X^2+X^3, true for every X, with no row). A telescoping closed form isn't used for a
   range shorter than its shifts (the next point): there it stays TOO_LONG. The terms: Σ 1÷K from 1 to 10 is 7381÷2520, Σ SIN(K)
   from 1 to 3 is SIN(1)+SIN(2)+SIN(3), in the request's angle mode (DEG's SIN(30) is 0.5). A term with no value is
   UNDEFINED. Further apart, rules 4 and 5, or NO_CLOSED_FORM.
   - **Why always, and not only when rules 4 and 5 refuse** (found building): over a range shorter than its shifts, a
     telescoped form can name a fraction at a pole outside the range. Σ 1÷K − 1÷(K+2) from −1 to −1 is −2, but its
     telescoped form holds 1÷0. Past 12 terms, any pole the form names is in the range too.
8. **To ∞.** The sum is the limit of its partial sums, decided group by group, never by evaluating partial sums.
   - **A telescoping class** converges; its hi part goes to 0.
   - **A geometric group** converges when |B| < 1, decided by the evaluator beyond 1E-20 (const_sign, CAS 009 rule 5).
     B = EXP(−1) converges. A |B| it can't decide is UNDECIDED.
   - **A polynomial group** with any nonzero coefficient diverges, and so does a geometric group with |B| ≥ 1.
   - **When every group converges**, the answer is lo's part alone: Σ (1÷2)^K from 0 is 2, Σ K÷2^K from 1 is 2,
     Σ 1÷(K×(K+1)) from 1 is 1.
   - **When one diverges**, the dominant group decides: the largest |B|, then the fastest growth. A polynomial's
     partial sums grow a degree faster than its terms, and a swing's (B = −1) don't: so at |B| = 1 a polynomial of
     degree d grows as N^(d+1), against N^d for (−1)^K×K^d, and equal growth is UNDECIDED (3+(−1)^K×K: its sign
     depends on the coefficients). The cold review found the first text's "then the highest degree" wrong here.
     - B > 0 (a polynomial is B = 1): ∞ or −∞ by the sign of its leading coefficient (Σ K is ∞, Σ −2^K is −∞), as
       limit prints them.
     - B < 0: UNDEFINED, since the partial sums swing with growing size and have no limit (Σ (−2)^K, Σ (−1)^K×K).
     - Σ (−1)^K alone has partial sums 1, 0, 1, …: UNDEFINED, as B = −1 is a dominant group with B < 0.
     - A tie between B and −B, or a leading sign the evaluator can't decide (a letter, as in Σ A×K): UNDECIDED. The
       sign is asked before a swing is UNDEFINED: a coefficient that may be 0 ((SQRT(6)−SQRT(2)×SQRT(3))×(−1)^K) can't
       swing.
   - **Ruled (abacus #8247):** ∞ and −∞ as limit prints them, UNDEFINED where the partial sums oscillate (limit's word
     for "no limit"), UNDECIDED on a tie or a sign it can't decide. limit's own NO_ROOM on −2^(N+1)+N×2^(N+1)+2 at
     N → ∞ is why this doesn't call limit. ζ(2m) is out of this build: Σ 1÷K^2 to ∞ is NO_CLOSED_FORM.
9. **The answer.**
   - The canonical form of the sum of the parts, with the polynomial and geometric parts expanded (as integrate's
     answers are), and each telescoped fraction left as c÷(hi+1+s)^m. So Σ (2×K+1)÷(K^2×(K+1)^2) from 1 to N is
     1−1÷(N+1)^2, not the prototype's −0.5÷(N+0.5×N^2+0.5)+1.
   - With numeric bounds the answer is a number, or exact constants (EXP(1), π): Σ K from 1 to 100 is 5050.
   - Σ K from 1 to N is 0.5×N+0.5×N^2. The factored N×(N+1)÷2 is FACTOR's job, not this one's.
   - The bound's letter: the answer is in hi's own letter, or lo's. A summand holding hi's letter is a constant
     there, as any other letter is (Σ N from 1 to N is N^2).
   - **Statuses:** OK; NO_CLOSED_FORM (rules 6, 7); UNDEFINED (a pole in the range, an oscillating divergent sum, or a
     summand undefined for every var, CAS 016 rule 1); UNDECIDED (rule 8); BAD_ARG (rule 2); NO_ROOM (the arena or
     the depth guard); TOO_LONG past cap.
10. **Steps.** Not in this build: casim_step on CASIM_SUM is BAD_ARG, as for every op without steps. A steps build
    (the groups, each antidifference, the telescoping class, and the check) can follow, as CAS 013's and 017's did, if
    abacus or Michael wants it.
11. **Bounds, decision 68 with decision 80's budget.**
    - **Frames:** no existing frame moves (partfrac is called through its existing entry, as CAS 013 does). The new
      frames are reported from the S3; the sum's worst stack through the keys is measured on the bench.
    - **Arena:** the list above needs at most 3,408 B beyond partfrac's work area (7,328 B), so all of it answers at
      8,192 B. Past the arena: NO_ROOM, never an overrun. As built, at 8,192 B: polynomials to degree 8 (K^12 needs
      10,352 B), a polynomial times a rational ratio to degree 6 (K^6×2^K needs 7,160 B); an irrational ratio costs
      more (W). Each group's work is given back before the next (compact).
    - **Flash:** the prototype is +13.5 KB of text on the host, with its main, a stdio trace and inlining still in it.
      The estimate is 8 to 10 KB on the S3, with the stop at 20. That leaves about 58 KB for complex roots and the
      series engine.
    - **Time:** the S3's worst on the bench.

## Rule 4b: a letter in the ratio, answered with its condition (Michael, 01:07Z via abacus #8293; shape ruled #8320)

Michael's ruling on the sums-letter card: "Answer it, with 'X≠1' shown under the answer", and "I'd like to spend the
extra time here to make it good." So Σ X^K is no longer refused. This is the first casim answer that holds only under a
condition, so the shape is written here first, for abacus to rule on before it is built.

1. **Where the condition goes: CANCEL's rows.** out is the answer, then each condition as a row after "; ", as CANCEL
   and PARTFRAC already return the values a cancelling removed ("K+1; K=1"). The status stays OK. The firmware shows
   the rows under the answer, as it would CANCEL's (soroban's side). TOO_LONG when the answer and its rows pass 191
   bytes, as for CANCEL. The CLI prints out as it is.
   - `casim sum "X^K" K 0 N RAD` prints `-1÷(X-1)+X^(N+1)÷(X-1); X≠1` (expanded, as rule 9 says).
   - The rows' glyphs: ≠ (U+2260) and <. If the firmware's font lacks ≠, soroban says, and the row is written
     another way.
2. **Which conditions, and how each is written.** A condition says where the answer's formula fails, which is where a
   group's ratio B is 1 (its antidifference divides by (B−1)^(d+1)). One row per value, solved for the letter where
   that is plain:
   - B is k×L^m, a rational k and a letter's whole power, m ≠ 0 (X^K, X^(2×K), X^(−K), (2×X)^K, (−X)^K): a row for each
     real root of k×L^m = 1, ascending. X≠1, and X≠-1 too when m is even; (−X)^K is X≠-1 (abacus's minor point: no
     "-X≠1"), (2×X)^K is X≠0.5, and (−2×X^2)^K, which is never 1, has none. X^(−K) has the same rows as X^K; no X≠0
     row, since the summand has no value at 0 already (abacus (b)).
   - Any other B holding a letter (A×X, 2^X, EXP(X), X÷Y): one row "B≠1" with B as canon prints it (2×X≠1, EXP(X)≠1).
     Correct, if less plain; solving B = 1 in general is solve's job, not this one's.
   - K×X^K's divisor is (X−1)^2: the same row, X≠1. The row is about where the answer has no value, not the
     divisor's multiplicity.
   - **L≠0 where the answer itself has no value at 0** (rule 4b's cold review): X^(a×K+b) with b < 0 puts X^b in a
     coefficient, and the answer can hold X÷(X−X^2), which canon doesn't cancel, where the sum is 1. So for each letter
     L of a ratio, L = 0 is put in the answer, and where it has no value there the row L≠0 is added (asked of the
     answer: X^(K−1) from 1 merges to X^N and gets none). X^(−K) from a lo of 1 or more has no value at 0 anyway, and
     gets none (abacus (b)). EXP(X)^K gets X≠0 beside EXP(X)≠1: the same value, the plainer row first.
   - Several letter groups: each one's rows, in the order of their letters, a value once (X^K+Y^K: X≠1; Y≠1).
   - A ratio whose B − 1 the evaluator proves nonzero (no letter) gets no row, as today. A ratio that is 1 but not
     written so (SQRT(2)^K×SQRT(0.5)^K) stays NO_CLOSED_FORM: it has no letter to state a condition on.
3. **To ∞.** A letter group is answered on condition that it converges: the row is ABS(B)<1, solved for k×L^m as
   ABS(L)<|k|^(−1/m) when m > 0, and ABS(L)>|k|^(−1/m) when m < 0 (abacus (b): X^(−K) converges for |X| > 1). It
   replaces X≠1, which it implies. Σ X^K from 0 to ∞ is "-1÷(X-1); ABS(X)<1"; (X÷2)^K is "-2÷(X-2); ABS(X)<2".
   - Groups without a letter are rule 8's. When one diverges to ∞ or −∞, that is the answer, with the letter group's
     row: Σ X^K+2^K is "∞; ABS(X)<1", true where the row holds (abacus (a)). When it swings, UNDECIDED: X^K+(−2)^K has
     no limit for |X| < 1 but goes to ∞ at X = 2, and UNDEFINED carries no row. A pole in the range is UNDEFINED still
     (the cold review: the swing's UNDECIDED once caught it). Beside another letter group, both
     rows.
4. **X = 1's own value: excluded, not shown.** Proposed, for abacus's and Michael's word:
   - Each row then means one thing (a value the formula excludes), as CANCEL's do, and the firmware shows one kind of
     row. A second kind ("X=1: N+1") would be a second answer in the 191-byte slot, and for K^3×X^K it is a degree-4
     polynomial on its own.
   - The student gets X = 1's value with one more sum: `casim sum "1" K 0 N RAD` is N+1. The rows tell them which
     value to ask about.
   - If Michael wants it shown, the shape is a row "X=1: <the sum there>" after the ≠ rows, computed by this unit's own
     polynomial path with X replaced by 1, and TOO_LONG past 191 bytes as a whole.
5. **What it changed, as built.** Whole bounds at most 64 apart are summed term by term first (rule 7, abacus (c)).
   The judges below are built: sum-values.py's letter family (k×X^m with k from ±1, 2, 1÷2, −1÷3 and m from ±1, 2, −2,
   3) checks the answer at seeded X off the excluded values against the term-by-term sum, and at ∞ inside the region
   against a sum run until its terms vanish, and checks every row against the true roots or radius. 10 mutants.
6. **What it changes (the shape as sent).** sum-values.py gains a letter family judged at seeded values of the letter (excluded values
   skipped; at ∞ only inside ABS(B)<1), and its rows parsed. h018's judge must read rows. Golden: none. The two rules
   that refused letters (rule 4's letter refusal, rule 6's list entry) are amended when this is built.
7. **Not changed:** a letter in a coefficient (A×2^K) already answers with no row, since it divides by nothing. A
   letter in lo or hi is as before. The UNDECIDED of rule 8 for a leading sign holding a letter (Σ A×K to ∞) stays:
   a sign isn't a condition this rule states.

## Acceptance

- **Visible examples** (work/018-examples.txt):
  - each row of Why's list to N and, where it converges, to ∞;
  - each bound case of rule 2: an empty sum, hi = lo − 1, a symbolic lo with a polynomial, and the same with a rational
    term (refused);
  - each named limit of rule 6, rule 7's direct sums and their 64-term edge;
  - each status of rule 9, including a pole in the range and each divergent case of rule 8.
- **The oracle** (scripts/sum-values.py, in make check), by a different method from casim's:
  - every finite answer is evaluated at N = 0 … 30 and compared exactly (Python's fractions) with the sum added term
    by term from the summand;
  - every ∞ answer is compared, at 60 digits, with the verified finite form at N = 10^30 for telescoping and
    geometric families, to 25 digits;
  - the seeded families: polynomials to degree 12; q(k)×B^k with B among 2, 1÷2, −1÷3, 3÷2 and EXP(±1); telescoping
    sums built as Σ c_i÷(k+s_i)^m with Σ c_i = 0 in each class, partfrac'd form and multiplied-out form both; and
    non-telescoping rational functions, which must refuse;
  - planted wrong answers (a coefficient off by one, the lo part's sign flipped, one telescoped term dropped) must be
    refused by the judge first.
- **Golden:** no existing row moves. The CLI's usage text gains one line.
- **Mutants** (about 12): the antidifference's check skipped (polynomial; geometric); the W multiplier's power off by
  one; the telescoping class's zero-sum test skipped; a pole in the range not refused; the convergence test as
  |B| ≤ 1; the dominant group by degree before |B|; the divergent sign flipped; B < 0 divergent as ∞; the letter rule
  off; the empty sum's 0 dropped; rule 7's 64 as 65.
- **The device:** no existing frame moves, the S3 bench is byte-identical, and every earlier hidden set is rerun.
- **Size:** about a day of host work. Stop and report at twice that.
