# CAS work unit 018: sums

**From:** casim · **To:** abacus · **Status:** draft for abacus (#8051: casim writes the unit, abacus writes the hidden
set and the Accepted section).
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
     would show the wrong word. The firmware's text for it is Michael's, through a card.
2. **The bounds.**
   - lo is an integer constant, or an expression free of var. hi is the same, or "∞" (U+221E).
   - A numeric bound that isn't a whole number (1.5, π) is BAD_ARG, and so are −∞ and an hi holding var.
   - **Both bounds integers with hi < lo:** the sum is empty, and the answer is 0. At hi = lo − 1 the closed form gives
     0 by itself; below that it would give −Σ over hi+1..lo−1, which isn't the textbook's meaning.
   - A symbolic bound is a letter the answer may hold. A sum with a rational term (rule 5) and a symbolic lo is
     NO_CLOSED_FORM: its answer would divide by lo, which the input doesn't (integrate's divisor rule, CAS 017 rule 2).
3. **The input.** The summand's canonical form, term by term, in three kinds.
   - **Polynomial times geometric:** coef × var^j × B^(a×var+b), where coef and B are free of var, j ≤ 12 and a is
     a whole number. EXP(a×var+b) is the same, with B = EXP(a). Terms with the same B are grouped, with up to 4 groups.
     A term free of var is a polynomial term with j = 0.
   - **Rational in var:** every other term is written as one n÷d and goes to partfrac (CAS 012) as text, first, on the
     whole arena. partfrac's work area takes 7,328 B for any input, so the sum's own nodes are made after it, in what
     is left. Up to 6 such terms. partfrac's polynomial part joins the first kind.
   - **Anything else** (SIN(K), K!, LN(K), 1÷SQRT(K)) is NO_CLOSED_FORM, unless rule 7 sums it.
4. **Polynomial times geometric, by an antidifference.**
   - **B = 1 (a polynomial):** F with F(k+1) − F(k) = q(k) is solved top down from q's leading coefficient (Faulhaber).
   - **B ≠ 1:** F(k)×B^k with B×F(k+1) − F(k) = q(k), F of q's degree, solved top down, each step dividing by B − 1.
   - **The check:** F(k+1) − F(k) − q expands to exactly 0. For B ≠ 1 it is checked with B written as W + 1 (W a
     fresh letter) and multiplied through by W^(d+1), so its divisions merge, and only then is W replaced by B − 1.
   - **A letter in B is refused** (NO_CLOSED_FORM): the answer divides by B − 1, which could be 0 (CAS 017 rule 2's
     letter rule). So Σ X^K and Σ A^K are refused, and Σ A×2^K answers. The geometric series in a letter is the one
     textbook form this costs; whether to answer it with a stated condition (X ≠ 1) is Michael's call, through a card.
   - The sum is F(hi+1) − F(lo), or with B, F(hi+1)×B^(hi+1) − F(lo)×B^lo.
5. **Rational, by telescoping.**
   - Each partial fraction must be c÷(a×var+b)^m with a and b rational; it is written as (c÷a^m)÷(var+s)^m.
   - The fractions are classed by (s mod 1, m). In each class the coefficients must sum to exactly 0, or the class
     doesn't telescope: NO_CLOSED_FORM. Shifts within a class are at most 12 apart.
   - A class telescopes to Σ over its fractions of c × Σ_{j<d} [1÷(hi+1+s0+j)^m − 1÷(lo+s0+j)^m], with s0 the class's
     lowest shift and d each fraction's distance from it.
   - **Poles:** a fraction's pole is var = −s. When −s is an integer in [lo, hi] (from lo up, for a symbolic or ∞ hi),
     the sum has no value: UNDEFINED. Σ 1÷(K×(K+1)) from 0 is UNDEFINED; from 1 it is 1 − 1÷(N+1).
6. **Named limits (NO_CLOSED_FORM), each with a visible example:**
   - **Classes that don't telescope:** Σ 1÷K (the harmonic numbers) and Σ 1÷K^2 (ζ(2) at ∞). These need functions
     casim doesn't have, or ζ(2m) = rational × π^(2m) at ∞ alone. That one is optional and for abacus to rule on; it's
     not in this build.
   - **A letter in a geometric ratio** (rule 4).
   - **Every other function of var** (SIN(K), LN(K)), unless rule 7 sums it.
7. **Integer bounds, summed term by term.** When rules 4 and 5 refuse, and both bounds are integers at most 64 terms
   apart, the terms are added exactly as canon makes them: Σ 1÷K from 1 to 10 is 7381÷2520, Σ SIN(K) from 1 to 3 is
   SIN(1)+SIN(2)+SIN(3). A term with no value is UNDEFINED, as in rule 5. Past 64 terms: NO_CLOSED_FORM.
8. **To ∞.** The sum is the limit of its partial sums, decided group by group, never by evaluating partial sums.
   - **A telescoping class** converges; its hi part goes to 0.
   - **A geometric group** converges when |B| < 1, decided by the evaluator beyond 1E-20 (const_sign, CAS 009 rule 5).
     B = EXP(−1) converges. A |B| it can't decide is UNDECIDED.
   - **A polynomial group** with any nonzero coefficient diverges, and so does a geometric group with |B| ≥ 1.
   - **When every group converges**, the answer is lo's part alone: Σ (1÷2)^K from 0 is 2, Σ K÷2^K from 1 is 2,
     Σ 1÷(K×(K+1)) from 1 is 1.
   - **When one diverges**, the dominant group decides: the largest |B|, then the highest degree.
     - B > 0 (a polynomial is B = 1): ∞ or −∞ by the sign of its leading coefficient (Σ K is ∞, Σ −2^K is −∞), as
       limit prints them.
     - B < 0: UNDEFINED, since the partial sums swing with growing size and have no limit (Σ (−2)^K, Σ (−1)^K×K).
     - Σ (−1)^K alone has partial sums 1, 0, 1, …: UNDEFINED, as B = −1 is a dominant group with B < 0.
     - A tie between B and −B, or a leading sign the evaluator can't decide (a letter, as in Σ A×K): UNDECIDED.
   - **For abacus to rule on:** a divergent sum as ∞ or −∞ (proposed, matching limit), or UNDEFINED for every
     divergent sum. limit's own NO_ROOM on −2^(N+1)+N×2^(N+1)+2 at N → ∞ is why this doesn't call limit.
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
      8,192 B. Past the arena: NO_ROOM, never an overrun.
    - **Flash:** the prototype is +13.5 KB of text on the host, with its main, a stdio trace and inlining still in it.
      The estimate is 8 to 10 KB on the S3, with the stop at 20. That leaves about 58 KB for complex roots and the
      series engine.
    - **Time:** the S3's worst on the bench.

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
